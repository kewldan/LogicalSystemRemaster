#include "Window.h"
#include "HUD.h"
#include "BlockManager.h"
#include "Camera2D.h"
#include "RenderPipeline.h"
#include "Input.h"
#include "nfd.h"
#include <format>

bool vsync = true, hideUI = false;


void openLoadDialog(BlockManager &blocks, Engine::Camera2D *camera) {
    nfdchar_t *loadPath;
    nfdresult_t loadResult = NFD_OpenDialog("bson;ls", nullptr, &loadPath);

    if (loadResult == NFD_OKAY) {
        ImGuiToast toast(0);
        if (blocks.load(camera, loadPath)) {
            toast.set_type(ImGuiToastType_Success);
            toast.set_title("%s loaded successfully", loadPath);
        } else {
            toast.set_type(ImGuiToastType_Error);
            toast.set_title("%s was not loaded!", loadPath);
        }
        ImGui::InsertNotification(toast);

        free(loadPath);
    }
}

void openSaveDialog(BlockManager &blocks, Engine::Camera2D *camera) {
    nfdchar_t *savePath;
    nfdresult_t saveResult = NFD_SaveDialog("bson;ls", nullptr, &savePath);

    if (saveResult == NFD_OKAY) {
        ImGuiToast toast(0);
        if (blocks.save(camera, savePath)) {
            toast.set_type(ImGuiToastType_Success);
            toast.set_title("%s saved successfully", const_cast<const char *>(savePath));
        } else {
            toast.set_type(ImGuiToastType_Error);
            toast.set_title("%s was not saved!", const_cast<const char *>(savePath));
        }
        ImGui::InsertNotification(toast);

        free(savePath);
    }
}

float map(float value, float max1, float min2, float max2) {
    return min2 + value * (max2 - min2) / max1;
}

int main() {
    Engine::Window::init();
    Engine::Window window(1280, 720, "Logical system");
    window.setIcon("data/textures/favicon.png");
    Engine::Input input(window.getId());
    Engine::Camera2D camera(&window);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    input.registerCallbacks();

    Engine::HUD::init(&window);
    RenderPipeline pipeline(
            new Engine::Shader("block"),
            new Engine::Shader("blur"),
            new Engine::Shader("final"),
            new Engine::Shader("background"),
            new Engine::Shader("quad"),
            window.width,
            window.height
    );
    auto io = &ImGui::GetIO();

    char saveFilename[256];
    memset(saveFilename, 0, 256);

    ImFontConfig font_cfg;
    font_cfg.FontDataOwnedByAtlas = false;

    int fontSize = 0;
    void *fontData = Engine::Filesystem::readResourceFile("data/fonts/comfortaa.ttf", &fontSize);
    io->Fonts->AddFontFromMemoryTTF(fontData, fontSize, 16.f, &font_cfg);
    static const ImWchar icons_ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};

    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.FontDataOwnedByAtlas = false;

#ifndef NDEBUG
    io->Fonts->AddFontFromFileTTF("data/fonts/fa.ttf", 16.f, &icons_config,
                                  icons_ranges);
#else
    int iconsFontSize;
    void *bin = static_cast<void *>(Engine::Filesystem::readResourceFile("data/fonts/fa.ttf", &iconsFontSize));
    io->Fonts->AddFontFromMemoryTTF(bin, iconsFontSize, 16.f, &icons_config,
                                    icons_ranges);
