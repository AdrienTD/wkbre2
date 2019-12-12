#pragma once

template <class T> class DynArray {
private:
	T* pointer;
	size_t length;

	void free() {
		if(length)
			delete[] pointer;
		pointer = nullptr;
		length = 0;
	}

	void setsize(size_t newlen) {
		if(newlen)
			pointer = new T[newlen];
		length = newlen;
	}

public:
	void resize(size_t newlen) {
		free();
		setsize(newlen);
	}

	size_t size() const { return length; }

	T* begin() { return pointer; }
	T* end() { return pointer + length; }
	const T* begin() const { return pointer; }
	const T* end() const { return pointer + length; }

	T& operator[] (size_t index) { return pointer[index]; }
	//const T& operator[] const (size_t index) { return pointer[index]; }

	DynArray() { pointer = nullptr; length = 0; }
	DynArray(int len) { setsize(len); }
};