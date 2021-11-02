// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "SoundPlayer.h"

#include "AL/al.h"
#include "AL/alc.h"

#include <cassert>
#include <map>
#include <string>
#include <vector>
#include <array>
#include <thread>
#include <atomic>

#include "WavDocument.h"
#include "file.h"
#include "util/BinaryReader.h"
#include "util/DynArray.h"

#include <mpg123.h>

#include "settings.h"
#include <nlohmann/json.hpp>

struct OALSoundPlayer : SoundPlayer {
	static constexpr size_t MAX_SOURCES = 64;

	bool initialised = false;
	ALCdevice* device;
	ALCcontext* context;

	std::map<std::string, ALuint> sndBufCache;
	std::array<ALuint, MAX_SOURCES> sources;
	ALuint musicSource; ALuint musicBuffer = 0;

	Vector3 listenerPosition{ 0.0f, 0.0f, 0.0f };

	char* streamBytes; int streamSize;
	char* streamPtr;
	mpg123_handle* mHandle;

	std::thread musicLoadingThread;
	std::atomic<bool> isMusicLoading{ false };

	std::pair<bool, ALuint> getSoundBuf(const std::string& filePath) {
		auto it = sndBufCache.find(filePath);
		ALuint buf;
		if (it != sndBufCache.end()) {
			buf = it->second;
		}
		else {
			char* wavdata; int wavsize;
			if (!FileExists(filePath.c_str()))
				return { false, 0 };
			LoadFile(filePath.c_str(), &wavdata, &wavsize);

			WavDocument wav;
			BinaryReader br(wavdata);
			wav.read(br);
			free(wavdata);

			std::vector<int16_t> samples;
			WavSampleReader wsr(&wav);
			while (wsr.available())
				samples.push_back((int16_t)(wsr.nextSample() * 32767.0f));

			alGenBuffers(1, &buf);
			alBufferData(buf, AL_FORMAT_MONO16, samples.data(), samples.size() * 2, wav.samplesPerSec);
			sndBufCache[filePath] = buf;
		}
		return { true, buf };
	}

	~OALSoundPlayer() {
		if (musicLoadingThread.joinable())
			musicLoadingThread.join();
		deinit();
	}

	void init() override {
		if (initialised) return;
		device = alcOpenDevice(nullptr);
		assert(device);
		context = alcCreateContext(device, nullptr);
		alcMakeContextCurrent(context);
		alGenSources(MAX_SOURCES, sources.data());
		alGenSources(1, &musicSource);
		initialised = true;
	}
	void deinit() override {
		if (!initialised) return;
		alDeleteSources(sources.size(), sources.data());
		alDeleteSources(1, &musicSource);
		for (auto& e : sndBufCache) {
			alDeleteBuffers(1, &e.second);
		}
		sndBufCache.clear();
		if (musicBuffer)
			alDeleteBuffers(1, &musicBuffer);
		alcDestroyContext(context);
		alcCloseDevice(device);
		initialised = false;
	}

	void playSound(const std::string& filePath) override {
		bool fnd;  ALuint buf;
		std::tie(fnd, buf) = getSoundBuf(filePath);
		if (!fnd) {
			printf("WARNING: Sound \"%s\" not found\n", filePath.c_str());
			return;
		}

		size_t i = 0;
		for (; i < MAX_SOURCES; i++) {
			ALint val;
			alGetSourcei(sources[i], AL_SOURCE_STATE, &val);
			if (val != AL_PLAYING) {
				auto src = sources[i];
				alSourcei(src, AL_BUFFER, buf);
				alSourcei(src, AL_MIN_GAIN, 1.0f);
				alSourcei(src, AL_SOURCE_RELATIVE, AL_TRUE);
				alSource3f(src, AL_POSITION, 0.0f, 0.0f, 0.0f);
				alSourcePlay(src);
				break;
			}
		}
	}

	void playSound3D(const std::string& filePath, const Vector3& pos, float refDist, float maxDist) override {
		if ((pos - listenerPosition).sqlen3() > maxDist * maxDist)
			return;

		bool fnd;  ALuint buf;
		std::tie(fnd, buf) = getSoundBuf(filePath);
		if (!fnd) {
			printf("WARNING: Sound \"%s\" not found\n", filePath.c_str());
			return;
		}

		size_t i = 0;
		for (; i < MAX_SOURCES; i++) {
			ALint val;
			alGetSourcei(sources[i], AL_SOURCE_STATE, &val);
			if (val != AL_PLAYING) {
				auto src = sources[i];
				alSourcei(src, AL_BUFFER, buf);
				alSourcei(src, AL_MIN_GAIN, 0.0f);
				alSourcei(src, AL_SOURCE_RELATIVE, AL_FALSE);
				alSourcef(src, AL_REFERENCE_DISTANCE, refDist);
				//alSourcef(src, AL_MAX_DISTANCE, maxDist);
				Vector3 fpos = pos * Vector3(1, 1, -1);
				alSourcefv(src, AL_POSITION, &fpos.x);
				alSourcePlay(src);
				break;
			}
		}
	}

