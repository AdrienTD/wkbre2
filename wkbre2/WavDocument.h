// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <cstdint>
#include "util/DynArray.h"

struct File;
struct BinaryReader;

struct WavDocument {
	uint16_t formatTag;
	uint16_t numChannels;
	uint32_t samplesPerSec;
	uint32_t avgBytesPerSec;
	uint16_t blockAlign;
	uint16_t pcmBitsPerSample;
	DynArray<uint8_t> data;

	//void read(File* file);
	void read(BinaryReader& br);
	//void write(File* file);

	size_t getNumSamples() { return data.size() / blockAlign; }
};

struct WavSampleReader {
	WavDocument* _wav;
	uint8_t* _pnt;

	WavSampleReader(WavDocument* wav) { _wav = wav; _pnt = wav->data.data(); }

	float nextSample();
	bool available();
};