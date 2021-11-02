// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <memory>
#include <string>

struct Vector3;

struct SoundPlayer {
	virtual ~SoundPlayer() = default;
	virtual void init() = 0;
	virtual void deinit() = 0;
	virtual void playSound(const std::string& filePath) = 0;
	virtual void playSound3D(const std::string& filePath, const Vector3& pos, float refDist, float maxDist) = 0;
	virtual void setListenerPosition(const Vector3& pos, const Vector3& at) = 0;
	virtual void playMusic(const std::string& filePath) = 0;
	virtual bool isMusicPlaying() = 0;

	static std::unique_ptr<SoundPlayer> s_soundPlayer;
	static SoundPlayer* getSoundPlayer();
};