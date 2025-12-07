// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "Language.h"
#include "file.h"
#include "util/GSFileParser.h"

namespace
{
	std::string convertLatin1ToUtf8(const std::string& str)
	{
		std::string u8str;
		u8str.reserve(str.size());
		for (char ch : str) {
			unsigned char code = (unsigned char)ch;
			if (code & 0x80) {
				u8str.push_back((code >> 6) | 0b1100'0000);
				u8str.push_back((code & 0b0011'1111) | 0b1000'0000);
			}
			else u8str.push_back(code);
		}
		return u8str;
	}
}

void Language::load(const char* path)
{
	char* out; int outsize;
	LoadFile(path, &out, &outsize, 1);
	out[outsize] = 0;
	GSFileParser gsf(out);
	while (!gsf.eof) {
		auto tag = gsf.nextTag();
		auto str = gsf.nextString(true);
		text[tag] = convertLatin1ToUtf8(str);
		gsf.advanceLine();
	}
	free(out);
}

std::string Language::getText(const std::string& key) const
{
	auto it = text.find(key);
	if (it != text.end())
		return it->second;
	else
		return key;
}
