// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "ScriptContext.h"
#include "../server.h"
#include "../client.h"

// (not anymore, almost) Empty, might need to be removed

bool ScriptContext::isServer() const { return gameState->isServer(); }
bool ScriptContext::isClient() const { return gameState->isClient(); }
