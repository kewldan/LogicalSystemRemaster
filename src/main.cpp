#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define GLFW_INCLUDE_NONE

#include "main.h"

Engine::Window *window;
Engine::Camera *camera;

BlockManager *blocks;
RenderPipeline *pipeline;

ImGuiIO *io;
ImFontConfig font_cfg;

bool focused = true, windowResized = false, saveMenu = false, bloom = true, controlsMenu = false;
char mouseButtons = 0;
int currentBlock = 0, currentRotation = 0;
char *saveFilename = nullptr;
int blockX, blockY;
glm::vec2 offsets;
ImGui::FileBrowser saveDialog(ImGuiFileBrowserFlags_SelectDirectory | ImGuiFileBrowserFlags_CreateNewDir), loadDialog;

void load_example(const char *path) {
    ImGuiToast toast(0);
    if (blocks->load(path)) {
        toast.set_type(ImGuiToastType_Success);
        toast.set_title("%s loaded successfully", path);
    } else {
        toast.set_type(ImGuiToastType_Error);
        toast.set_title("%s was not loaded!", path);
    }
    ImGui::InsertNotification(toast);
}

void mouse_button_callback(GLFWwindow *w, int button, int action, int mods) {
    if (button >= 0 && button < 8) {
        mouseButtons ^= (-action ^ mouseButtons) & (1U << button);
    }
}

void key_callback(GLFWwindow *w, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
            if (key == GLFW_KEY_0)
                currentBlock = 9;
            else
                currentBlock = key - GLFW_KEY_1;
            currentBlock += 10 * window->isKeyPressed(GLFW_KEY_LEFT_SHIFT);
            currentBlock = min(currentBlock, 14);
        } else if (key == GLFW_KEY_R) {
            int f = window->isKeyPressed(GLFW_KEY_LEFT_SHIFT) ? -1 : 1;
            currentRotation = (currentRotation + f % 4 < 0) ? (currentRotation + f % 4 + 4) : (currentRotation + f) % 4;
        } else if (window->isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
            if (key == GLFW_KEY_S) {
                saveMenu = true;
            } else if (key == GLFW_KEY_O) {
                loadDialog.Open();
            } else if (key == GLFW_KEY_C) {
                offsets = glm::vec2(blockX, blockY);
            } else if (key == GLFW_KEY_V) {
                int bx, by;
                for (auto i = blocks->blocks.begin(); i != blocks->blocks.end(); ++i) {
                    if (i->second->selected) {
                        bx = Block_X(i->first) + blockX - offsets.x;
                        by = Block_Y(i->first) + blockY - offsets.y;
                        blocks->set(bx, by, new Block(bx, by, i->second->type, i->second->rotation));
                    }
                }
            }
        }
    }
}

