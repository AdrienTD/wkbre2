// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <nlohmann/json_fwd.hpp>

extern nlohmann::json g_settings;

bool LoadSettings();
