#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <tuple>
#include "vecmat.h"

struct BinaryReader
{
	const char *pnt;
	inline uint8_t readUint8() { uint8_t a = *(const uint8_t*)pnt; pnt++; return a; }
	inline uint16_t readUint16() { uint16_t a = *(const uint16_t*)pnt; pnt += 2; return a; }
	inline uint32_t readUint32() { uint32_t a = *(const uint32_t*)pnt; pnt += 4; return a; }
	inline float readFloat() { float a = *(const float*)pnt; pnt += 4; return a; }
	inline std::string readString(int numChars) {
		std::string s(pnt, numChars);
		pnt += numChars;
		return s;
	}
	inline std::string readStringZ() {
		size_t len = strlen(pnt);
		std::string s(pnt, len);
		pnt += len + 1;
		return s;
	}
	inline Vector3 readVector3() {
		const float *a = (const float*)pnt;
		Vector3 v(a[0], a[1], a[2]);
		pnt += 12;
		return v;
	}
	inline void seek(int offset) { pnt += offset; }
	BinaryReader(void* ptr) : pnt((const char*)ptr) {}
	BinaryReader(const char* ptr) : pnt(ptr) {}

	inline void readTo(uint8_t& val) { val = readUint8(); }
	inline void readTo(uint16_t& val) { val = readUint16(); }
	inline void readTo(uint32_t& val) { val = readUint32(); }
	inline void readTo(int32_t& val) { val = (int32_t)readUint32(); }
	inline void readTo(float& val) { val = readFloat(); }
	inline void readTo(std::string& val) { val = readStringZ(); }
	inline void readTo(Vector3& val) { val = readVector3(); }
	template<typename T, typename ... Args> void readTo(T& val, Args& ... rest) { readTo(val); readTo(rest...); }
	template<typename T> std::tuple<T> readValues() { T val; readTo(val); return { val }; }
	template<typename T, typename ... Args> std::tuple<T, Args...> readValues() {
		T val;
		readTo(val);
		return std::tuple_cat(std::tie(val), readValues<Args...>());
	}
};
