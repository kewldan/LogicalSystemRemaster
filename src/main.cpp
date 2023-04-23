#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define GLFW_INCLUDE_NONE
#include "main.h"

Engine::Window* window;
Engine::HUD* hud;
Engine::Camera* camera;

BlockManager* blocks;
RenderPipeline* pipeline;
Blocks selected;

ImGuiIO* io;
ImFontConfig font_cfg;

bool focused = true, windowResized = false, saveMenu = false, loadMenu = false, bloom = true;
char mouseButtons;
int currentBlock = 0, currentRotation = 0;
char* saveFilename;
int blockX, blockY;
glm::vec2 offsets;
ImGui::FileBrowser saveDialog(ImGuiFileBrowserFlags_SelectDirectory | ImGuiFileBrowserFlags_CreateNewDir), loadDialog;

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
		else if (window->isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
			if (key == GLFW_KEY_S) {
				saveMenu = true;
			}
			else if (key == GLFW_KEY_O) {
				loadMenu = true;
			}
			else if (key == GLFW_KEY_C) {
				offsets = glm::vec2(blockX, blockY);
			}
			else if (key == GLFW_KEY_V) {
				for (auto i = selected.begin(); i != selected.end(); ++i) {
					blocks->blocks[Block::TO_LONG(Block::X(i->first) + blockX - offsets.x, Block::Y(i->first) + blockY - offsets.y)] = new Block(*i->second);
				}
			}
		}
	}
}

