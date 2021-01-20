#include "ScriptContext.h"
#include "../server.h"
#include "../client.h"

CliScriptContext::CliScriptContext(Client* program, ClientGameObject* self) : ScriptContext<Client, ClientGameObject>(program, self) {}

SrvScriptContext::SrvScriptContext(Server* program, ServerGameObject* self) : ScriptContext<Server, ServerGameObject>(program, self) {}
