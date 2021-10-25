#pragma once

#include <array>
#include <map>
#include <string>
#include <vector>
#include "util/GSFileParser.h"

enum struct PSType {
	Points = 1,
	Streaks,
	Sprites,
	Mesh
};

enum struct PSForceType {
	Gravity = 1,
	Wind
};

struct PSystemParser : GSFileParser {
	template<typename S> void readStruct(S& s) {
		advanceLine();
		auto start = nextTag();
		if (start != "{")
			throw "Expected '{' missing";
		advanceLine();
		s.parse(*this);
	}
	void readTo(int& s) { s = nextInt(); }
	void readTo(float& s) { s = nextFloat(); }
	template<typename S> void readTo(S& s) { readStruct(s); }
	template<typename S, size_t N> void readTo(std::array<S, N>& s) {
		for (S& e : s)
			readTo(e);
	}
	using GSFileParser::GSFileParser;
};

template<typename T> struct PS_Gradient {
	std::vector<float> Time;
	std::vector<T> Value;

	void parse(PSystemParser& fp, const std::string& name) {
		while (!fp.eof) {
			auto tag = fp.nextTag();
			if (tag == "Time") {
				Time.push_back(fp.nextFloat());
				Value.emplace_back();
			}
			else if (tag == name) {
				fp.readTo(Value.back());
			}
			else if (tag == "}")
				break;
			fp.advanceLine();
		}
	}

	std::tuple<T, T, float> getAtTime(float t, const T& fallback) {
		if (Time.empty()) return { fallback, fallback, 0.0f };
		size_t i;
		for (i = 0; i < Time.size(); i++) {
			if (Time[i] > t)
				break;
		}
		if (i == 0) return { Value[0], Value[0], 0.0f };
		if (i == Time.size()) return { Value.back(), Value.back(), 0.0f };
		return { Value[i - 1], Value[i], (t - Time[i - 1]) / (Time[i] - Time[i - 1]) };
	}
};

struct PS_Colour_Static {
	std::array<int, 4> colour;
	void parse(PSystemParser& fp);
};

struct PS_Colour_Dynamic {
	PS_Gradient<std::array<int, 4>> Gradient;
	bool Input_Age;
	void parse(PSystemParser& fp);
};

struct PS_Colour_Fn {
	bool isDynamic = false;
	PS_Colour_Static cstatic;
	PS_Colour_Dynamic cdynamic;
	void parse(PSystemParser& fp);
};

struct PS_Force {
	PSForceType direction; // Gravity, Wind
	float intensity = 0.0f;

	void parse(PSystemParser& fp);
};

struct PS_Particles {
	int Max_Particles;
	std::array<float, 2> Max_Age_Range;
	PS_Colour_Fn Colour_Fn;
	std::vector<PS_Force> Forces;
	PSType type; // Points, Streaks, Sprites, Mesh
	std::string Sprites, Mesh;
	std::array<float, 2> Rotations_Per_Second;
	PS_Gradient<float> Scale_Gradient;

	void parse(PSystemParser& fp);
};

struct PS_Generation_Decision {
	int Detail_Level = 1;
	float Interval = 1.0f;

	void parse(PSystemParser& fp);
};

struct PS_Direction {
	bool Parent_Relative = false, World_Relative = false;
	float Cone = 90.0f;

	void parse(PSystemParser& fp);
};

struct PS_Position_Type {
	int type = 0;

	void parse(PSystemParser& fp);
};

struct PS_Velocity {
	std::array<float, 2> Magnitude_Range;
	PS_Direction Direction;

	void parse(PSystemParser& fp);
};

struct PS_State_Generation {
	PS_Position_Type Position;
	PS_Velocity Velocity;

	void parse(PSystemParser& fp);
};

struct ParticleSystem {
	PS_Particles Particles;
	PS_Generation_Decision Generation_Decision;
	PS_State_Generation State_Generation;

	void parse(PSystemParser& fp);
};

struct PSCache
{
	std::map<std::string, ParticleSystem> cache;
	std::string folder;
	void clear() { cache.clear(); }
	ParticleSystem* getPS(const std::string& name);
	PSCache(const std::string& folder) : folder(folder) {}
};
