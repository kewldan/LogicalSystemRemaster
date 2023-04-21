﻿#include "main.h"

Window* window;
HUD* hud;
BlockManager* blocks;
Camera* camera;
RenderPipeline* pipeline;
std::mutex mutex;
double tickTime;
bool simulate = true, focused = true, windowResized = false, saveMenu = false, loadMenu = false, bloom = true;
char mouseButtons;
int currentBlock = 0, currentRotation = 0, targetTps = 8;
char* saveFilename, * loadFilename;
ImGuiIO* io;
ImFontConfig font_cfg;
std::thread tickThread;

void mouse_button_callback(GLFWwindow* w, int button, int action, int mods)
{
	if (button >= 0 && button < 8) {
		mouseButtons ^= (-action ^ mouseButtons) & (1U << button);
	}
}

void key_callback(GLFWwindow* w, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS) {
		if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
			if (key == GLFW_KEY_0)
				currentBlock = 9;
			else
				currentBlock = key - GLFW_KEY_1;
			currentBlock += 10 * window->isKeyPressed(GLFW_KEY_LEFT_SHIFT);
			currentBlock = min(currentBlock, 14);
		}
		else if (key == GLFW_KEY_R) {
			int f = window->isKeyPressed(GLFW_KEY_LEFT_SHIFT) ? -1 : 1;
			currentRotation = (currentRotation + f % 4 < 0) ? (currentRotation + f % 4 + 4) : (currentRotation + f) % 4;
		}
		else if (key == GLFW_KEY_S && window->isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
			saveMenu = true;
		}
		else if (key == GLFW_KEY_O && window->isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
			loadMenu = true;
		}
		else if (key == GLFW_KEY_C && window->isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
			blocks->blocks.clear();
		}
	}
}

