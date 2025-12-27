// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "settings.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <codecvt>
#include <locale>
#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

nlohmann::json g_settings;

static const std::filesystem::path configPath = "wkconfig.json";

static void ResetDefaultSettings()
{
	g_settings["gamePath"] = ".";
	g_settings["gameVersion"] = 0;
	g_settings["musicEnabled"] = true;
#ifdef _WIN32
	g_settings["renderer"] = "d3d11";
	g_settings["msaaNumSamples"] = 1;
	g_settings["d3d11AdapterIndex"] = 0;
	g_settings["enhancedGraphics"] = true;
#endif
}

static void WriteSettings()
{
	std::ofstream file(configPath);
	if (file.good()) {
		file << g_settings.dump(4);
	}
}

void LoadSettings() {

	if (!std::filesystem::exists(configPath)) {
		ResetDefaultSettings();
		WriteSettings();
		return;
	}

	std::ifstream file(configPath);
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
		}
	}
	else {
#ifdef _WIN32
		MessageBoxA(nullptr, "The configuration file wkconfig.json could not be opened.", "wkbre2", 16);
#endif
	}
}