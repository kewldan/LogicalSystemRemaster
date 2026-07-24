#include "Window.h"
#include "HUD.h"
#include "BlockManager.h"
#include "Camera2D.h"
#include "RenderPipeline.h"
#include "Input.h"
#include "nfd.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#if defined(__APPLE__)
#define PRIMARY_SHORTCUT_MODIFIER "Cmd"
#else
#define PRIMARY_SHORTCUT_MODIFIER "Ctrl"
#endif

bool vsync = true, hideUI = false;
static const nfdfilteritem_t schemeFilter[] = {{"Logical System schemes", "bson,ls"}};

struct AppSettings {
    int width = 1280, height = 720, tps = 8;
    bool vsync = true, bloom = true;
};

static AppSettings loadSettings() {
    AppSettings s;
    try {
        std::ifstream file("settings.json");
        if (file.good()) {
            nlohmann::json j = nlohmann::json::parse(file);
            s.width = std::clamp(j.value("width", s.width), 640, 7680);
            s.height = std::clamp(j.value("height", s.height), 480, 4320);
            s.tps = std::clamp(j.value("tps", s.tps), 1, 256);
            s.vsync = j.value("vsync", s.vsync);
            s.bloom = j.value("bloom", s.bloom);
        }
    } catch (const std::exception &e) {
        PLOGW << "settings.json is invalid: " << e.what();
    }
    return s;
}

static void saveSettings(const AppSettings &s) {
    nlohmann::json j{
            {"width",  s.width},
            {"height", s.height},
            {"tps",    s.tps},
            {"vsync",  s.vsync},
            {"bloom",  s.bloom}
    };
    std::ofstream file("settings.json");
    file << j.dump(2);
}

void openLoadDialog(BlockManager &blocks, Engine::Camera2D *camera) {
    nfdchar_t *loadPath;
    nfdresult_t loadResult = NFD_OpenDialog(&loadPath, schemeFilter, 1, nullptr);

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

        NFD_FreePath(loadPath);
    }
}

// Saves silently into the current file; shows the dialog only for a new
// scheme or when forceDialog is set (Save As). Returns true when saved.
bool saveScheme(BlockManager &blocks, Engine::Camera2D *camera, bool forceDialog) {
    std::string path = blocks.currentFile;
    if (forceDialog || path.empty()) {
        nfdchar_t *savePath = nullptr;
        if (NFD_SaveDialog(&savePath, schemeFilter, 1, nullptr, nullptr) != NFD_OKAY) return false;
        path = savePath;
        NFD_FreePath(savePath);
        if (!std::filesystem::path(path).has_extension()) path += ".bson";
    }

    bool ok = blocks.save(camera, path.c_str());
    ImGuiToast toast(0);
    if (ok) {
        toast.set_type(ImGuiToastType_Success);
        toast.set_title("%s saved", path.c_str());
    } else {
        toast.set_type(ImGuiToastType_Error);
        toast.set_title("%s was not saved!", path.c_str());
    }
    ImGui::InsertNotification(toast);
    return ok;
}

float map(float value, float max1, float min2, float max2) {
    return min2 + value * (max2 - min2) / max1;
}

enum class PendingAction {
    None, NewScheme, OpenScheme, Exit
};

static void setWorkingDirectoryToExecutable() {
    // Resolve data/ (and logs) relative to the executable, not the caller's cwd
#ifdef _WIN32
    char exeDir[MAX_PATH];
    if (GetModuleFileNameA(nullptr, exeDir, MAX_PATH) > 0) {
        char *slash = strrchr(exeDir, '\\');
        if (slash) {
            *slash = 0;
            SetCurrentDirectoryA(exeDir);
        }
    }
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string executablePath(size, '\0');
    if (_NSGetExecutablePath(executablePath.data(), &size) == 0) {
        std::filesystem::current_path(std::filesystem::path(executablePath.c_str()).parent_path());
    }
#endif
}

static void openUrl(const char *url) {
#ifdef _WIN32
    ShellExecuteA(nullptr, nullptr, url, nullptr, nullptr, SW_SHOW);
#elif defined(__APPLE__)
    std::string command = "open \"" + std::string(url) + "\"";
    std::system(command.c_str());
#endif
}

