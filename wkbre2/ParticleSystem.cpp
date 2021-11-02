// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "ParticleSystem.h"
#include "util/GSFileParser.h"
#include "file.h"

void ParticleSystem::parse(PSystemParser& fp)
{
	while (!fp.eof) {
		auto tag = fp.nextTag();
		if (tag == "Particles")
			fp.readStruct(Particles);
		else if (tag == "Generation_Decision")
			fp.readStruct(Generation_Decision);
		else if (tag == "State_Generation")
			fp.readStruct(State_Generation);
		fp.advanceLine();
	}
}

void PS_State_Generation::parse(PSystemParser& fp)
{
	while (!fp.eof) {
		auto tag = fp.nextTag();
		if (tag == "Position")
			fp.readStruct(Position);
		else if (tag == "Velocity")
			fp.readStruct(Velocity);
		else if (tag == "}")
			break;
		fp.advanceLine();
	}

}

void PS_Velocity::parse(PSystemParser& fp)
{
	while (!fp.eof) {
		auto tag = fp.nextTag();
		if (tag == "Magnitude_Range")
			fp.readTo(Magnitude_Range);
		else if (tag == "Direction")
			fp.readStruct(Direction);
		else if (tag == "}")
			break;
		fp.advanceLine();
	}
}

void PS_Position_Type::parse(PSystemParser& fp)
{
	while (!fp.eof) {
		auto tag = fp.nextTag();
		if (tag == "Point")
			type = 1;
		else if (tag == "}")
			break;
		fp.advanceLine();
	}
}

void PS_Direction::parse(PSystemParser& fp)
{
	while (!fp.eof) {
		auto tag = fp.nextTag();
		if (tag == "Parent_Relative")
			Parent_Relative = true;
		else if (tag == "World_Relative")
			World_Relative = true;
		else if (tag == "Cone")
			Cone = fp.nextFloat();
		else if (tag == "}")
			break;
		fp.advanceLine();
	}
}

void PS_Generation_Decision::parse(PSystemParser& fp)
{
	while (!fp.eof) {
		auto tag = fp.nextTag();
		if (tag == "Detail_Level")
			Detail_Level = fp.nextInt();
		else if (tag == "Interval")
			Interval = fp.nextFloat();
		else if (tag == "}")
			break;
		fp.advanceLine();
	}
}

void PS_Particles::parse(PSystemParser& fp)
{
	while (!fp.eof) {
		auto tag = fp.nextTag();
		if (tag == "Max_Particles")
			Max_Particles = fp.nextInt();
		else if (tag == "Max_Age_Range")
			fp.readTo(Max_Age_Range);
		else if (tag == "Colour_Fn")
			fp.readTo(Colour_Fn);
		else if (tag == "Force") {
			Forces.emplace_back();
			Forces.back().parse(fp);
		}
		else if (tag == "Points")
			type = PSType::Points;
		else if (tag == "Streaks")
			type = PSType::Streaks;
		else if (tag == "Sprites") {
			type = PSType::Sprites;
			Sprites = fp.nextString(true);
		}
		else if (tag == "Mesh") {
			type = PSType::Mesh;
			Mesh = fp.nextString(true);
		}
		else if (tag == "Rotations_Per_Second")
			fp.readTo(Rotations_Per_Second);
		else if (tag == "Gradient")
			Scale_Gradient.parse(fp, "Scale");
		else if (tag == "}")
			break;
		fp.advanceLine();
	}
}

void PS_Force::parse(PSystemParser& fp)
{
	while (!fp.eof) {
		auto tag = fp.nextTag();
		if (tag == "Gravity") {
			direction = PSForceType::Gravity;
			intensity = fp.nextFloat();
		}
		else if (tag == "Wind") {
			direction = PSForceType::Wind;
			intensity = fp.nextFloat();
		}
		else if (tag == "}")
			break;
		fp.advanceLine();
	}

}

void PS_Colour_Fn::parse(PSystemParser& fp)
{
	while (!fp.eof) {
		auto tag = fp.nextTag();
		if (tag == "Static") {
			isDynamic = false;
			fp.readStruct(cstatic);
		}
		else if (tag == "Dynamic") {
			isDynamic = true;
			fp.readStruct(cdynamic);
		}
		else if (tag == "}")
			break;
		fp.advanceLine();
	}
}

void PS_Colour_Dynamic::parse(PSystemParser& fp)
{
	while (!fp.eof) {
		auto tag = fp.nextTag();
		if (tag == "Gradient")
			Gradient.parse(fp, "Colour");
		else if (tag == "Input_Age")
			Input_Age = true;
		else if (tag == "}")
			break;
		fp.advanceLine();
	}
}

void PS_Colour_Static::parse(PSystemParser& fp)
{
	while (!fp.eof) {
		auto tag = fp.nextTag();
		if (tag == "Colour")
			fp.readTo(colour);
		else if (tag == "}")
			break;
		fp.advanceLine();
	}
}

ParticleSystem* PSCache::getPS(const std::string& name) {
	auto it = cache.find(name);
	if (it != cache.end()) {
		return &it->second;
	}
	else {
		char* text; int tsize;
		std::string fname = folder + '/' + name + ".PSystem";
		if (!FileExists(fname.c_str()))
			fname = folder + "/Empty.PSystem";
		if (name == "Trail")
			fname = folder + "/Arrow_Trail.PSystem";
		LoadFile(fname.c_str(), &text, &tsize, 1);
		PSystemParser psp{ text };
		auto& ps = cache[name];
		ps.parse(psp);
		free(text);
		return &ps;
	}
}