int main() {
    saveFilename = new char[128];
    strcpy_s(saveFilename, 128, "");

    Engine::Window::init();
    window = new Engine::Window(1280, 720, "Logical system");
    window->setIcon("./data/icons/icon.png");

    glfwSetKeyCallback(window->getId(), key_callback);
    glfwSetMouseButtonCallback(window->getId(), mouse_button_callback);

    PLOGI << "<< LOADING ASSETS >>";

    Engine::HUD::init(window);
    camera = new Engine::Camera(window);
    pipeline = new RenderPipeline("block", "blur", "final", "background", "selection", window->width, window->height);
    io = &ImGui::GetIO();

    font_cfg.FontDataOwnedByAtlas = false;
    io->Fonts->AddFontFromFileTTF("./data/fonts/tahoma.ttf", 18.f, &font_cfg);
    ImGui::MergeIconsWithLatestFont(16.f, false);

    saveDialog.SetTitle("Save scheme");
    loadDialog.SetTitle("Load scheme");
    loadDialog.SetTypeFilters({".ls"});

    blocks = new BlockManager(window, quadVertices, (int) sizeof(quadVertices));
    PLOGI << "<< STARING GAME LOOP >>";
    while (window->update(&windowResized)) {
        camera->update();

        if (windowResized) {
            pipeline->resize(window->width, window->height);
            camera->updateOrthographic();
        }
        blockX = (int) floorf((window->getCursorPosition().x / (float) window->width *
                               (window->width * 2.f * camera->zoom - window->width) + camera->position.x) / 32.f +
                              0.5f);
        blockY = (int) floorf(((window->height - window->getCursorPosition().y) / (float) window->height *
                               (window->height * 2.f * camera->zoom - window->height) + camera->position.y) / 32 +
                              0.5f);
        window->reset();

        pipeline->beginPass(camera, bloom, blocks->atlas, blocks->VAO, [](Engine::Shader *shader) {
            int j = 0, x, y;
            int LB = (int) camera->position.x - 16;
            int RB = window->width + 16 + (int) camera->position.x;
            int BB = (int) camera->position.y - 16;
            int TB = window->height + 16 + (int) camera->position.y;
            for (auto i = blocks->blocks.begin(); i != blocks->blocks.end(); ++i) {
                x = Block_X(i->first) << 5;
                y = Block_Y(i->first) << 5;
                if (x > LB && x < RB && y > BB && y < TB) {
                    blocks->info[j] = BlockInfo(i->second->type->id, i->second->active, i->second->selected,
                                                i->second->getMVP());
                    j++;
                }
                if (j == BLOCK_BATCHING) {
                    blocks->draw(j);
                    j = 0;
                }
            }
            if (j > 0) {
                blocks->draw(j);
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
                } else if (f) {
                    int LB = min(start.x, start.x + delta.x) + camera->position.x;
                    int BB = min(window->height - start.y, window->height - start.y - delta.y) + camera->position.y;

                    int RB = LB + abs(delta.x);
                    int TB = BB + abs(delta.y);
                    int count = 0;
                    for (auto i = blocks->blocks.begin(); i != blocks->blocks.end(); ++i) {
                        int px = Block_X(i->first) << 5;
                        int py = Block_Y(i->first) << 5;
                        i->second->selected = px > LB && px < RB && py > BB && py < TB;
                        count += i->second->selected;
                    }

                    ImGuiToast toast(ImGuiToastType_Info, 2000);
                    toast.set_title("Selected %d blocks", count);
                    ImGui::InsertNotification(toast);
                }
            }
            f = ImGui::IsMouseDragging(ImGuiMouseButton_Left);
        });

        Engine::HUD::begin();
        ImGui::SetNextWindowPos(ImVec2(15, 15), ImGuiCond_Once);
        ImGui::SetNextWindowBgAlpha(0.4f);
        if (ImGui::Begin("##Debug", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                          ImGuiWindowFlags_NoFocusOnAppearing |
                                          ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove)) {
            ImGui::Text("FPS: %.1f", io->Framerate);
            ImGui::Text("Tick: %.3fms", blocks->tickTime * 1000.);
            ImGui::Text("Blocks: %zu", blocks->blocks.size());
        }
        ImGui::End();
        focused = false;
        ImGui::SetNextWindowPos(ImVec2(15, 200), ImGuiCond_Once);
        if (ImGui::Begin("Simulation", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_MenuBar)) {
            focused |= ImGui::IsWindowHovered();
            focused |= ImGui::IsWindowFocused();
            if (ImGui::BeginMenuBar()) {
                if (ImGui::BeginMenu("File")) {
                    if (ImGui::MenuItem("\xef\x85\x9b  New"))
                        blocks->blocks.clear();
                    if (ImGui::MenuItem("\xef\x81\xbc Open", "Ctrl + O"))
                        loadDialog.Open();
                    if (ImGui::MenuItem("\xef\x83\x87  Save", "Ctrl + S"))
                        saveMenu = true;
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Examples")) {
                    if (ImGui::MenuItem("\xef\x81\xbc Wires overview")) load_example("./data/examples/wires.ls");
                    if (ImGui::MenuItem("\xef\x81\xbc Blocks overview")) load_example("./data/examples/blocks.ls");
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Graphics")) {
                    static bool vsync = false, _vsync = true;
                    ImGui::MenuItem("VSync", nullptr, &_vsync);

                    if (vsync != _vsync) {
                        vsync = _vsync;
                        Engine::Window::setVsync(vsync);
                    }

                    ImGui::MenuItem("Bloom", nullptr, &bloom);
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Help")) {
                    if (ImGui::MenuItem("Controls guide"))
                        controlsMenu = true;
                    if (ImGui::MenuItem("Itch.io"))
                        ShellExecute(nullptr, nullptr, "https://kewldan.itch.io/logical-system", nullptr, nullptr, SW_SHOW);
                    if (ImGui::MenuItem("Source code"))
                        ShellExecute(nullptr, nullptr, "https://github.com/kewldan/LogicalSystemRemaster", nullptr, nullptr, SW_SHOW);
                    ImGui::MenuItem(std::format("Version: 1.0.7 ({})", __DATE__).c_str(), nullptr, nullptr, false);
                    ImGui::MenuItem("Author: kewldan", nullptr, nullptr, false);
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }
            ImGui::Checkbox("Play", &blocks->simulate);
            ImGui::SliderInt("TPS", &blocks->TPS, 1, 128);
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Simulation ticks per second");
                ImGui::EndTooltip();
            }
            ImGui::Combo("Block", &currentBlock,
                         "Wire\0Wire right\0Wire left\0Wire side\0Wire all side\0Wire 2\0Wire 3\0Not\0And\0Nand\0Xor\0Nxor\0Button\0Timer\0Light");
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
            const char *path = s.c_str();
            ImGuiToast toast(0);
            if (blocks->load(path)) {
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
        focused |= saveDialog.IsOpened();
        if (saveDialog.HasSelected()) {
            auto p = saveDialog.GetSelected();
            std::string s = p.string();
            const char *path = s.c_str();
            static char *buf = new char[128];
            strcpy_s(buf, 128, path);
            strcat_s(buf, 128, "\\");
            strcat_s(buf, 128, saveFilename);
            strcat_s(buf, 128, ".ls");
            ImGuiToast toast(0);
            if (blocks->save(buf)) {
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
                focused |= ImGui::IsWindowHovered();
                focused |= ImGui::IsWindowFocused();
                ImGui::InputText("Filename", saveFilename, 64, ImGuiInputTextFlags_NoHorizontalScroll);
                if (ImGui::Button("\xef\x83\x87 Save", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
                    saveDialog.Open();
                    saveMenu = false;
                }
            }

            ImGui::End();
        }
        if (controlsMenu) {
            ImGui::SetNextWindowPos(ImVec2(io->DisplaySize.x * 0.5f, io->DisplaySize.y * 0.5f), ImGuiCond_Always,
                                    ImVec2(0.5f, 0.5f));
            if (ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                focused |= ImGui::IsWindowHovered();
                focused |= ImGui::IsWindowFocused();
                if (ImGui::CollapsingHeader("\xef\xa3\x8c  Mouse", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Text("LMB - place blocks");
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("+ CTRL - Multiple selection");
                        ImGui::EndTooltip();
                    }
                    ImGui::Text("RMB - remove blocks");
                }
                ImGui::NewLine();
                if (ImGui::CollapsingHeader("\xef\x84\x9c Keyboard", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Text("WASD - Move camera");
                    ImGui::Text("R - Rotate");
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("+ SHIFT - Anti-clockwise");
                        ImGui::EndTooltip();
                    }
                    ImGui::Text("0 - 9 - Block type");
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("+ SHIFT - Another set of blocks");
                        ImGui::EndTooltip();
                    }
                }
                ImGui::NewLine();
                if (ImGui::Button("OK", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
                    controlsMenu = false;
                }
            }
            ImGui::End();
        }
        Engine::HUD::end();

        if (!focused) {
            float cameraSpeed = 500.f * camera->zoom * camera->zoom * io->DeltaTime;
            if (window->isKeyPressed(GLFW_KEY_A)) {
                camera->position.x -= cameraSpeed;
            } else if (window->isKeyPressed(GLFW_KEY_D)) {
                camera->position.x += cameraSpeed;
            }

            if (window->isKeyPressed(GLFW_KEY_W)) {
                camera->position.y += cameraSpeed;
            } else if (window->isKeyPressed(GLFW_KEY_S)) {
                camera->position.y -= cameraSpeed;
            }
            if (window->isKeyPressed(GLFW_KEY_A) || window->isKeyPressed(GLFW_KEY_D) ||
                window->isKeyPressed(GLFW_KEY_W) || window->isKeyPressed(GLFW_KEY_S)) {
                camera->updateView();
            }
            static bool prev = false;
            if ((mouseButtons >> GLFW_MOUSE_BUTTON_LEFT) & 1U && !window->isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
                if (!blocks->has(blockX, blockY)) {
                    blocks->set(blockX, blockY, new Block(blockX, blockY, &blocks->types[currentBlock],
                                                          static_cast<BlockRotation>(currentRotation)));
                } else if (!prev) {
                    Block *block = blocks->get(blockX, blockY);
                    if (block->type->id == blocks->types[12].id) {
                        block->active ^= 1;
                    } else {
                        blocks->rotate(blockX, blockY, rotateBlock(block->rotation,
                                                                   window->isKeyPressed(GLFW_KEY_LEFT_SHIFT) ? -1.f
                                                                                                             : 1.f));
                    }
                }
                prev = true;
            } else {
                prev = false;
            }
            if ((mouseButtons >> GLFW_MOUSE_BUTTON_RIGHT) & 1U) {
                blocks->erase(blockX, blockY);
            }
        }
    }
    blocks->thread.join();
    delete window;
    Engine::Window::destroy();
    return 0;
}
