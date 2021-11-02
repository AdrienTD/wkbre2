// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "Language.h"
#include "file.h"
#include "util/GSFileParser.h"

void Language::load(const char* path)
{
	char* out; int outsize;
	LoadFile(path, &out, &outsize, 1);
	out[outsize] = 0;
	GSFileParser gsf(out);
	while (!gsf.eof) {
		auto tag = gsf.nextTag();
		auto str = gsf.nextString(true);
		text[tag] = str;
		gsf.advanceLine();
	}
	free(out);
}

std::string Language::getText(const std::string& key)
{
	auto it = text.find(key);
	if (it != text.end())
		return it->second;
	else
		return key;
}
