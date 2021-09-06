#pragma once

#include <nlohmann/json_fwd.hpp>

extern nlohmann::json g_settings;

void LoadSettings();