#include "HUD.h"

Engine::HUD::HUD(Engine::Window* window) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window->getId(), true);
	ImGui_ImplOpenGL3_Init("#version 330");
	ImGui::GetIO().IniFilename = NULL;
}

void Engine::HUD::begin() {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.f);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
}

void Engine::HUD::end() {
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(43.f / 255.f, 43.f / 255.f, 43.f / 255.f, 100.f / 255.f));
	ImGui::RenderNotifications();
	ImGui::PopStyleColor(1);
	ImGui::PopStyleVar(2);
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

Engine::HUD::~HUD() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
}
