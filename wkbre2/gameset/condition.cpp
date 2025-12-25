// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "condition.h"
#include "gameset.h"
#include "util/GSFileParser.h"
#include "../Language.h"

void GSCondition::parse(GSFileParser & gsf, GameSet & gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		auto tag = gsf.nextTag();
		if (tag == "TEST") {
			test = ReadValueDeterminer(gsf, gs);
		}
		else if (tag == "HINT") {
			hint = gsf.nextString();
			while (!gsf.eol) {
				hintValues.push_back(ReadValueDeterminer(gsf, gs));
			}
		}
		else if (tag == "END_CONDITION")
			break;
		gsf.advanceLine();
	}
}

std::string GSCondition::getFormattedHint(const std::string& hint, const std::vector<ValueDeterminer*>& hintValues, const Language& lang, ScriptContext* ctx)
{
	std::string langHint = lang.getText(hint);
	std::string result;
	result.reserve(langHint.size() + hintValues.size() * 8);
	size_t valIndex = 0;
	for (size_t i = 0; i < langHint.size(); ++i) {
		char c = langHint[i];
		if (c == '%') {
			switch (langHint[i + 1]) {
			case '%': result.push_back('%'); break;
			case 'd': result.append(std::to_string((int)hintValues.at(valIndex++)->eval(ctx))); break;
			case 0: return result;
			}
			++i;
		}
		else if (c == '\\') {
			switch (langHint[i + 1]) {
			case '\\': result.push_back('\\'); break;
			case 'n': result.push_back('\n'); break;
			case 0: return result;
			}
			++i;
		}
		else {
			result.push_back(c);
		}
	}
	return result;
}

std::string GSCondition::getFormattedHint(const Language& lang, ScriptContext* ctx) const
{
	return getFormattedHint(hint, hintValues, lang, ctx);
}

