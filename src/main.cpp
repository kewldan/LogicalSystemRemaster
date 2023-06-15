#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define GLFW_INCLUDE_NONE

#include "Window.h"
#include "HUD.h"
#include "BlockManager.h"
#include "Camera2D.h"
#include "RenderPipeline.h"
#include "Input.h"
#include <format>

ImGuiIO *io;
ImFontConfig font_cfg;

bool saveMenu, vsync = true, displayFps = false, displayMenu = true;
ImGui::FileBrowser saveDialog(ImGuiFileBrowserFlags_SelectDirectory | ImGuiFileBrowserFlags_CreateNewDir), loadDialog;

float map(float value, float max1, float min2, float max2) {
    return min2 + value * (max2 - min2) / max1;
}

int main() {
    Engine::Window::init();
    auto window = new Engine::Window(1280, 720, "Logical system");
    window->setIcon("data/textures/favicon.png");
    auto input = new Engine::Input(window->getId());
    auto camera = new Engine::Camera2D(window);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    input->registerCallbacks();

    Engine::HUD::init(window);
    Engine::HUD::show_command_palette = false;
    auto pipeline = new RenderPipeline(
            new Engine::Shader("block"),
            new Engine::Shader("blur"),
            new Engine::Shader("final"),
            new Engine::Shader("background"),
            new Engine::Shader("quad"),
            window->width,
            window->height
    );
    io = &ImGui::GetIO();

    auto saveFilename = new char[128];
    saveFilename[0] = 0;

    font_cfg.FontDataOwnedByAtlas = false;
    int size = 0;
    void *fontData = Engine::Filesystem::readResourceFile("data/fonts/comfortaa.ttf", &size);
    io->Fonts->AddFontFromMemoryTTF(fontData, size, 16.f, &font_cfg);
    ImGui::MergeIconsWithLatestFont(16.f, false);

    saveDialog.SetTitle("Save scheme");
    loadDialog.SetTitle("Load scheme");
    loadDialog.SetTypeFilters({".ls", ".bson"});

    auto blocks = new BlockManager(window, quadVertices, (int) sizeof(quadVertices));

    int blockX, blockY;

    {
        ImCmd::Command stress_test_cmd;
        stress_test_cmd.Name = "Spawn blocks";
        stress_test_cmd.InitialCallback = []() {
            ImCmd::Prompt(std::vector<std::string>{
                    "128x128",
                    "256x256",
                    "512x512",
                    "1024x1024",
            });
        };
        stress_test_cmd.SubsequentCallback = [&blocks](int s) {
            int dim;
            switch (s) {
                case 0:
                    dim = 128;
                    break;
                case 1:
                    dim = 256;
                    break;
                case 2:
                    dim = 512;
                    break;
                case 3:
                    dim = 1024;
                    break;
                default:
                    dim = 0;
                    break;
            }
            for (int x = 0; x < dim; x++) {
                for (int y = 0; y < dim; y++) {
                    blocks->set(x, y);
                }
            }
        };
        ImCmd::AddCommand(std::move(stress_test_cmd));

        ImCmd::Command fps_mon_command;
        fps_mon_command.Name = "Display FPS";
        fps_mon_command.InitialCallback = []() {
            displayFps ^= 1;
        };
        ImCmd::AddCommand(std::move(fps_mon_command));

        ImCmd::Command display_menu_command;
        display_menu_command.Name = "Show menu";
        display_menu_command.InitialCallback = []() {
            displayMenu ^= 1;
        };
        ImCmd::AddCommand(std::move(display_menu_command));

        ImCmd::Command block_info_command;
        block_info_command.Name = "Show block info";
        block_info_command.InitialCallback = [&blocks, &blockX, &blockY]() {
            ImGuiToast toast(ImGuiToastType_Info, 7000);
            Block *block = blocks->get(blockX, blockY);
            if (block) {
                toast.set_title("%p - %d blocks\nBlock %dx%d\nType - %d\nData - %.2X", &blocks->blocks,
                                blocks->length(), blockX, blockY, block->type->id, 217);
            } else {
                toast.set_title("%p - %d blocks\nBlock %dx%d", &blocks->blocks, blocks->length(), blockX, blockY);
            }
            ImGui::InsertNotification(toast);
        };
        ImCmd::AddCommand(std::move(block_info_command));
    }

    do {
        if (window->isResized()) {
            pipeline->resize(window->width, window->height);
        }

        input->update();
        camera->update();
        window->reset();
        window->setVsync(vsync);

        float cursorX = map(input->getCursorPosition().x, (float) window->width, camera->left, camera->right);
        float cursorY = map(input->getCursorPosition().y, (float) window->height, camera->bottom, camera->top);
        blockX = (int) floorf((cursorX + camera->position.x) / 32.f +
                              0.5f);
        blockY = (int) floorf((((float) window->height - cursorY) + camera->position.y) / 32.f +
                              0.5f);

        pipeline->beginPass(camera, blocks->atlas, blocks->VAO, [&blocks, &camera]() { blocks->draw(camera); });

        if (!io->WantCaptureKeyboard) {
            for (int i = 0; i <= 10; i++) {
                if (input->isKeyJustPressed(GLFW_KEY_0 + i)) {
                    blocks->playerInput.currentBlock = !i ? 9 : i - 1;
                    blocks->playerInput.currentBlock += 10 * input->isKeyPressed(GLFW_KEY_LEFT_SHIFT);
                    blocks->playerInput.currentBlock = std::min(blocks->playerInput.currentBlock, 14);
                    break;
                }
            }
            if (input->isKeyJustPressed(GLFW_KEY_R)) {
                blocks->playerInput.currentRotation = rotateBlock(blocks->playerInput.currentRotation,
                                                                  input->isKeyPressed(GLFW_KEY_LEFT_SHIFT) ? -1 : 1);
            }
            if (input->isKeyJustPressed(GLFW_KEY_DELETE)) {
                blocks->delete_selected();
            }
            if (input->isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
                if (input->isKeyJustPressed(GLFW_KEY_S)) {
                    saveMenu = true;
                }
                if (input->isKeyJustPressed(GLFW_KEY_O)) {
                    loadDialog.Open();
                }
                if (input->isKeyJustPressed(GLFW_KEY_X)) {
                    blocks->cut(blockX, blockY);
                }
                if (input->isKeyJustPressed(GLFW_KEY_C)) {
                    blocks->copy(blockX, blockY);
                }
                if (input->isKeyJustPressed(GLFW_KEY_V)) {
                    blocks->paste(blockX, blockY);
                }
                if (input->isKeyJustPressed(GLFW_KEY_A)) {
                    blocks->select_all();
                }
                if (input->isKeyJustPressed(GLFW_KEY_N)) {
                    blocks->blocks.clear();
                }
                if (input->isKeyPressed(GLFW_KEY_LEFT_SHIFT)) {
                    if (input->isKeyJustPressed(GLFW_KEY_P)) {
                        Engine::HUD::show_command_palette ^= 1;
                    }
                }
            } else {
                float cameraSpeed = 500.f * camera->getZoom() * camera->getZoom() * io->DeltaTime;
                if (input->isKeyPressed(GLFW_KEY_A)) {
                    camera->position.x -= cameraSpeed;
                }
                if (input->isKeyPressed(GLFW_KEY_D)) {
                    camera->position.x += cameraSpeed;
                }

                if (input->isKeyPressed(GLFW_KEY_W)) {
                    camera->position.y += cameraSpeed;
                }
                if (input->isKeyPressed(GLFW_KEY_S)) {
                    camera->position.y -= cameraSpeed;
                }
            }

            if (!io->WantCaptureMouse) {
                if (input->getMouseWheelDelta().y != 0.f) {
                    camera->zoomIn(input->getMouseWheelDelta().y * -0.1f);
                }
                if (!input->isKeyPressed(GLFW_KEY_LEFT_SHIFT)) {
                    Block *block = blocks->get(blockX, blockY);
                    if (block) {
                        if (input->isMouseButtonJustPressed(GLFW_MOUSE_BUTTON_LEFT)) {
                            if (block->type->id == blocks->types[12].id) {
                                block->active ^= 1;
                            } else {
                                blocks->rotate(blockX, blockY, rotateBlock(block->rotation,
                                                                           input->isKeyPressed(GLFW_KEY_LEFT_SHIFT)
                                                                           ? -1.f
                                                                           : 1.f));
                            }
                        }
                    } else if (input->isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
                        if (!blocks->has(blockX, blockY)) {
                            blocks->set(blockX, blockY);
                        }
                    }
                    if (input->isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
                        blocks->erase(blockX, blockY);
                    }
                }
            }
        }

        static glm::vec2 cameraStart, cameraDelta, size, start, delta;
        if (input->isStartDragging()) {
            cameraStart = camera->position;
        }
        if (input->isDragging()) {
            start = input->getDraggingStartPosition();
            start.x = map(start.x, (float) window->width, camera->left, camera->right);
            start.y = map(start.y, (float) window->height, camera->bottom, camera->top);
            delta = input->getCursorPosition() - input->getDraggingStartPosition();
            delta.x *= (camera->right - camera->left) / (float) window->width;
            delta.y *= (camera->top - camera->bottom) / (float) window->height;
            cameraDelta = glm::vec2(camera->position.x, camera->position.y) - cameraStart;

            size = glm::vec2(delta.x + cameraDelta.x, delta.y - cameraDelta.y);

            int s_x = (int) std::min(start.x, start.x + size.x);
            int s_y = (int) std::max(start.y, start.y + size.y);

            pipeline->drawSelection(camera, glm::vec2(s_x, window->height - s_y) - cameraDelta, glm::abs(size));
        }
        if (input->isStopDragging()) {
            int s_LB = (int) (std::min(start.x, start.x + size.x) + camera->position.x - cameraDelta.x);
            int s_BB = (int) ((float) window->height - std::max(start.y, start.y + size.y) +
                              camera->position.y -
                              cameraDelta.y);

            int s_RB = s_LB + (int) abs(size.x);
            int s_TB = s_BB + (int) abs(size.y);
            blocks->selectedBlocks = 0;
            for (auto &it: blocks->blocks) {
                int px = Block_X(it.first) << 5;
                int py = Block_Y(it.first) << 5;
                it.second->selected = px > s_LB && px < s_RB && py > s_BB && py < s_TB;
                blocks->selectedBlocks += it.second->selected;
            }

            ImGuiToast toast(ImGuiToastType_Info, 2000);
            toast.set_title("Selected %d blocks", blocks->selectedBlocks);
            ImGui::InsertNotification(toast);
        }

        Engine::HUD::begin();
        if (displayFps) {
            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
            ImGui::SetNextWindowBgAlpha(0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
            if (ImGui::Begin("##Debug", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                                 ImGuiWindowFlags_NoFocusOnAppearing |
                                                 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove)) {
                ImGui::Text("FPS: %.0f", io->Framerate);
                ImGui::Text("Tick: %.1fms", blocks->tickTime);
            }
            ImGui::End();
            ImGui::PopStyleVar();
        }
        if (displayMenu) {
            ImGui::SetNextWindowPos(ImVec2(5, 50), ImGuiCond_Once);
            if (ImGui::Begin("Simulation", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_MenuBar)) {
                if (ImGui::BeginMenuBar()) {
                    if (ImGui::BeginMenu("File")) {
                        if (ImGui::MenuItem("\xef\x85\x9b  New", "Ctrl + N"))
                            blocks->blocks.clear();
                        if (ImGui::MenuItem("\xef\x81\xbc Open", "Ctrl + O"))
                            loadDialog.Open();
                        if (ImGui::MenuItem("\xef\x83\x87  Save", "Ctrl + S"))
                            saveMenu = true;
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Edit")) {
                        if (ImGui::MenuItem("\xef\x83\x85 Copy", "Ctrl + C"))
                            blocks->copy(blockX, blockY);
                        if (ImGui::MenuItem("\xef\x83\xaa Paste", "Ctrl + V"))
                            blocks->paste(blockX, blockY);
                        if (ImGui::MenuItem("\xef\x83\x84 Cut", "Ctrl + X"))
                            blocks->cut(blockX, blockY);
                        if (ImGui::MenuItem("\xef\xa1\x8c Select all", "Ctrl + A"))
                            blocks->select_all();
                        if (ImGui::MenuItem("\xef\x87\xb8 Delete", "DELETE"))
                            blocks->delete_selected();
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Examples")) {
                        if (ImGui::MenuItem("\xef\x81\xbc Blocks overview"))
                            blocks->load_example(camera, "data/examples/Blocks.ls");
                        if (ImGui::MenuItem("\xef\x81\xbc 1 Byte RAM"))
                            blocks->load_example(camera, "data/examples/RAM1Byte.ls");
                        if (ImGui::MenuItem("\xef\x81\xbc 4 Bit adder"))
                            blocks->load_example(camera, "data/examples/Adder4Bit.ls");
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Graphics")) {
                        ImGui::MenuItem("VSync", nullptr, &vsync);
                        ImGui::MenuItem("Bloom", nullptr, &pipeline->bloom);
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Help")) {
                        if (ImGui::MenuItem("Console", "Ctrl + Shift + P"))
                            Engine::HUD::show_command_palette ^= 1;
                        if (ImGui::MenuItem("Itch.io"))
                            ShellExecute(nullptr, nullptr, "https://kewldan.itch.io/logical-system", nullptr, nullptr,
                                         SW_SHOW);
                        if (ImGui::MenuItem("Source code"))
                            ShellExecute(nullptr, nullptr, "https://github.com/kewldan/LogicalSystemRemaster", nullptr,
                                         nullptr, SW_SHOW);
                        static const std::string versionString = std::format("Version: 2.0.7 ({})", __DATE__);
                        static const char *version = versionString.c_str();
                        ImGui::MenuItem(version, nullptr, nullptr, false);
                        ImGui::MenuItem("Author: kewldan", nullptr, nullptr, false);
                        ImGui::EndMenu();
                    }
                    ImGui::EndMenuBar();
                }
                ImGui::Checkbox("Play", &blocks->simulate);
                if (!blocks->simulate) {
                    ImGui::SameLine();
                    if (ImGui::Button("Tick")) {
                        blocks->update();
                    }
                }
                ImGui::SliderInt("TPS", &blocks->TPS, 2, 256);
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Simulation ticks per second");
                    ImGui::EndTooltip();
                }
                ImGui::Combo("Block", &blocks->playerInput.currentBlock,
                             "Wire straight\0Wire angled right\0Wire angled left\0Wire T\0Wire cross\0Wire 2\0Wire 3\0NOT\0AND\0NAND\0XOR\0NXOR\0Switch\0Clock\0Lamp\0");
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Use 0-9 for 0-9 elements\nand SHIFT for 10-14 elements");
                    ImGui::EndTooltip();
                }
                ImGui::Combo("Rotation", &blocks->playerInput.currentRotation, "Up\0Right\0Down\0Left\0");
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Use R for rotate clockwise\nand SHIFT for anti-clockwise");
                    ImGui::EndTooltip();
                }
            }

            ImGui::End();
        }

        loadDialog.Display();
        if (loadDialog.HasSelected()) {
            std::string s = loadDialog.GetSelected().string();
            const char *path = s.c_str();
            ImGuiToast toast(0);
            if (blocks->load(camera, path)) {
                toast.set_type(ImGuiToastType_Success);
                toast.set_title("%s loaded successfully", path);
            } else {
                toast.set_type(ImGuiToastType_Error);
                toast.set_title("%s was not loaded!", path);
            }
            ImGui::InsertNotification(toast);
            loadDialog.ClearSelected();
        }

        saveDialog.Display();
        if (saveDialog.HasSelected()) {
            std::string s = saveDialog.GetSelected().string();
            const char *path = s.c_str();
            static char *buf = new char[128];
            strcpy_s(buf, 128, path);
            strcat_s(buf, 128, "\\");
            strcat_s(buf, 128, saveFilename);
            strcat_s(buf, 128, ".bson");
            ImGuiToast toast(0);
            if (blocks->save(camera, buf)) {
                toast.set_type(ImGuiToastType_Success);
                toast.set_title("%s saved successfully", const_cast<const char *>(buf));
            } else {
                toast.set_type(ImGuiToastType_Error);
                toast.set_title("%s was not saved!", const_cast<const char *>(buf));
            }
            ImGui::InsertNotification(toast);
            saveDialog.ClearSelected();
        }
        if (saveMenu) {
            if (ImGui::Begin("Enter scheme name", &saveMenu, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::InputText("Filename", saveFilename, 64, ImGuiInputTextFlags_NoHorizontalScroll);
                if (ImGui::Button("\xef\x83\x87 Save", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
                    saveDialog.Open();
                    saveMenu = false;
                }
            }

            ImGui::End();
        }
        Engine::HUD::end();
    } while (window->update());
    blocks->thread.join();
    return 0;
}
