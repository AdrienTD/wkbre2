#pragma once

struct ServerGameObject;
struct ClientGameObject;

template<typename D, typename T> struct CommonEval : T {
	virtual typename T::EvalRetSrv eval(ServerGameObject *self) override { return ((D*)this)->common_eval(self); }
	virtual typename T::EvalRetCli eval(ClientGameObject *self) override { return ((D*)this)->common_eval(self); }
};
