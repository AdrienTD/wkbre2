#include "settings.h"
#include <nlohmann/json.hpp>
#include <fstream>

nlohmann::json g_settings;

void LoadSettings() {
	std::ifstream file("wkconfig.json");
	if (file.good())
		file >> g_settings;
}