	virtual void setListenerPosition(const Vector3& pos, const Vector3& at) override {
		Vector3 fpos = pos * Vector3(1, 1, -1);
		alListenerfv(AL_POSITION, &fpos.x);
		float orient[6] = { at.x, at.y, -at.z, 0.0f, 1.0f, 0.0f };
		alListenerfv(AL_ORIENTATION, orient);
		listenerPosition = pos;
	}

	void playMusicImp(const std::string& filePath) {
		if (!FileExists(filePath.c_str()))
			return;
		LoadFile(filePath.c_str(), &streamBytes, &streamSize);
		streamPtr = streamBytes;

		mpg123_init();
		mHandle = mpg123_new(nullptr, nullptr);
		mpg123_replace_reader_handle(mHandle,
			[](void* file, void* out, size_t len) -> ssize_t {
				OALSoundPlayer* self = (OALSoundPlayer*)file;
				size_t clen = len;
				if (self->streamPtr - self->streamBytes + len > self->streamSize)
					clen = self->streamSize - (self->streamPtr - self->streamBytes);
				memcpy(out, self->streamPtr, clen);
				self->streamPtr += clen;
				return clen;
			},
			[](void* file, off_t off, int mode) -> off_t {
				OALSoundPlayer* self = (OALSoundPlayer*)file;
				if (mode == SEEK_SET) self->streamPtr = self->streamBytes + off;
				else if (mode == SEEK_CUR) self->streamPtr += off;
				else if (mode == SEEK_END) self->streamPtr = self->streamBytes + self->streamSize + off;
				return self->streamPtr - self->streamBytes;
			},
			nullptr
		);
		
		// allowed formats
		const long* supRates; size_t numSupRates;
		mpg123_rates(&supRates, &numSupRates);
		mpg123_format_none(mHandle);
		for (size_t i = 0; i < numSupRates; i++)
			mpg123_format(mHandle, supRates[i], MPG123_MONO | MPG123_STEREO, MPG123_ENC_SIGNED_16);

		mpg123_open_handle(mHandle, this);
		long mRate; int mChannels, mEncoding;
		mpg123_getformat(mHandle, &mRate, &mChannels, &mEncoding);
		mpg123_format_none(mHandle);
		mpg123_format(mHandle, mRate, mChannels, mEncoding);

		char* music; int numSamples;
		numSamples = mpg123_length(mHandle);
		int16_t* samples = new int16_t[numSamples * mChannels];
		size_t rread = 0;
		mpg123_read(mHandle, (unsigned char*)samples, numSamples * mChannels * 2, &rread);

		alSourceStop(musicSource);
		alSourcei(musicSource, AL_BUFFER, 0);

		if (musicBuffer) {
			alDeleteBuffers(1, &musicBuffer);
		}
		alGenBuffers(1, &musicBuffer);
		alBufferData(musicBuffer, (mChannels == 2) ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16, samples, numSamples * mChannels * 2, mRate);
		delete [] samples;

		alSourcei(musicSource, AL_BUFFER, musicBuffer);
		alSourcei(musicSource, AL_SOURCE_RELATIVE, AL_TRUE);
		alSource3f(musicSource, AL_POSITION, 0.0f, 0.0f, 0.0f);
		alSourcePlay(musicSource);
	}

	virtual void playMusic(const std::string& filePath) override {
		if (!g_settings.value<bool>("musicEnabled", true))
			return;
		if (musicLoadingThread.joinable())
			musicLoadingThread.join();
		isMusicLoading = true;
		musicLoadingThread = std::thread([this,filePath]() {
			playMusicImp(filePath);
			isMusicLoading = false;
		});
	}

	virtual bool isMusicPlaying() override {
		ALint val;
		alGetSourcei(musicSource, AL_SOURCE_STATE, &val);
		return val == AL_PLAYING || isMusicLoading;
	}
};

std::unique_ptr<SoundPlayer> SoundPlayer::s_soundPlayer = nullptr;
SoundPlayer* SoundPlayer::getSoundPlayer() {
	if (!s_soundPlayer) {
		s_soundPlayer = std::make_unique<OALSoundPlayer>();
		s_soundPlayer->init();
	}
	return s_soundPlayer.get();
}