// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "settings.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <codecvt>
#include <locale>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

nlohmann::json g_settings;

bool LoadSettings() {
	std::ifstream file("wkconfig.json");
	if (file.good()) {
		try {
			file >> g_settings;
		}
		catch (const nlohmann::json::exception &exc) {
#ifdef _WIN32
			std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> cvt;
			std::wstring wstr = cvt.from_bytes(exc.what());
			MessageBoxW(nullptr, wstr.c_str(), L"wkconfig.json Parsing Error", 16);
#endif
            return false;
		}
	}
	else {
#ifdef _WIN32
		MessageBoxA(nullptr, "The configuration file wkconfig.json could not be opened.", "wkbre2", 16);
#endif
        return false;
	}

    return true;
}
