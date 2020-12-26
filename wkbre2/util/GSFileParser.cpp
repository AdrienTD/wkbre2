#include "GSFileParser.h"

// Move the cursor to the beginning of the next line.
void GSFileParser::advanceLine() {
	const char *c = cursor;
	while (*c && !isnewline(*c)) c++;
	if (!*c) eof = true;
	else if ((c[0] == '\r') && (c[1] == '\n')) c += 2;
	else c++;
	cursor = c;
	eol = false;
	++linenum;
}

// Return the next word as a string.
std::string GSFileParser::nextString(bool quoted) {
	const char *c = cursor;

	// Find beginning of next word
	while (*c && !isnewline(*c) && isspace(*c)) c++; // 0 is not a space
	if (quoted) {
		if (*c == '\"')
			c++;
		else
			quoted = false;
	}
	const char *beg = c;

	// Find end of word
	const char *end;
	if (quoted) {
		while (*c && !isnewline(*c) && (*c != '\"')) c++;
		end = c;
		if (*c == '\"') c++;
	}
	else {
		while (*c && !isspace(*c)) c++;
		end = c;
	}

	// Skip the spaces after the word, to check if it is the last word in the line.
	while (*c && !isnewline(*c) && isspace(*c)) c++; // 0 is not a space

	eof = *c == 0;
	eol = isnewline(*c) || eof;
	cursor = c;
	return std::string(beg, end - beg);
}