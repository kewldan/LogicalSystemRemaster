#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define GLFW_INCLUDE_NONE
#define STB_TRUETYPE_IMPLEMENTATION

#include <plog/Log.h>
#include "Window.h"
#include "HUD.h"
#include "BlockManager.h"
#include "Camera.h"
#include <cmath>
#include "RenderPipeline.h"
#include <format>
#include "Input.h"

Engine::Window *window;
Engine::Input *input;
Engine::Camera *camera;

BlockManager *blocks;
RenderPipeline *pipeline;

ImGuiIO *io;
ImFontConfig font_cfg;

struct PlayerInput {
    int currentBlock;
    int currentRotation;
} playerInput;

bool saveMenu, bloom = true, vsync, _vsync = true;;
char *saveFilename = nullptr;
int blockX, blockY, selectedBlocks;
ImGui::FileBrowser saveDialog(ImGuiFileBrowserFlags_SelectDirectory | ImGuiFileBrowserFlags_CreateNewDir), loadDialog;

void load_example(const char *path) {
    int size = 0;
    auto data = (const char *) Engine::File::readResourceFile(path, &size);
    blocks->load_from_memory(camera, data, size);
    ImGuiToast toast(ImGuiToastType_Success, 2000);
    toast.set_title("%s loaded successfully", path);
    ImGui::InsertNotification(toast);
}

void mouse_wheel_callback(GLFWwindow *w, double xOffset, double yOffset) {
    camera->zoomIn((float) yOffset * -0.1f);
}

void cut() {
    blocks->cut(selectedBlocks, blockX, blockY);
    ImGuiToast toast(ImGuiToastType_Success, 2000);
    toast.set_title("%d blocks cut", selectedBlocks);
    ImGui::InsertNotification(toast);
}

void copy() {
    blocks->copy(selectedBlocks, blockX, blockY);
    ImGuiToast toast(ImGuiToastType_Success, 2000);
    toast.set_title("%d blocks copied", selectedBlocks);
    ImGui::InsertNotification(toast);
}

void paste() {
    ImGuiToast toast(0);
    int count = blocks->paste(blockX, blockY);
    if (count != -1) {
        toast.set_type(ImGuiToastType_Success);
        toast.set_title("%d blocks pasted", count);
    } else {
        toast.set_type(ImGuiToastType_Error);
        toast.set_title("Failed to paste");
    }
    ImGui::InsertNotification(toast);
}

void select_all() {
    blocks->select_all();
    selectedBlocks = blocks->length();
    ImGuiToast toast(ImGuiToastType_Info, 2000);
    toast.set_title("%d blocks selected", selectedBlocks);
    ImGui::InsertNotification(toast);
}

float map(float value, float max1, float min2, float max2) {
    return min2 + value * (max2 - min2) / max1;
}

