#include "settings.h"
#include <fstream>

nlohmann::json g_settings;

void LoadSettings() {
	std::ifstream file("wkconfig.json");
	if (file.good())
		file >> g_settings;
}