int main()
{
	saveFilename = new char[128];
	strcpy_s(saveFilename, 128, "");

	window = new Engine::Window(1280, 720, "Logical system");
	window->setIcon("./data/icons/icon.png");

	glfwSetKeyCallback(window->getId(), key_callback);
	glfwSetMouseButtonCallback(window->getId(), mouse_button_callback);

	PLOGI << "<< LOADING ASSETS >>";

	hud = new Engine::HUD(window);
	camera = new Engine::Camera(window);
	pipeline = new RenderPipeline("block", "blur", "final", "background", "selection", window->width, window->height);
	io = &ImGui::GetIO();

	font_cfg.FontDataOwnedByAtlas = false;
	io->Fonts->AddFontFromFileTTF("./data/fonts/tahoma.ttf", 17.f, &font_cfg);
	ImGui::MergeIconsWithLatestFont(16.f, false);

	saveDialog.SetTitle("Save scheme");
	loadDialog.SetTitle("Load scheme");
	loadDialog.SetTypeFilters({ ".ls" });

	blocks = new BlockManager(window, quadVertices, (int)sizeof(quadVertices));
	PLOGI << "<< STARING GAME LOOP >>";
	while (window->update(&windowResized)) {
		camera->update();

		if (windowResized) {
			pipeline->resize(window->width, window->height);
			camera->updateOrthographic();
		}
		blockX = (int)floorf((window->getCursorPosition().x / (float)window->width * (window->width * 2.f * camera->zoom - window->width) + camera->position.x) / 32.f + 0.5f);
		blockY = (int)floorf(((window->height - window->getCursorPosition().y) / (float)window->height * (window->height * 2.f * camera->zoom - window->height) + camera->position.y) / 32 + 0.5f);



		window->reset();

		{
			Engine::Shader* shader = pipeline->beginPass(camera);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D_ARRAY, blocks->atlas);

			glBindVertexArray(blocks->VAO);

			int j = 0;
			int LB = (int)camera->position.x - 16;
			int RB = window->width + 16 + (int)camera->position.x;
			int BB = (int)camera->position.y - 16;
			int TB = window->height + 16 + (int)camera->position.y;
			for (auto i = blocks->blocks.begin(); i != blocks->blocks.end(); ++i) {
				int x = Block::X(i->first) << 5;
				int y = Block::Y(i->first) << 5;
				if (x > LB && x < RB && y > BB && y < TB) {
					blocks->mvp[j] = i->second->getMVP(x, y);
					blocks->info[j] = glm::vec2(static_cast<float>(i->second->type->id), i->second->active ? 1.f : 0.f);
					j++;
				}

				if (j == BLOCK_BATCHING) {
					blocks->uploadBuffers(j);
					j = 0;
				}
			}
			if (j > 0) {
				blocks->uploadBuffers(j);
			}

			static bool f = false;
			static ImVec2 start, delta;
			if (window->isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
				if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
					start = io->MouseClickedPos[ImGuiMouseButton_Left];
					delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);

					int x = min(start.x, start.x + delta.x);
					int y = min(window->height - start.y, window->height - start.y - delta.y);

					pipeline->drawSelection(camera, glm::vec2(x, y), glm::abs(glm::vec2(delta.x, delta.y)));
				}
				else if (f) {
					selected.clear();

					for (auto i = blocks->blocks.begin(); i != blocks->blocks.end(); ++i) {
						int x = Block::X(i->first);
						int y = Block::Y(i->first);
						int px = x << 5;
						int py = y << 5;
						if (px > start.x - 16 && px < start.x + delta.x + 16 && py > start.y - 16 && py < start.y + delta.y + 16) {
							selected[i->first] = i->second;
						}
					}

					ImGuiToast toast(ImGuiToastType_Info, 2000);
					toast.set_title("Selected %ld blocks", selected.size());
					ImGui::InsertNotification(toast);
				}
			}
			f = ImGui::IsMouseDragging(ImGuiMouseButton_Left);

			pipeline->endPass(12, bloom);
		}

		hud->begin();
		ImGui::SetNextWindowPos(ImVec2(15, 15), ImGuiCond_Once);
		static bool show_debugMenu = true;
		if (show_debugMenu) {
			ImGui::SetNextWindowPos(ImVec2(15, 15), ImGuiCond_Once);
			ImGui::SetNextWindowBgAlpha(0.35f);

			if (ImGui::Begin("##Debug overlay", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
				ImGuiWindowFlags_NoFocusOnAppearing |
				ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove)) {
				ImGui::Text("FPS: %.1f", io->Framerate);
				ImGui::Text("Tick: %.3fms", blocks->tickTime * 1000.);
				ImGui::Text("Blocks: %ld", blocks->blocks.size());
			}
			ImGui::End();
		}

		focused = false;
		ImGui::SetNextWindowPos(ImVec2(15, 200), ImGuiCond_Once);
		if (ImGui::Begin("Simulation", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_MenuBar)) {
			focused |= ImGui::IsWindowHovered();
			focused |= ImGui::IsWindowFocused();
			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu("File"))
				{
					if (ImGui::MenuItem("Open", "Ctrl + O"))
						loadDialog.Open();
					if (ImGui::MenuItem("Save", "Ctrl + S"))
						saveMenu = true;
					if (ImGui::MenuItem("Clear"))
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
			ImGui::Checkbox("Play", &blocks->simulate);
			ImGui::SliderInt("TPS", &blocks->TPS, 1, 64);
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

		loadDialog.Display();
		focused |= loadDialog.IsOpened();
		if (loadDialog.HasSelected()) {
			auto p = loadDialog.GetSelected();
			std::string s = p.string();
			const char* path = s.c_str();
			if (blocks->load(path)) {
				ImGuiToast toast(ImGuiToastType_Success, 3000);
				toast.set_title("%s loaded successfully", path);
				ImGui::InsertNotification(toast);
			}
			else {
				ImGuiToast toast(ImGuiToastType_Error, 3000);
				toast.set_title("%s was not loaded!", path);
				ImGui::InsertNotification(toast);
			}
			loadDialog.ClearSelected();
		}


		saveDialog.Display();
		focused |= saveDialog.IsOpened();
		if (saveDialog.HasSelected()) {
			auto p = loadDialog.GetSelected();
			std::string s = p.string();
			const char* path = s.c_str();
			static char* buf = new char[128];
			strcpy_s(buf, 128, path);
			strcat_s(buf, 128, "\\");
			strcat_s(buf, 128, saveFilename);
			strcat_s(buf, 128, ".ls");
			if (blocks->save(buf)) {
				ImGuiToast toast(ImGuiToastType_Success, 3000);
				toast.set_title("%s saved successfully", const_cast<const char*>(buf));
				ImGui::InsertNotification(toast);
			}
			else {
				ImGuiToast toast(ImGuiToastType_Error, 3000);
				toast.set_title("%s was not saved!", const_cast<const char*>(buf));
				ImGui::InsertNotification(toast);
			}
			saveDialog.ClearSelected();
		}
		if (saveMenu) {
			if (ImGui::Begin("Enter scheme name", &saveMenu, ImGuiWindowFlags_AlwaysAutoResize)) {
				focused |= ImGui::IsWindowHovered();
				focused |= ImGui::IsWindowFocused();
				ImGui::InputText("Filename", saveFilename, 64, ImGuiInputTextFlags_NoHorizontalScroll);
				if (ImGui::Button("Save", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
					saveDialog.Open();
					saveMenu = false;
				}
			}
			ImGui::End();
		}
		hud->end();

		if (!focused) {
			float cameraSpeed = 500.f * camera->zoom * camera->zoom * io->DeltaTime;
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
			if (window->isKeyPressed(GLFW_KEY_A) || window->isKeyPressed(GLFW_KEY_D) || window->isKeyPressed(GLFW_KEY_W) || window->isKeyPressed(GLFW_KEY_S)) {
				camera->updateView();
			}
			static bool prev = false;
			if ((mouseButtons >> GLFW_MOUSE_BUTTON_LEFT) & 1U && !window->isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
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
	blocks->shouldStop = true;
	blocks->thread.join();
	return 0;
}