int main() {
    Engine::Window::init();
    window = new Engine::Window(1280, 720, "Logical system");
    window->setIcon("data/textures/favicon.png");
    input = new Engine::Input(window->getId());
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    input->registerCallbacks();

    glfwSetScrollCallback(window->getId(), mouse_wheel_callback);

    PLOGI << "<< LOADING ASSETS >>";

    Engine::HUD::init(window);
    camera = new Engine::Camera(window);
    pipeline = new RenderPipeline(
            new Engine::Shader("block"),
            new Engine::Shader("blur"),
            new Engine::Shader("final"),
            new Engine::Shader("background"),
            new Engine::Shader("quad"),
            new Engine::Shader("glow"),
            window->width,
            window->height
    );
    io = &ImGui::GetIO();

    saveFilename = new char[128];
    memset(saveFilename, 0, 128);

    font_cfg.FontDataOwnedByAtlas = false;
    int size = 0;
    void *fontData = Engine::File::readResourceFile("FONT", &size);
    io->Fonts->AddFontFromMemoryTTF(fontData, size, 16.f, &font_cfg);
    ImGui::MergeIconsWithLatestFont(16.f, false);

    saveDialog.SetTitle("Save scheme");
    loadDialog.SetTitle("Load scheme");
    loadDialog.SetTypeFilters({".ls", ".bson"});

    blocks = new BlockManager(window, quadVertices, (int) sizeof(quadVertices));

    PLOGI << "<< STARING GAME LOOP >>";
    do {
        input->update();
        camera->update();
        window->reset();

        if (window->isResized()) {
            pipeline->resize(window->width, window->height);
            camera->updateOrthographic();
        }
        float cursorX = map(input->getCursorPosition().x, (float) window->width, camera->left, camera->right);
        float cursorY = map(input->getCursorPosition().y, (float) window->height, camera->bottom, camera->top);
        blockX = (int) floorf((cursorX + camera->position.x) / 32.f +
                              0.5f);
        blockY = (int) floorf((((float) window->height - cursorY) + camera->position.y) / 32.f +
                              0.5f);

        if (!io->WantCaptureKeyboard) {
            for (int i = 0; i <= 10; i++) {
                if (input->isKeyJustPressed(GLFW_KEY_0 + i)) {
                    playerInput.currentBlock = !i ? 9 : i - 1;
                    playerInput.currentBlock += 10 * input->isKeyPressed(GLFW_KEY_LEFT_SHIFT);
                    playerInput.currentBlock = std::min(playerInput.currentBlock, 14);
                    break;
                }
            }
            if (input->isKeyJustPressed(GLFW_KEY_R)) {
                playerInput.currentRotation = rotateBlock(playerInput.currentRotation,
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
                    cut();
                }
                if (input->isKeyJustPressed(GLFW_KEY_C)) {
                    copy();
                }
                if (input->isKeyJustPressed(GLFW_KEY_V)) {
                    paste();
                }
                if (input->isKeyJustPressed(GLFW_KEY_A)) {
                    select_all();
                }
                if (input->isKeyJustPressed(GLFW_KEY_N)) {
                    blocks->blocks.clear();
                }
            }

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
            if (input->isKeyPressed(GLFW_KEY_A) || input->isKeyPressed(GLFW_KEY_D) ||
                input->isKeyPressed(GLFW_KEY_W) || input->isKeyPressed(GLFW_KEY_S)) {
                camera->updateView();
            }
            if (!io->WantCaptureMouse) {
                if (!input->isKeyPressed(GLFW_KEY_LEFT_SHIFT)) {
                    Block *block = blocks->get(blockX, blockY);
                    if (block) {
                        if (input->isMouseButtonJustPressed(GLFW_MOUSE_BUTTON_LEFT)) {
                            if (block->type->id == blocks->types[12].id) {
                                block->active ^= 1;
                            } else {
                                blocks->rotate(blockX, blockY, rotateBlock(block->rotation,
                                                                           input->isKeyPressed(GLFW_KEY_LEFT_SHIFT) ? -1.f
                                                                                                                    : 1.f));
                            }
                        }
                    } else if (input->isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
                        if (!blocks->has(blockX, blockY)) {
                            blocks->set(blockX, blockY, new Block(blockX, blockY, &blocks->types[playerInput.currentBlock],
                                                                  playerInput.currentRotation));
                        }
                    }
                    if (input->isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
                        blocks->erase(blockX, blockY);
                    }
                }
            }
        }

        pipeline->beginPass(camera, bloom, blocks->atlas, blocks->VAO, []() { blocks->draw(camera); });

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
            selectedBlocks = 0;
            for (auto &it: blocks->blocks) {
                int px = Block_X(it.first) << 5;
                int py = Block_Y(it.first) << 5;
                it.second->selected = px > s_LB && px < s_RB && py > s_BB && py < s_TB;
                selectedBlocks += it.second->selected;
            }

            ImGuiToast toast(ImGuiToastType_Info, 2000);
            toast.set_title("Selected %d blocks", selectedBlocks);
            ImGui::InsertNotification(toast);
        }

        Engine::HUD::begin();
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
        ImGui::SetNextWindowBgAlpha(0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        if (ImGui::Begin("##Debug", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                             ImGuiWindowFlags_NoFocusOnAppearing |
                                             ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove)) {
            ImGui::Text("FPS: %.0f", io->Framerate);
            ImGui::Text("Tick: %.1fms", blocks->tickTime * 1000.);
        }
        ImGui::End();
        ImGui::PopStyleVar();
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
                        copy();
                    if (ImGui::MenuItem("\xef\x83\xaa Paste", "Ctrl + V"))
                        paste();
                    if (ImGui::MenuItem("\xef\x83\x84 Cut", "Ctrl + X"))
                        cut();
                    if (ImGui::MenuItem("\xef\xa1\x8c Select all", "Ctrl + A"))
                        select_all();
                    if (ImGui::MenuItem("\xef\x87\xb8 Delete", "DELETE"))
                        blocks->delete_selected();
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Examples")) {
                    if (ImGui::MenuItem("\xef\x81\xbc Blocks overview")) load_example("data/examples/Blocks.ls");
                    if (ImGui::MenuItem("\xef\x81\xbc 1 Byte RAM")) load_example("data/examples/RAM1Byte.ls");
                    if (ImGui::MenuItem("\xef\x81\xbc 4 Bit adder")) load_example("data/examples/Adder4Bit.ls");
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Graphics")) {
                    ImGui::MenuItem("VSync", nullptr, &_vsync);

                    if (vsync != _vsync) {
                        vsync = _vsync;
                        Engine::Window::setVsync(vsync);
                    }

                    ImGui::MenuItem("Bloom", nullptr, &bloom);
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Help")) {
                    if (ImGui::MenuItem("Itch.io"))
                        ShellExecute(nullptr, nullptr, "https://kewldan.itch.io/logical-system", nullptr, nullptr,
                                     SW_SHOW);
                    if (ImGui::MenuItem("Source code"))
                        ShellExecute(nullptr, nullptr, "https://github.com/kewldan/LogicalSystemRemaster", nullptr,
                                     nullptr, SW_SHOW);
                    ImGui::MenuItem(std::format("Version: 2.0.1 ({})", __DATE__).c_str(), nullptr, nullptr, false);
                    ImGui::MenuItem("Author: kewldan", nullptr, nullptr, false);
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }
            ImGui::Checkbox("Play", &blocks->simulate);
            ImGui::SliderInt("TPS", &blocks->TPS, 2, 256);
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Simulation ticks per second");
                ImGui::EndTooltip();
            }
            ImGui::Combo("Block", &playerInput.currentBlock,
                         "Wire straight\0Wire angled right\0Wire angled left\0Wire T\0Wire cross\0Wire 2\0Wire 3\0NOT\0AND\0NAND\0XOR\0NXOR\0Switch\0Clock\0Lamp\0");
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Use 0-9 for 0-9 elements\nand SHIFT for 10-14 elements");
                ImGui::EndTooltip();
            }
            ImGui::Combo("Rotation", &playerInput.currentRotation, "Up\0Right\0Down\0Left");
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Use R for rotate clockwise\nand SHIFT for anti-clockwise");
                ImGui::EndTooltip();
            }
        }
        ImGui::End();

        loadDialog.Display();
        if (loadDialog.HasSelected()) {
            auto p = loadDialog.GetSelected();
            std::string s = p.string();
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
            auto p = saveDialog.GetSelected();
            std::string s = p.string();
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
