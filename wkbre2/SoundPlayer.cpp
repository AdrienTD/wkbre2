#include "SoundPlayer.h"

#include "AL/al.h"
#include "AL/alc.h"

#include <cassert>
#include <map>
#include <string>
#include <vector>
#include <array>

#include "WavDocument.h"
#include "file.h"
#include "util/BinaryReader.h"

struct OALSoundPlayer : SoundPlayer {
	static constexpr size_t MAX_SOURCES = 32;

	ALCdevice* device;
	ALCcontext* context;

	std::map<std::string, ALuint> sndBufCache;
	std::array<ALuint, MAX_SOURCES> sources;

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

	void init() override {
		device = alcOpenDevice(nullptr);
		assert(device);
		context = alcCreateContext(device, nullptr);
		alcMakeContextCurrent(context);
		alGenSources(MAX_SOURCES, sources.data());
	}
	void deinit() override {
		alDeleteSources(sources.size(), sources.data());
		for (auto& e : sndBufCache) {
			alDeleteBuffers(1, &e.second);
		}
		sndBufCache.clear();
		alcDestroyContext(context);
		alcCloseDevice(device);
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
				//alSourcef(src, AL_REFERENCE_DISTANCE, refDist);
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
	}
};

SoundPlayer* SoundPlayer::s_soundPlayer = nullptr;
SoundPlayer* SoundPlayer::getSoundPlayer() {
	if (!s_soundPlayer) {
		s_soundPlayer = new OALSoundPlayer;
		s_soundPlayer->init();
	}
	return s_soundPlayer;
}