void tick() {
	static double lastUpdate = 0;
	static double elapsed = 0;
	while (!glfwWindowShouldClose(window->getId())) {
		if (glfwGetTime() > lastUpdate + (1. / targetTps) && simulate) {
			elapsed = glfwGetTime();
			mutex.lock();
			blocks->update();
			mutex.unlock();
			tickTime = glfwGetTime() - elapsed;
			lastUpdate = glfwGetTime();
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
}

int main()
{
	saveFilename = new char[64];
	loadFilename = new char[64];
	strcpy_s(saveFilename, 64, "");
	strcpy_s(loadFilename, 64, "");

	window = new Window(1280, 720, "Logical system");

	glfwSetKeyCallback(window->getId(), key_callback);
	glfwSetMouseButtonCallback(window->getId(), mouse_button_callback);

	hud = new HUD(window);
	camera = new Camera(window);
	pipeline = new RenderPipeline("block", "blur", "final", "background", window->width, window->height);
	io = &ImGui::GetIO();

	font_cfg.FontDataOwnedByAtlas = false;
	io->Fonts->AddFontFromFileTTF("./data/fonts/tahoma.ttf", 17.f, &font_cfg);
	ImGui::MergeIconsWithLatestFont(16.f, false);

	blocks = new BlockManager();

	tickThread = std::thread(tick);
	while (window->update(&windowResized)) {
		if (windowResized) {
			pipeline->resize(window->width, window->height);
		}
		int blockX = (int)floorf((window->getCursorPosition().x / (float)window->width * (window->width * 2.f * camera->zoom - window->width) + camera->position.x) / 32.f + 0.5f);
		int blockY = (int)floorf(((window->height - window->getCursorPosition().y) / (float)window->height * (window->height * 2.f * camera->zoom - window->height) + camera->position.y) / 32 + 0.5f);

		camera->update();

		window->reset();

		int drawCalls = 0;
		{
			Shader* shader = pipeline->beginPass(camera);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D_ARRAY, blocks->atlas);

			glBindVertexArray(blocks->VAO);

			int j = 0;
			int LB = (int)camera->position.x - 16;
			int RB = window->width + 16 + (int)camera->position.x;
			int BB = (int)camera->position.y - 16;
			int TB = (int)window->height + 16 + (int)camera->position.y;
			for (auto i = blocks->blocks.begin(); i != blocks->blocks.end(); ++i) {
				int x = Block::X(i->first) * 32;
				int y = Block::Y(i->first) * 32;
				if (x > LB && x < RB && y > BB && y < TB) {
					blocks->mvp[j] = i->second->getMVP(x, y);
					blocks->info[j] = glm::vec2(static_cast<float>(i->second->type->id), i->second->active ? 1.f : 0.f);
					j++;
				}

				if (j == BLOCK_BATCHING) {
					blocks->uploadBuffers(j);
					j = 0;
					drawCalls++;
				}
			}
			if (j > 0) {
				blocks->uploadBuffers(j);
				drawCalls++;
			}
			pipeline->endPass(16, bloom);
		}

		hud->begin();
		ImGui::SetNextWindowPos(ImVec2(15, 15), ImGuiCond_Once);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);

		static bool show_debugMenu = true;
		if (show_debugMenu) {
			ImGui::SetNextWindowPos(ImVec2(15, 15), ImGuiCond_Once);
			ImGui::SetNextWindowBgAlpha(0.35f);

			if (ImGui::Begin("##Debug overlay", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
				ImGuiWindowFlags_NoFocusOnAppearing |
				ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove)) {
				ImGui::Text("Logical System");
				ImGui::NewLine();

				ImGui::Text("FPS: %.1f", io->Framerate);
				ImGui::Text("Draw calls: %d", drawCalls);
				ImGui::Text("Tick: %.3fms", tickTime * 1000);
				ImGui::Text("Blocks: %ld", blocks->blocks.size());

				ImGui::NewLine();
				ImGui::Text("Position: X: %.1f, Y: %.1f", camera->position.x, camera->position.y);
				ImGui::Text("Zoom: %.1f", camera->zoom);
				ImGui::Text("Camera viewport: %dx%d", static_cast<int>(window->width * 2 * camera->zoom - window->width), static_cast<int>(window->height * 2 * camera->zoom - window->height));
				ImGui::Text("Viewport: %dx%d", window->width, window->height);

				ImGui::NewLine();
				ImGui::Text("Block position: %d %d", blockX, blockY);
				if (blocks->has(blockX, blockY)) {
					Block* block = blocks->get(blockX, blockY);
					ImGui::Text("Block: T%d A%d R%d", block->type->id, block->active, block->rotation);
				}
				else {
					ImGui::Text("Block: null");
				}
#ifndef NDEBUG
				ImGui::Text("Debug build: %s - %s", __DATE__, __TIME__);
#endif  
			}
			ImGui::End();
		}

		focused = false;
		ImGui::SetNextWindowPos(ImVec2(15, 400), ImGuiCond_Once);
		if (ImGui::Begin("Simulation", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_MenuBar)) {
			focused |= ImGui::IsWindowHovered();
			focused |= ImGui::IsWindowFocused();
			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu("File"))
				{
					if (ImGui::MenuItem("Open", "Ctrl + O"))
						loadMenu = true;
					if (ImGui::MenuItem("Save", "Ctrl + S"))
						saveMenu = true;
					if (ImGui::MenuItem("Clear", "Ctrl + C"))
						blocks->blocks.clear();
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Graphics")) {
					static bool vsync = false, _vsync = true;
					ImGui::MenuItem("VSync", 0, &_vsync);

					if (vsync != _vsync) {
						vsync = _vsync;
						window->setVsync(vsync);
					}

					ImGui::MenuItem("Bloom", 0, &bloom);
					ImGui::EndMenu();
				}
				ImGui::EndMenuBar();
			}
			ImGui::Checkbox("Play", &simulate);
			ImGui::SliderInt("TPS", &targetTps, 1, 64);
			ImGui::Combo("Block", &currentBlock, "Wire\0Wire right\0Wire left\0Wire side\0Wire all side\0Wire 2\0Wire 3\0Not\0And\0Nand\0Xor\0Nxor\0Button\0Timer\0Light");
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::Text("Use 0-9 for 0-9 elements\nand SHIFT for 10-14 elements");
				ImGui::EndTooltip();
			}
			ImGui::Combo("Rotation", &currentRotation, "Up\0Right\0Down\0Left");
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::Text("Use R for rotate clockwise\nand SHIFT for anti-clockwise");
				ImGui::EndTooltip();
			}
		}
		ImGui::End();
		if (saveMenu) {
			if (ImGui::Begin("Save menu", &saveMenu, ImGuiWindowFlags_AlwaysAutoResize)) {
				focused |= ImGui::IsWindowHovered();
				focused |= ImGui::IsWindowFocused();
				ImGui::InputText("Filename", saveFilename, 64, ImGuiInputTextFlags_NoHorizontalScroll);
				if (ImGui::Button("Save", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
					if (blocks->save(saveFilename)) {
						ImGuiToast toast(ImGuiToastType_Success, 3000);
						toast.set_title("%s saved successfully", const_cast<const char*>(saveFilename));
						ImGui::InsertNotification(toast);
					}
					else {
						ImGuiToast toast(ImGuiToastType_Error, 3000);
						toast.set_title("%s was not saved!", const_cast<const char*>(saveFilename));
						ImGui::InsertNotification(toast);
					}
					strcpy_s(saveFilename, 64, "");
					saveMenu = false;
				}
			}
			ImGui::End();
		}
		if (loadMenu) {
			if (ImGui::Begin("Open menu", &loadMenu, ImGuiWindowFlags_AlwaysAutoResize)) {
				focused |= ImGui::IsWindowHovered();
				focused |= ImGui::IsWindowFocused();
				ImGui::InputText("Filename", loadFilename, 64, ImGuiInputTextFlags_NoHorizontalScroll);
				if (ImGui::Button("Open", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
					if (blocks->load(loadFilename)) {
						ImGuiToast toast(ImGuiToastType_Success, 3000);
						toast.set_title("%s loaded successfully", const_cast<const char*>(loadFilename));
						ImGui::InsertNotification(toast);
					}
					else {
						ImGuiToast toast(ImGuiToastType_Error, 3000);
						toast.set_title("%s was not loaded!", const_cast<const char*>(loadFilename));
						ImGui::InsertNotification(toast);
					}
					strcpy_s(loadFilename, 64, "");
					loadMenu = false;
				}
			}
			ImGui::End();
		}
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(43.f / 255.f, 43.f / 255.f, 43.f / 255.f, 100.f / 255.f));
		ImGui::RenderNotifications();
		ImGui::PopStyleColor(1);
		ImGui::PopStyleVar(2);

		hud->end();


		if (!focused) {
			float delta = io->DeltaTime;
			float cameraZoomSq = camera->zoom * camera->zoom;
			float cameraSpeed = 500.f * cameraZoomSq * delta;
			if (window->isKeyPressed(GLFW_KEY_A)) {
				camera->position.x -= cameraSpeed;
			}
			else if (window->isKeyPressed(GLFW_KEY_D)) {
				camera->position.x += cameraSpeed;
			}

			if (window->isKeyPressed(GLFW_KEY_W)) {
				camera->position.y += cameraSpeed;
			}
			else if (window->isKeyPressed(GLFW_KEY_S)) {
				camera->position.y -= cameraSpeed;
			}
			static bool prev = false;
			if ((mouseButtons >> GLFW_MOUSE_BUTTON_LEFT) & 1U) {
				if (!blocks->has(blockX, blockY)) {
					blocks->set(blockX, blockY, new Block(&blocks->types[currentBlock], static_cast<BlockRotation>(currentRotation)));
				}
				else if (prev == false) {
					Block* block = blocks->get(blockX, blockY);
					if (block->type->id == blocks->types[12].id) {
						block->active ^= 1;
					}
					else {
						block->rotation = rotateBlock(block->rotation, window->isKeyPressed(GLFW_KEY_LEFT_SHIFT) ? -1.f : 1.f);
					}
				}
				prev = true;
			}
			else {
				prev = false;
			}
			if ((mouseButtons >> GLFW_MOUSE_BUTTON_RIGHT) & 1U) {
				blocks->erase(blockX, blockY);
			}
		}
	}
	tickThread.join();
	return 0;
}