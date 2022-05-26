// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <vector>

struct CommonGameObject;
struct ServerGameObject;
struct ClientGameObject;
struct GSFileParser;
struct GameSet;
struct ScriptContext;
struct SrvScriptContext;
struct CliScriptContext;

// Contains the result of an object finder evalutation.
// Basically a std::vector of CommonGameObject* with a
// single element optimization.
struct ObjectFinderResult {
public:
	using value_type = CommonGameObject*;

	ObjectFinderResult() = default;
	ObjectFinderResult(CommonGameObject* obj) : _lone(obj) {}
	template <typename It> ObjectFinderResult(It first, It last) : _vec(first, last) {}

	CommonGameObject** begin() { return isSingle() ? &_lone : _vec.data(); }
	CommonGameObject** end() { return isSingle() ? (&_lone + 1) : (_vec.data() + _vec.size()); }
	CommonGameObject* const* begin() const { return isSingle() ? &_lone : _vec.data(); }
	CommonGameObject* const* end() const { return isSingle() ? (&_lone + 1) : (_vec.data() + _vec.size()); }
	bool empty() const { return !_lone && _vec.empty(); }
	size_t size() const { return isSingle() ? 1 : _vec.size(); }
	CommonGameObject*& operator[](size_t index) { return (isSingle() && index==0) ? _lone : _vec[index]; }
	CommonGameObject* const& operator[](size_t index) const { return (isSingle() && index==0) ? _lone : _vec[index]; }
	CommonGameObject*& front() { return isSingle() ? _lone : _vec.front(); }
	CommonGameObject* const& front() const { return isSingle() ? _lone : _vec.front(); }
	CommonGameObject*& back() { return isSingle() ? _lone : _vec.back(); }
	CommonGameObject* const& back() const { return isSingle() ? _lone : _vec.back(); }

	void clear() { _lone = nullptr; _vec.clear(); }

	void push_back(CommonGameObject* obj) {
		if (empty()) {
			_lone = obj;
		}
		else if (isSingle()) {
			_vec.clear();
			_vec.push_back(_lone);
			_vec.push_back(obj);
			_lone = nullptr;
		}
		else {
			_vec.push_back(obj);
		}
	}

	void pop_back() {
		if (_lone) {
			_lone = nullptr;
		}
		else {
			_vec.pop_back();
		}
	}

	void reserve(size_t capacity) {
		if (isSingle() && capacity > 1) {
			_vec.clear();
			_vec.push_back(_lone);
			_lone = nullptr;
		}
		_vec.reserve(capacity);
	}

	void resize(size_t size) {
		if (size == 0) {
			clear();
			return;
		}
		if (isSingle() && size > 1) {
			_vec.clear();
			_vec.push_back(_lone);
			_lone = nullptr;
		}
		_vec.resize(size);
	}
private:
	CommonGameObject* _lone = nullptr;
	std::vector<CommonGameObject*> _vec;
	bool isSingle() const { return _lone != nullptr; }
};

template<typename AnyGameObject> struct SpecificFinderResult : public ObjectFinderResult {
	using ObjectFinderResult::ObjectFinderResult;
	explicit SpecificFinderResult(ObjectFinderResult&& res) : ObjectFinderResult(res) {}
	AnyGameObject** begin(){ return (AnyGameObject**)ObjectFinderResult::begin(); }
	AnyGameObject** end() { return (AnyGameObject**)ObjectFinderResult::end(); }
	AnyGameObject* const* begin() const { return (AnyGameObject* const*)ObjectFinderResult::begin(); }
	AnyGameObject* const* end() const { return (AnyGameObject* const*)ObjectFinderResult::end(); }
	AnyGameObject*& operator[](size_t index) { return (AnyGameObject*&)ObjectFinderResult::operator[](index); }
	AnyGameObject* const& operator[](size_t index) const { return (AnyGameObject* const&)ObjectFinderResult::operator[](index); }
};

using SrvFinderResult = SpecificFinderResult<ServerGameObject>;
using CliFinderResult = SpecificFinderResult<ClientGameObject>;

struct ObjectFinder {
	using EvalRetSrv = SrvFinderResult;
	using EvalRetCli = CliFinderResult;
	virtual ~ObjectFinder() {}
	virtual ObjectFinderResult eval(ScriptContext* ctx) = 0;
	virtual void parse(GSFileParser &gsf, GameSet &gs) = 0;

	CommonGameObject* getFirst(ScriptContext* ctx) {
		auto objlist = eval(ctx);
		return objlist.empty() ? nullptr : objlist[0];
	}
	ObjectFinderResult fail(ScriptContext* ctx);

	SrvFinderResult eval(SrvScriptContext* ctx) {
		return SrvFinderResult(eval((ScriptContext*)ctx));
	}
	CliFinderResult eval(CliScriptContext* ctx) {
		return CliFinderResult(eval((ScriptContext*)ctx));
	}
	ServerGameObject* getFirst(SrvScriptContext* ctx) {
		return (ServerGameObject*)getFirst((ScriptContext*)ctx);
	}
	ClientGameObject* getFirst(CliScriptContext* ctx) {
		return (ClientGameObject*)getFirst((ScriptContext*)ctx);
	}
};

ObjectFinder *ReadFinder(GSFileParser &gsf, const GameSet &gs);
ObjectFinder *ReadFinderNode(GSFileParser &gsf, const GameSet &gs);
