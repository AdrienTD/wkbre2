#include "TimeManager.h"
#include "util/GSFileParser.h"
#include <SDL_timer.h>
#include "imgui/imgui.h"

void TimeManager::reset(game_time_t initialTime) {
	psCurrentTime = (uint32_t)(initialTime * 1000.0f);
	currentTime = previousTime = psCurrentTime / 1000.0f; elapsedTime = 0;
	timeSpeed = 1.0f;
	paused = true; lockCount = 1;
	previousSDLTime = SDL_GetTicks();
}

void TimeManager::load(GSFileParser &gsf) {
	gsf.advanceLine();
	while (!gsf.eof) {
		std::string tag = gsf.nextTag();
		if (tag == "CURRENT_TIME") {
			currentTime = gsf.nextFloat();
			psCurrentTime = (uint32_t)(currentTime * 1000.0f);
		}
		else if (tag == "PREVIOUS_TIME")
			previousTime = gsf.nextFloat();
		else if (tag == "ELAPSED_TIME")
			elapsedTime = gsf.nextFloat();
		else if (tag == "PAUSED")
			paused = gsf.nextInt();
		else if (tag == "LOCK_COUNT")
			lockCount = gsf.nextInt();
		else if (tag == "END_TIME_MANAGER_STATE")
			break;
		gsf.advanceLine();
	}
}

void TimeManager::tick() {
	uint32_t nextSDLTime = SDL_GetTicks();
	uint32_t elapsedSDLTime = nextSDLTime - previousSDLTime;
	previousSDLTime = nextSDLTime;
	if (paused) return;
	if (elapsedSDLTime >= 1000)
		elapsedSDLTime = 34;
	previousTime = currentTime;
	psCurrentTime += elapsedSDLTime;
	currentTime = (float)psCurrentTime / 1000.0f;
	elapsedTime = currentTime - previousTime;
}

void TimeManager::imgui() {
	ImGui::Begin("Time Manager");
	ImGui::Value("Current time", currentTime);
	ImGui::Value("Previous time", previousTime);
	ImGui::Value("Elapsed time", elapsedTime);
	ImGui::DragFloat("Speed", &timeSpeed);
	ImGui::InputScalar("Lock count", ImGuiDataType_U32, &lockCount);
	ImGui::Checkbox("Paused", &paused);
	ImGui::End();
}