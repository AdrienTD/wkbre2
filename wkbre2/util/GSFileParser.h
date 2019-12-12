#pragma once

#include <string>
#include <cstdlib>

struct GameSet;

struct GSFileParser
{
	//const char *file;
	const char *cursor;
	bool eof, eol;
	union {
		void *addptr;
		GameSet *gameSet;
	};

	bool isnewline(char c) const { return (c == '\n') || (c == '\r'); }

	// Move the cursor to the beginning of the next line.
	void advanceLine();

	// Return the next word as a string.
	std::string nextString(bool quoted = false);

	int nextInt() {
		return atoi(nextString().c_str());
	}

	float nextFloat() {
		return (float)atof(nextString().c_str());
	}

	std::string nextTag() {
		std::string tag = nextString();
		if (tag == "/*") {
			do advanceLine(); while ((tag = nextString()) != "*/" && !eof);
		}
		return tag;
	}

	GSFileParser(const char *text, void *addptr = nullptr) : /*file(text),*/ cursor(text), eof(false), eol(false), addptr(addptr) {}
};
