// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "settings.h"
#include <nlohmann/json.hpp>
#include <fstream>

nlohmann::json g_settings;

void LoadSettings() {
	std::ifstream file("wkconfig.json");
	if (file.good())
		file >> g_settings;
}