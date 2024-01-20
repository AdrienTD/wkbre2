#pragma once

#include <cstdint>
#include <string>

struct GSFileParser;

struct GSFileLocation {
public:
	void set(GSFileParser& gsf);
	int getLine() const { return _lineIndex; }
	std::string asString() const;

private:
	using FileIndex = std::uint16_t;
	using LineIndex = std::uint16_t;
	FileIndex _fileIndex = 0;
	LineIndex _lineIndex = 0;

	static FileIndex registerFile(const std::string& filePath);
};