#endif

    BlockManager blocks(&window, quadVertices, (int) sizeof(quadVertices));

    int blockX, blockY;

    do {
        if (window.isResized()) {
            pipeline.resize(window.width, window.height);
        }

        input.update();
        camera.update();
        window.reset();
        window.setVsync(vsync);

        float cursorX = map(input.getCursorPosition().x, (float) window.width, camera.left, camera.right);
        float cursorY = map(input.getCursorPosition().y, (float) window.height, camera.bottom, camera.top);
        blockX = (int) floorf((cursorX + camera.position.x) / 32.f +
                              0.5f);
        blockY = (int) floorf((((float) window.height - cursorY) + camera.position.y) / 32.f +
                              0.5f);

        pipeline.beginPass(&camera, blocks.atlas, blocks.VAO, [&blocks, &camera]() { blocks.draw(&camera); });

        if (!io->WantCaptureKeyboard) {
            for (int i = 0; i <= 10; i++) {
                if (input.isKeyJustPressed(GLFW_KEY_0 + i)) {
                    blocks.currentBlock = !i ? 9 : i - 1;
                    blocks.currentBlock += 10 * input.isKeyPressed(GLFW_KEY_LEFT_SHIFT);
                    blocks.currentBlock = std::min(blocks.currentBlock, 14);
                    break;
                }
            }
            if (input.isKeyJustPressed(GLFW_KEY_F1)) {
                ImGuiToast toast(ImGuiToastType_Info, 7000);
                Block *block = blocks.get(blockX, blockY);
                if (block) {
                    toast.set_title("%p - %d blocks\nBlock %dx%d\nType - %d\nData - %.2X", &blocks.blocks,
                                    blocks.length(), blockX, blockY, block->type->id, 217);
                } else {
                    toast.set_title("%p - %d blocks\nBlock %dx%d", &blocks.blocks, blocks.length(), blockX, blockY);
                }
                ImGui::InsertNotification(toast);
            }
            if (input.isKeyJustPressed(GLFW_KEY_F2)) {
                hideUI ^= 1;
            }
            if (input.isKeyJustPressed(GLFW_KEY_F3)) {
                for (int x = 0; x < 256; x++) {
                    for (int y = 0; y < 256; y++) {
                        blocks.set(x, y);
                    }
                }
            }
            if (input.isKeyJustPressed(GLFW_KEY_R)) {
                blocks.currentRotation = rotateBlock(blocks.currentRotation,
                                                     input.isKeyPressed(GLFW_KEY_LEFT_SHIFT) ? -1 : 1);
            }
            if (input.isKeyJustPressed(GLFW_KEY_DELETE)) {
                blocks.delete_selected();
            }
            if (input.isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
                if (input.isKeyJustPressed(GLFW_KEY_S)) {
                    openSaveDialog(blocks, &camera);
                }
                if (input.isKeyJustPressed(GLFW_KEY_O)) {
                    openLoadDialog(blocks, &camera);
                }
                if (input.isKeyJustPressed(GLFW_KEY_X)) {
                    blocks.cut(blockX, blockY);
                }
                if (input.isKeyJustPressed(GLFW_KEY_C)) {
                    blocks.copy(blockX, blockY);
                }
                if (input.isKeyJustPressed(GLFW_KEY_V)) {
                    blocks.paste(blockX, blockY);
                }
                if (input.isKeyJustPressed(GLFW_KEY_A)) {
                    blocks.select_all();
                }
                if (input.isKeyJustPressed(GLFW_KEY_N)) {
                    blocks.blocks.clear();
                }
            } else {
                float cameraSpeed = 500.f * camera.getZoom() * camera.getZoom() * io->DeltaTime;
                if (input.isKeyPressed(GLFW_KEY_A)) {
                    camera.position.x -= cameraSpeed;
                }
                if (input.isKeyPressed(GLFW_KEY_D)) {
                    camera.position.x += cameraSpeed;
                }

                if (input.isKeyPressed(GLFW_KEY_W)) {
                    camera.position.y += cameraSpeed;
                }
                if (input.isKeyPressed(GLFW_KEY_S)) {
                    camera.position.y -= cameraSpeed;
                }
            }

            if (!io->WantCaptureMouse) {
                if (input.getMouseWheelDelta().y != 0.f) {
                    camera.zoomIn(input.getMouseWheelDelta().y * -0.1f);
                }
                if (!input.isKeyPressed(GLFW_KEY_LEFT_SHIFT)) {
                    Block *block = blocks.get(blockX, blockY);
                    if (block) {
                        if (input.isMouseButtonJustPressed(GLFW_MOUSE_BUTTON_LEFT)) {
                            if (block->type->id == blocks.types[12].id) {
                                block->active ^= 1;
                            } else {
                                blocks.rotate(blockX, blockY, rotateBlock(block->rotation,
                                                                          input.isKeyPressed(GLFW_KEY_LEFT_SHIFT)
                                                                          ? -1.f
                                                                          : 1.f));
                            }
                        }
                    } else if (input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
                        if (!blocks.has(blockX, blockY)) {
                            blocks.set(blockX, blockY);
                        }
                    }
                    if (input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
                        blocks.erase(blockX, blockY);
                    }
                }
            }
        }

        static glm::vec2 cameraStart, cameraDelta, size, start, delta;
        if (input.isStartDragging()) {
            cameraStart = camera.position;
        }
        if (input.isDragging()) {
            start = input.getDraggingStartPosition();
            start.x = map(start.x, (float) window.width, camera.left, camera.right);
            start.y = map(start.y, (float) window.height, camera.bottom, camera.top);
            delta = input.getCursorPosition() - input.getDraggingStartPosition();
            delta.x *= (camera.right - camera.left) / (float) window.width;
            delta.y *= (camera.top - camera.bottom) / (float) window.height;
            cameraDelta = glm::vec2(camera.position.x, camera.position.y) - cameraStart;

            size = glm::vec2(delta.x + cameraDelta.x, delta.y - cameraDelta.y);

            int s_x = (int) std::min(start.x, start.x + size.x);
            int s_y = (int) std::max(start.y, start.y + size.y);

            pipeline.drawSelection(&camera, glm::vec2(s_x, window.height - s_y) - cameraDelta, glm::abs(size));
        }
        if (input.isStopDragging()) {
            int s_LB = (int) (std::min(start.x, start.x + size.x) + camera.position.x - cameraDelta.x);
            int s_BB = (int) ((float) window.height - std::max(start.y, start.y + size.y) +
                              camera.position.y -
                              cameraDelta.y);

            int s_RB = s_LB + (int) abs(size.x);
            int s_TB = s_BB + (int) abs(size.y);
            blocks.selectedBlocks = 0;
            for (auto &it: blocks.blocks) {
                int px = Block_X(it.first) << 5;
                int py = Block_Y(it.first) << 5;
                it.second->selected = px > s_LB && px < s_RB && py > s_BB && py < s_TB;
                blocks.selectedBlocks += it.second->selected;
            }

            ImGuiToast toast(ImGuiToastType_Info, 2000);
            toast.set_title("Selected %d blocks", blocks.selectedBlocks);
            ImGui::InsertNotification(toast);
        }

        Engine::HUD::begin();
        if (!hideUI) {
            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
            ImGui::SetNextWindowBgAlpha(0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
            if (ImGui::Begin("##Debug", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                                 ImGuiWindowFlags_NoFocusOnAppearing |
                                                 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove)) {
                ImGui::Text("FPS: %.0f", io->Framerate);
                ImGui::Text("Tick: %.1fms", blocks.tickTime);
            }
            ImGui::End();
            ImGui::PopStyleVar();

            ImGui::SetNextWindowPos(ImVec2(5, 50), ImGuiCond_Once);
            if (ImGui::Begin("Simulation", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_MenuBar)) {
                if (ImGui::BeginMenuBar()) {
                    if (ImGui::BeginMenu("File")) {
                        if (ImGui::MenuItem(ICON_FA_FILE "  New", "Ctrl + N"))
                            blocks.blocks.clear();
                        if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Open", "Ctrl + O"))
                            openLoadDialog(blocks, &camera);
                        if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK "  Save", "Ctrl + S"))
                            openSaveDialog(blocks, &camera);
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Edit")) {
                        if (ImGui::MenuItem(ICON_FA_COPY " Copy", "Ctrl + C"))
                            blocks.copy(blockX, blockY);
                        if (ImGui::MenuItem(ICON_FA_PASTE " Paste", "Ctrl + V"))
                            blocks.paste(blockX, blockY);
                        if (ImGui::MenuItem(ICON_FA_HAND_SCISSORS " Cut", "Ctrl + X"))
                            blocks.cut(blockX, blockY);
                        if (ImGui::MenuItem(ICON_FA_SQUARE_CHECK " Select all", "Ctrl + A"))
                            blocks.select_all();
                        if (ImGui::MenuItem(ICON_FA_TRASH_CAN " Delete", "DELETE"))
                            blocks.delete_selected();
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Examples")) {
                        if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Blocks overview"))
                            blocks.load_example(&camera, "data/examples/BlocksSample.bson");
                        if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " 1 Byte RAM"))
                            blocks.load_example(&camera, "data/examples/MemorySample.bson");
                        if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " 4 Bit adder"))
                            blocks.load_example(&camera, "data/examples/AdderSample.bson");
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Graphics")) {
                        ImGui::MenuItem("VSync", nullptr, &vsync);
                        ImGui::MenuItem("Bloom", nullptr, &pipeline.bloom);
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Help")) {
                        if (ImGui::MenuItem("Itch.io"))
                            ShellExecute(nullptr, nullptr, "https://kewldan.itch.io/logical-system", nullptr, nullptr,
                                         SW_SHOW);
                        if (ImGui::MenuItem("Source code"))
                            ShellExecute(nullptr, nullptr, "https://github.com/kewldan/LogicalSystemRemaster", nullptr,
                                         nullptr, SW_SHOW);
                        static const std::string versionString = std::format("Version: 2.0.9 ({})", __DATE__);
                        static const char *version = versionString.c_str();
                        ImGui::MenuItem(version, nullptr, nullptr, false);
                        ImGui::MenuItem("Author: kewldan", nullptr, nullptr, false);
                        ImGui::EndMenu();
                    }
                    ImGui::EndMenuBar();
                }
                ImGui::Checkbox("Play", &blocks.simulate);
                if (!blocks.simulate) {
                    ImGui::SameLine();
                    if (ImGui::Button("Tick")) {
                        blocks.update();
                    }
                }
                ImGui::SliderInt("TPS", &blocks.TPS, 2, 256);
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Simulation ticks per second");
                    ImGui::EndTooltip();
                }
                ImGui::Combo("Block", &blocks.currentBlock,
                             "Wire straight\0Wire angled right\0Wire angled left\0Wire T\0Wire cross\0Wire 2\0Wire 3\0NOT\0AND\0NAND\0XOR\0NXOR\0Switch\0Clock\0Lamp\0");
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Use 0-9 for 0-9 elements\nand SHIFT for 10-14 elements");
                    ImGui::EndTooltip();
                }
                ImGui::Combo("Rotation", &blocks.currentRotation, "Up\0Right\0Down\0Left\0");
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Use R for rotate clockwise\nand SHIFT for anti-clockwise");
                    ImGui::EndTooltip();
                }
            }
            ImGui::End();
        }
        Engine::HUD::end();
    } while (window.update());
    blocks.thread.join();
    return 0;
}