#ifdef _WIN32
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
#else
int main() {
#endif
    setWorkingDirectoryToExecutable();
    if (NFD_Init() != NFD_OKAY) return 1;

    AppSettings settings = loadSettings();
    vsync = settings.vsync;

    Engine::Window::init();
    Engine::Window window(settings.width, settings.height, "Logical system");
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
    pipeline.bloom = settings.bloom;
    auto io = &ImGui::GetIO();

    ImFontConfig font_cfg;
    font_cfg.FontDataOwnedByAtlas = false;

#if !defined(NDEBUG) || defined(__APPLE__)
    io->Fonts->AddFontFromFileTTF("data/fonts/comfortaa.ttf", 16.f, &font_cfg);
#else
    int fontSize = 0;
    void *fontData = Engine::Filesystem::readResourceFile("data/fonts/comfortaa.ttf", &fontSize);
    if (fontData && fontSize > 0) io->Fonts->AddFontFromMemoryTTF(fontData, fontSize, 16.f, &font_cfg);
#endif
    static const ImWchar icons_ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};

    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.FontDataOwnedByAtlas = false;

#if !defined(NDEBUG) || defined(__APPLE__)
    io->Fonts->AddFontFromFileTTF("data/fonts/fa.ttf", 16.f, &icons_config,
                                  icons_ranges);
#else
    int iconsFontSize;
    void *bin = static_cast<void *>(Engine::Filesystem::readResourceFile("data/fonts/fa.ttf", &iconsFontSize));
    if (bin && iconsFontSize > 0) io->Fonts->AddFontFromMemoryTTF(bin, iconsFontSize, 16.f, &icons_config,
                                                                  icons_ranges);
