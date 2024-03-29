// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <cstddef>
#include <cstring>
#include <type_traits>

template <class T> class DynArray {
private:
	T* pointer;
	size_t length;

	void freeP() {
		if (length)
			delete[] pointer;
		pointer = nullptr;
		length = 0;
	}

	void setsize(size_t newlen) {
		if (newlen)
			pointer = new T[newlen];
		length = newlen;
	}

	void copyFrom(const DynArray& other) {
		if constexpr (std::is_trivially_copyable<T>::value) {
			memcpy(pointer, other.pointer, length * sizeof(T));
		}
		else {
			for (size_t i = 0; i < length; ++i)
				pointer[i] = other.pointer[i];
		}
	}

public:
	void resize(size_t newlen) {
		freeP();
		setsize(newlen);
	}

	size_t size() const { return length; }
	T* data() { return pointer; }
	const T* data() const { return pointer; }

	T* begin() { return pointer; }
	T* end() { return pointer + length; }
	const T* begin() const { return pointer; }
	const T* end() const { return pointer + length; }

	T& operator[] (size_t index) { return pointer[index]; }
	const T& operator[] (size_t index) const { return pointer[index]; }

	DynArray() { pointer = nullptr; length = 0; }
	DynArray(int len) { setsize(len); }
	DynArray(const DynArray& other) { setsize(other.length); copyFrom(other); }
	DynArray(DynArray &&other) noexcept { pointer = other.pointer; length = other.length; other.pointer = nullptr; other.length = 0; }
	void operator=(const DynArray &other) { resize(other.length); copyFrom(other); }
	void operator=(DynArray &&other) noexcept { freeP(); pointer = other.pointer; length = other.length; other.pointer = nullptr; other.length = 0; }
	~DynArray() { freeP(); }
};