// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "ScriptContext.h"
#include "../server.h"
#include "../client.h"

CliScriptContext::CliScriptContext(Client* program, ClientGameObject* self) : ScriptContext<Client, ClientGameObject>(program, self) {}

SrvScriptContext::SrvScriptContext(Server* program, ServerGameObject* self) : ScriptContext<Server, ServerGameObject>(program, self) {}
