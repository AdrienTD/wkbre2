// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

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
	int linenum = 1;

	std::string fileName = "somewhere"; // used to indicate file locations in gameset errors

	static bool isnewline(char c) { return (c == '\n') || (c == '\r'); }
	static bool iswhitespace(char c) { return isspace((unsigned char)c); }

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
			//do advanceLine(); while ((tag = nextString()) != "*/" && !eof);
			while (!eof) {
				if (cursor[0] == '*' && cursor[1] == '/') {
					tag = nextString();
					break;
				}
				else if (isnewline(cursor[0]))
					advanceLine();
				else if (cursor[0] == 0)
					return {};
				else
					++cursor;
			}
		}
		return tag;
	}

	std::string locate() const {
		return fileName + " (line " + std::to_string(linenum) + ')';
	}

	GSFileParser(const char *text, void *addptr = nullptr) : /*file(text),*/ cursor(text), eof(false), eol(false), addptr(addptr) {}
};
