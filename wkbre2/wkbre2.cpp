// wkbre2.cpp : définit le point d'entrée de l'application.
//

#include "wkbre2.h"
#include <cstdio>
#include "settings.h"
#include "file.h"
#include <nlohmann/json.hpp>

void LaunchTest();

int main()
{
	printf("Hello! :)\n");

	LoadSettings();
	strcpy_s(gamedir, g_settings["game_path"].get<std::string>().c_str());

	LaunchTest();
	
	return 0;
}