#endif

    BlockManager blocks(&window, quadVertices, (int) sizeof(quadVertices));
    blocks.TPS = settings.tps;

    int blockX, blockY;
    bool running = true;
    PendingAction pending = PendingAction::None;
    bool painting = false, movingSelection = false;
    int lastPaintX = 0, lastPaintY = 0, moveStartX = 0, moveStartY = 0;

    auto executeAction = [&](PendingAction action) {
        switch (action) {
            case PendingAction::NewScheme:
                blocks.clear();
                break;
            case PendingAction::OpenScheme:
                openLoadDialog(blocks, &camera);
                break;
            case PendingAction::Exit:
                running = false;
                break;
            default:
                break;
        }
    };
    auto request = [&](PendingAction action) {
        if (blocks.dirty) {
            pending = action;
        } else {
            executeAction(action);
        }
    };

    // Draws straight wires along an orthogonal path (x first, then y),
    // turning each previous wire toward the freshly placed one.
    auto paintWire = [&](int fromX, int fromY, int toX, int toY) {
        int x = fromX, y = fromY;
        while (x != toX || y != toY) {
            int dx = x != toX ? (toX > x ? 1 : -1) : 0;
            int dy = dx == 0 ? (toY > y ? 1 : -1) : 0;
            BlockRotation dir = dx == 1 ? 1 : dx == -1 ? 3 : dy == 1 ? 0 : 2;
            Block *prev = blocks.get(x, y);
            if (prev && prev->typeId == 0) blocks.rotate(x, y, dir);
            x += dx;
            y += dy;
            if (!blocks.has(x, y)) {
                blocks.set(x, y, Block(0, dir));
            }
        }
    };

    while (running) {
        if (window.isResized()) {
            pipeline.resize(window.width, window.height);
        }

        input.update();
        camera.update();
        window.reset();
        window.setVsync(vsync);

        // GLFW reports the cursor in logical points, while the renderer uses
        // framebuffer pixels. They differ on Retina / HiDPI displays.
        int logicalWidth, logicalHeight;
        glfwGetWindowSize(window.getId(), &logicalWidth, &logicalHeight);
        const float cursorScaleX = logicalWidth > 0 ? (float) window.width / logicalWidth : 1.f;
        const float cursorScaleY = logicalHeight > 0 ? (float) window.height / logicalHeight : 1.f;
        auto toFramebuffer = [&](glm::vec2 position) {
            return glm::vec2(position.x * cursorScaleX, position.y * cursorScaleY);
        };
        const glm::vec2 cursorPosition = toFramebuffer(input.getCursorPosition());

        static double tickAccumulator = 0.0;
        if (blocks.simulate) {
            tickAccumulator += io->DeltaTime;
            double period = 1.0 / (double) std::clamp(blocks.TPS, 1, 1000);
            int steps = 0;
            while (tickAccumulator >= period && steps < 8) {
                blocks.update();
                tickAccumulator -= period;
                steps++;
            }
            if (steps == 8) tickAccumulator = 0.0; // fell behind, do not spiral
        } else {
            tickAccumulator = 0.0;
        }

        float cursorX = map(cursorPosition.x, (float) window.width, camera.left, camera.right);
        float cursorY = map(cursorPosition.y, (float) window.height, camera.bottom, camera.top);
        blockX = (int) floorf((cursorX + camera.position.x) / 32.f +
                              0.5f);
        blockY = (int) floorf((((float) window.height - cursorY) + camera.position.y) / 32.f +
                              0.5f);

        bool showGhost = !io->WantCaptureMouse && !hideUI && !blocks.pasting && !movingSelection &&
                         !input.isDragging() && !blocks.has(blockX, blockY);

        pipeline.beginPass(&camera, blocks.atlas, blocks.VAO, [&]() {
            blocks.draw(&camera);
            Engine::Shader *shader = pipeline.blockShader();
            if (blocks.pasting) {
                shader->upload("alpha", 0.5f);
                blocks.drawPasteGhost(blockX, blockY);
                shader->upload("alpha", 1.f);
            } else if (movingSelection) {
                shader->upload("alpha", 0.5f);
                blocks.drawSelectionGhost(blockX - moveStartX, blockY - moveStartY);
                shader->upload("alpha", 1.f);
            } else if (showGhost) {
                shader->upload("alpha", 0.45f);
                blocks.drawGhost(blockX, blockY);
                shader->upload("alpha", 1.f);
            }
        });

        if (!io->WantCaptureKeyboard) {
            for (int i = 0; i < 10; i++) {
                if (input.isKeyJustPressed(GLFW_KEY_0 + i)) {
                    blocks.currentBlock = !i ? 9 : i - 1;
                    blocks.currentBlock += 10 * input.isKeyPressed(GLFW_KEY_LEFT_SHIFT);
                    blocks.currentBlock = std::min(blocks.currentBlock, (int) BLOCK_TYPE_COUNT - 1);
                    break;
                }
            }
            if (input.isKeyJustPressed(GLFW_KEY_ESCAPE)) {
                if (blocks.pasting) {
                    blocks.cancelPaste();
                } else {
                    blocks.deselect_all();
                }
            }
            if (input.isKeyJustPressed(GLFW_KEY_F1)) {
                ImGuiToast toast(ImGuiToastType_Info, 7000);
                Block *block = blocks.get(blockX, blockY);
                if (block) {
                    toast.set_title("%p - %d blocks\nBlock %dx%d\nType - %d\nData - %.2X", &blocks.blocks,
                                    blocks.length(), blockX, blockY, block->typeId, 217);
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
                blocks.commitUndo();
            }
            if (input.isKeyJustPressed(GLFW_KEY_R)) {
                if (blocks.selectedBlocks > 0) {
                    blocks.rotate_selected(input.isKeyPressed(GLFW_KEY_LEFT_SHIFT) ? -1 : 1);
                } else {
                    blocks.currentRotation = rotateBlock(blocks.currentRotation,
                                                         input.isKeyPressed(GLFW_KEY_LEFT_SHIFT) ? -1 : 1);
                }
            }
            if (input.isKeyJustPressed(GLFW_KEY_DELETE)) {
                blocks.delete_selected();
            }
            #if defined(__APPLE__)
            const bool primaryShortcutModifier = input.isKeyPressed(GLFW_KEY_LEFT_SUPER) ||
                                                input.isKeyPressed(GLFW_KEY_RIGHT_SUPER);
            #else
            const bool primaryShortcutModifier = input.isKeyPressed(GLFW_KEY_LEFT_CONTROL) ||
                                                input.isKeyPressed(GLFW_KEY_RIGHT_CONTROL);
            #endif
            if (primaryShortcutModifier) {
                if (input.isKeyJustPressed(GLFW_KEY_S)) {
                    saveScheme(blocks, &camera, input.isKeyPressed(GLFW_KEY_LEFT_SHIFT));
                }
                if (input.isKeyJustPressed(GLFW_KEY_O)) {
                    request(PendingAction::OpenScheme);
                }
                if (input.isKeyJustPressed(GLFW_KEY_X)) {
                    blocks.cut(blockX, blockY);
                }
                if (input.isKeyJustPressed(GLFW_KEY_C)) {
                    blocks.copy(blockX, blockY);
                }
                if (input.isKeyJustPressed(GLFW_KEY_V)) {
                    blocks.beginPaste();
                }
                if (input.isKeyJustPressed(GLFW_KEY_A)) {
                    blocks.select_all();
                }
                if (input.isKeyJustPressed(GLFW_KEY_N)) {
                    request(PendingAction::NewScheme);
                }
                if (input.isKeyJustPressed(GLFW_KEY_Z)) {
                    if (input.isKeyPressed(GLFW_KEY_LEFT_SHIFT)) {
                        blocks.redo();
                    } else {
                        blocks.undo();
                    }
                }
                if (input.isKeyJustPressed(GLFW_KEY_Y)) {
                    blocks.redo();
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
                    camera.zoomAt(input.getMouseWheelDelta().y * -0.1f,
                                  glm::vec2(cursorPosition.x / (float) window.width,
                                            1.f - cursorPosition.y / (float) window.height));
                }
                if (input.isMouseButtonJustPressed(GLFW_MOUSE_BUTTON_MIDDLE)) {
                    Block *picked = blocks.get(blockX, blockY);
                    if (picked) {
                        blocks.currentBlock = picked->typeId;
                        blocks.currentRotation = picked->rotation;
                    }
                }
                if (blocks.pasting) {
                    if (input.isMouseButtonJustPressed(GLFW_MOUSE_BUTTON_LEFT)) {
                        blocks.commitPaste(blockX, blockY);
                    } else if (input.isMouseButtonJustPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
                        blocks.cancelPaste();
                    }
                } else if (!input.isKeyPressed(GLFW_KEY_LEFT_SHIFT) && !movingSelection) {
                    Block *block = blocks.get(blockX, blockY);
                    if (block) {
                        if (input.isMouseButtonJustPressed(GLFW_MOUSE_BUTTON_LEFT)) {
                            if (block->selected && blocks.selectedBlocks > 0) {
                                movingSelection = true;
                                moveStartX = blockX;
                                moveStartY = blockY;
                            } else if (block->typeId == BLOCK_SWITCH || block->typeId == BLOCK_BUTTON) {
                                blocks.toggle(blockX, blockY);
                            } else {
                                blocks.rotate(blockX, blockY, rotateBlock(block->rotation, 1));
                            }
                        }
                    } else if (input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
                        if (painting && blocks.currentBlock == 0 &&
                            (lastPaintX != blockX || lastPaintY != blockY)) {
                            paintWire(lastPaintX, lastPaintY, blockX, blockY);
                        } else {
                            blocks.set(blockX, blockY);
                        }
                        painting = true;
                        lastPaintX = blockX;
                        lastPaintY = blockY;
                    }
                    if (input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
                        blocks.erase(blockX, blockY);
                    }
                }
            }

            if (input.isMouseButtonJustReleased(GLFW_MOUSE_BUTTON_LEFT) ||
                input.isMouseButtonJustReleased(GLFW_MOUSE_BUTTON_RIGHT)) {
                if (movingSelection) {
                    blocks.move_selected(blockX - moveStartX, blockY - moveStartY);
                    movingSelection = false;
                }
                painting = false;
                blocks.commitUndo();
            }
        }

        static glm::vec2 cameraStart, cameraDelta, size, start, delta;
        if (input.isStartDragging()) {
            cameraStart = camera.position;
        }
        if (input.isDragging()) {
            start = toFramebuffer(input.getDraggingStartPosition());
            start.x = map(start.x, (float) window.width, camera.left, camera.right);
            start.y = map(start.y, (float) window.height, camera.bottom, camera.top);
            delta = cursorPosition - toFramebuffer(input.getDraggingStartPosition());
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
                it.second.selected = px > s_LB && px < s_RB && py > s_BB && py < s_TB;
                blocks.selectedBlocks += it.second.selected;
            }

            ImGuiToast toast(ImGuiToastType_Info, 2000);
            toast.set_title("Selected %d blocks", blocks.selectedBlocks);
            ImGui::InsertNotification(toast);
        }

        Engine::HUD::begin();

        if (pending != PendingAction::None && !ImGui::IsPopupOpen("Unsaved changes")) {
            ImGui::OpenPopup("Unsaved changes");
        }
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Unsaved changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("The current scheme has unsaved changes.");
            ImGui::Spacing();
            if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save")) {
                if (saveScheme(blocks, &camera, false)) {
                    PendingAction action = pending;
                    pending = PendingAction::None;
                    ImGui::CloseCurrentPopup();
                    executeAction(action);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Don't save")) {
                PendingAction action = pending;
                pending = PendingAction::None;
                blocks.dirty = false; // user chose to discard
                ImGui::CloseCurrentPopup();
                executeAction(action);
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                pending = PendingAction::None;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

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
                        if (ImGui::MenuItem(ICON_FA_FILE "  New", PRIMARY_SHORTCUT_MODIFIER " + N"))
                            request(PendingAction::NewScheme);
                        if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Open", PRIMARY_SHORTCUT_MODIFIER " + O"))
                            request(PendingAction::OpenScheme);
                        if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK "  Save", PRIMARY_SHORTCUT_MODIFIER " + S"))
                            saveScheme(blocks, &camera, false);
                        if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK "  Save As...", PRIMARY_SHORTCUT_MODIFIER " + Shift + S"))
                            saveScheme(blocks, &camera, true);
                        ImGui::Separator();
                        if (ImGui::MenuItem(ICON_FA_FILE_EXPORT " Export to clipboard"))
                            blocks.export_scheme();
                        if (ImGui::MenuItem(ICON_FA_FILE_IMPORT " Import from clipboard"))
                            blocks.import_scheme(&camera);
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Edit")) {
                        if (ImGui::MenuItem(ICON_FA_ROTATE_LEFT " Undo", PRIMARY_SHORTCUT_MODIFIER " + Z", false, blocks.canUndo()))
                            blocks.undo();
                        if (ImGui::MenuItem(ICON_FA_ROTATE_RIGHT " Redo", PRIMARY_SHORTCUT_MODIFIER " + Y", false, blocks.canRedo()))
                            blocks.redo();
                        ImGui::Separator();
                        if (ImGui::MenuItem(ICON_FA_COPY " Copy", PRIMARY_SHORTCUT_MODIFIER " + C"))
                            blocks.copy(blockX, blockY);
                        if (ImGui::MenuItem(ICON_FA_PASTE " Paste", PRIMARY_SHORTCUT_MODIFIER " + V"))
                            blocks.beginPaste();
                        if (ImGui::MenuItem(ICON_FA_HAND_SCISSORS " Cut", PRIMARY_SHORTCUT_MODIFIER " + X"))
                            blocks.cut(blockX, blockY);
                        if (ImGui::MenuItem(ICON_FA_SQUARE_CHECK " Select all", PRIMARY_SHORTCUT_MODIFIER " + A"))
                            blocks.select_all();
                        if (ImGui::MenuItem(ICON_FA_TRASH_CAN " Delete", "DELETE"))
                            blocks.delete_selected();
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Examples")) {
                        if (ImGui::MenuItem(ICON_FA_LIGHTBULB " Blinker"))
                            blocks.load_example(&camera, "data/examples/blinker.bson", "Blinker");
                        if (ImGui::MenuItem("Logic gates"))
                            blocks.load_example(&camera, "data/examples/gates.bson", "Logic gates");
                        if (ImGui::MenuItem("RS latch"))
                            blocks.load_example(&camera, "data/examples/rs-latch.bson", "RS latch");
                        if (ImGui::MenuItem("Full adder"))
                            blocks.load_example(&camera, "data/examples/full-adder.bson", "Full adder");
                        if (ImGui::MenuItem("4-bit adder"))
                            blocks.load_example(&camera, "data/examples/adder-4bit.bson", "4-bit adder");
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Graphics")) {
                        ImGui::MenuItem("VSync", nullptr, &vsync);
                        ImGui::MenuItem("Bloom", nullptr, &pipeline.bloom);
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Help")) {
                        if (ImGui::MenuItem("Itch.io"))
                            openUrl("https://kewldan.itch.io/logical-system");
                        if (ImGui::MenuItem("Source code"))
                            openUrl("https://github.com/kewldan/LogicalSystemRemaster");
                        static const std::string versionString = std::format("Version: {} ({})", LS_VERSION, __DATE__);
                        ImGui::MenuItem(versionString.c_str(), nullptr, nullptr, false);
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
                             "Wire straight\0Wire angled right\0Wire angled left\0Wire T\0Wire cross\0Wire 2\0Wire 3\0NOT\0AND\0NAND\0XOR\0NXOR\0Switch\0Clock\0Lamp\0Button\0");
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Use 0-9 for 0-9 elements\nand SHIFT for 10-15 elements");
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

        static std::string lastTitle;
        std::string schemeName = blocks.currentFile.empty() ? "untitled" : blocks.currentFile;
        size_t nameStart = schemeName.find_last_of("/\\");
        if (nameStart != std::string::npos) schemeName.erase(0, nameStart + 1);
        std::string title = std::format("Logical system - {}{}", schemeName, blocks.dirty ? "*" : "");
        if (title != lastTitle) {
            window.setTitle(title.c_str());
            lastTitle = title;
        }

        if (!window.update()) {
            if (blocks.dirty) {
                glfwSetWindowShouldClose(window.getId(), GLFW_FALSE);
                if (pending == PendingAction::None) pending = PendingAction::Exit;
            } else {
                running = false;
            }
        }
    }

    settings.width = window.width;
    settings.height = window.height;
    settings.tps = blocks.TPS;
    settings.vsync = vsync;
    settings.bloom = pipeline.bloom;
    saveSettings(settings);

    Engine::HUD::destroy();
    NFD_Quit();
    return 0;
}
