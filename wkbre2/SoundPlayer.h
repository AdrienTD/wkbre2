#pragma once

#include <string>

struct Vector3;

struct SoundPlayer {
	virtual ~SoundPlayer() = default;
	virtual void init() = 0;
	virtual void deinit() = 0;
	virtual void playSound(const std::string& filePath) = 0;
	virtual void playSound3D(const std::string& filePath, const Vector3& pos, float refDist, float maxDist) = 0;
	virtual void setListenerPosition(const Vector3& pos, const Vector3& at) = 0;

	static SoundPlayer* s_soundPlayer;
	static SoundPlayer* getSoundPlayer();
};