#include "ScriptContext.h"
#include "../server.h"
#include "../client.h"

template struct ScriptContext<Server, ServerGameObject>;
template struct ScriptContext<Client, ClientGameObject>;

SrvScriptContext::CtxElement SrvScriptContext::candidate;
SrvScriptContext::CtxElement SrvScriptContext::creator;
SrvScriptContext::CtxElement SrvScriptContext::packageSender;
SrvScriptContext::CtxElement SrvScriptContext::sequenceExecutor;

CliScriptContext::CtxElement CliScriptContext::candidate;
CliScriptContext::CtxElement CliScriptContext::creator;
CliScriptContext::CtxElement CliScriptContext::target;
