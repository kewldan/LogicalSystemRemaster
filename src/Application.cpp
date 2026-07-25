#include "Application.h"

#include "Window.h"
#include "HUD.h"
#include "BlockCatalog.h"
#include "BlockManager.h"
#include "EditorCamera.h"
#include "RenderPipeline.h"
#include "Input.h"
#ifndef __EMSCRIPTEN__
#include "nfd.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <string>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__EMSCRIPTEN__)
#include <emscripten.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

#if defined(__APPLE__)
#define PRIMARY_SHORTCUT_MODIFIER "Cmd"
#else
#define PRIMARY_SHORTCUT_MODIFIER "Ctrl"
#endif

#ifndef __EMSCRIPTEN__
static const nfdfilteritem_t schemeFilter[] = {{"Logical System schemes", "bson,ls"}};
#endif

#ifdef __EMSCRIPTEN__
static void emscriptenFrame(void *self) {
    static_cast<Application *>(self)->frame();
}
#endif

static float map(float value, float max1, float min2, float max2) {
    return min2 + value * (max2 - min2) / max1;
}

static void setWorkingDirectoryToExecutable() {
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
#elif defined(__linux__)
    char executablePath[4096];
    ssize_t length = readlink("/proc/self/exe", executablePath, sizeof(executablePath) - 1);
    if (length > 0) {
        executablePath[length] = '\0';
        std::filesystem::current_path(std::filesystem::path(executablePath).parent_path());
    }
#endif
}

static void openUrl(const char *url) {
#ifdef _WIN32
    ShellExecuteA(nullptr, nullptr, url, nullptr, nullptr, SW_SHOW);
#elif defined(__APPLE__)
    std::string command = "open \"" + std::string(url) + "\"";
    std::system(command.c_str());
#elif defined(__EMSCRIPTEN__)
    EM_ASM({ window.open(UTF8ToString($0), "_blank"); }, url);
#elif defined(__linux__)
    std::string command = "xdg-open \"" + std::string(url) + "\" &";
    std::system(command.c_str());
#endif
}

void Application::setInitialFile(const std::string &path) {
    initialFile = path;
}

glm::vec2 Application::toFramebuffer(glm::vec2 position) const {
    return glm::vec2(position.x * cursorScaleX, position.y * cursorScaleY);
}

bool Application::loadSchemePath(const std::string &path) {
    auto &blocks = *this->blocks;
    auto &camera = *this->camera;
    ImGuiToast toast(0);
    if (blocks.load(&camera, path.c_str())) {
        storage.rememberRecentFile(settings, path);
        storage.saveSettings(settings);
        storage.clearRecovery();
        toast.set_type(ImGuiToastType_Success);
        toast.set_title("%s loaded successfully", path.c_str());
        ImGui::InsertNotification(toast);
        return true;
    }

    toast.set_type(ImGuiToastType_Error);
    toast.set_title("%s was not loaded!", path.c_str());
    ImGui::InsertNotification(toast);
    return false;
}

void Application::openLoadDialog() {
#ifdef __EMSCRIPTEN__
    ImGuiToast toast(ImGuiToastType_Info, 4000);
    toast.set_title("On the web, open a scheme from the Examples menu or Import from clipboard");
    ImGui::InsertNotification(toast);
#else
    nfdchar_t *loadPath;
    nfdresult_t loadResult = NFD_OpenDialog(&loadPath, schemeFilter, 1, nullptr);

    if (loadResult == NFD_OKAY) {
        loadSchemePath(loadPath);
        NFD_FreePath(loadPath);
    }
#endif
}

bool Application::saveScheme(bool forceDialog) {
    auto &blocks = *this->blocks;
    auto &camera = *this->camera;
    std::string path = blocks.currentFile;
    if (forceDialog || path.empty()) {
#ifdef __EMSCRIPTEN__
        ImGuiToast toast(ImGuiToastType_Info, 4000);
        toast.set_title("On the web, use Export to clipboard to save a scheme");
        ImGui::InsertNotification(toast);
        return false;
#else
        nfdchar_t *savePath = nullptr;
        if (NFD_SaveDialog(&savePath, schemeFilter, 1, nullptr, nullptr) != NFD_OKAY) return false;
        path = savePath;
        NFD_FreePath(savePath);
        if (!std::filesystem::path(path).has_extension()) path += ".bson";
#endif
    }

    bool ok = blocks.save(&camera, path.c_str());
    ImGuiToast toast(0);
    if (ok) {
        storage.rememberRecentFile(settings, path);
        storage.saveSettings(settings);
        storage.clearRecovery();
        toast.set_type(ImGuiToastType_Success);
        toast.set_title("%s saved", path.c_str());
    } else {
        toast.set_type(ImGuiToastType_Error);
        toast.set_title("%s was not saved!", path.c_str());
    }
    ImGui::InsertNotification(toast);
    return ok;
}

void Application::executeAction(PendingAction action) {
    auto &blocks = *this->blocks;
    switch (action) {
        case PendingAction::NewScheme:
            blocks.clear();
            storage.clearRecovery();
            break;
        case PendingAction::OpenScheme:
            openLoadDialog();
            break;
        case PendingAction::OpenRecent:
            if (!pendingRecentFile.empty()) {
                loadSchemePath(pendingRecentFile);
                pendingRecentFile.clear();
            }
            break;
        case PendingAction::Exit:
            storage.clearRecovery();
            running = false;
            break;
        default:
            break;
    }
}

void Application::request(PendingAction action) {
    if (blocks->dirty) {
        pending = action;
    } else {
        executeAction(action);
    }
}

void Application::requestRecent(const std::string &path) {
    pendingRecentFile = path;
    request(PendingAction::OpenRecent);
}

void Application::paintWire(int fromX, int fromY, int toX, int toY) {
    auto &blocks = *this->blocks;
    int x = fromX, y = fromY;
    while (x != toX || y != toY) {
        int dx = x != toX ? (toX > x ? 1 : -1) : 0;
        int dy = dx == 0 ? (toY > y ? 1 : -1) : 0;
        BlockRotation dir = dx == 1 ? 1 : dx == -1 ? 3 : dy == 1 ? 0 : 2;
        Block *prev = blocks.get(x, y);
        if (prev && prev->typeId == BLOCK_WIRE) blocks.rotate(x, y, dir);
        x += dx;
        y += dy;
        if (!blocks.has(x, y)) {
            blocks.set(x, y, Block(BLOCK_WIRE, dir));
        }
    }
}

int Application::run() {
    setWorkingDirectoryToExecutable();
#ifndef __EMSCRIPTEN__
    if (NFD_Init() != NFD_OKAY) return 1;
#endif

    settings = storage.loadSettings();
    vsync = settings.vsync;

    Engine::Window::init();
    Engine::Window window(settings.width, settings.height, "Logical system");
    window.setIcon("data/textures/favicon.png");
    Engine::Input input(window.getId());
    EditorCamera camera(&window);
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
    io = &ImGui::GetIO();

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

    this->window = &window;
    this->input = &input;
    this->camera = &camera;
    this->pipeline = &pipeline;
    this->blocks = &blocks;

    if (!initialFile.empty()) {
        loadSchemePath(initialFile);
    }

    showRecovery = storage.hasRecovery();
    lastAutosavedRevision = blocks.revision();
    nextAutosave = glfwGetTime() + 5.0;

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(emscriptenFrame, this, 0, true);
    return 0;
#else
    while (running) {
        frame();
    }

    settings.width = window.width;
    settings.height = window.height;
    settings.tps = blocks.TPS;
    settings.vsync = vsync;
    settings.bloom = pipeline.bloom;
    storage.saveSettings(settings);

    Engine::HUD::destroy();
    NFD_Quit();
    return 0;
#endif
}

void Application::frame() {
    beginFrame();
    updateCameraPan();
    updateZoom();
    camera->update();
    stepSimulation();
    computeHoveredCell();
    renderScene();
    handleInput();
    handleBoxSelect();
    autosave();
    drawGui();
    updateTitle();
    handleWindowClose();
}

void Application::beginFrame() {
    auto &window = *this->window;
    auto &input = *this->input;
    auto &pipeline = *this->pipeline;

    if (window.isResized()) {
        pipeline.resize(window.width, window.height);
    }

    input.update();
    window.reset();
    window.setVsync(vsync);

    int logicalWidth, logicalHeight;
    glfwGetWindowSize(window.getId(), &logicalWidth, &logicalHeight);
    cursorScaleX = logicalWidth > 0 ? (float) window.width / logicalWidth : 1.f;
    cursorScaleY = logicalHeight > 0 ? (float) window.height / logicalHeight : 1.f;
    cursorPosition = toFramebuffer(input.getCursorPosition());
}

void Application::updateCameraPan() {
    auto &input = *this->input;
    auto &camera = *this->camera;

    panGesture = !io->WantCaptureMouse && input.isKeyPressed(GLFW_KEY_SPACE);
    if (!panning && panGesture && input.isMouseButtonJustPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        panning = true;
        panStartCursor = cursorPosition;
        panStartCamera = glm::vec2(camera.position);
        painting = false;
    }
    if (panning) {
        if (panGesture && input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
            const glm::vec2 delta = cursorPosition - panStartCursor;
            const float viewScale = 2.f * camera.getZoom() - 1.f;
            camera.position.x = panStartCamera.x - delta.x * viewScale;
            camera.position.y = panStartCamera.y + delta.y * viewScale;
        } else {
            panning = false;
        }
    }
}

void Application::updateZoom() {
    auto &input = *this->input;
    auto &camera = *this->camera;
    auto &window = *this->window;

    if (!io->WantCaptureMouse && input.getMouseWheelDelta().y != 0.f) {
        const float anchorX = window.width > 0
                              ? cursorPosition.x / static_cast<float>(window.width) : 0.5f;
        const float anchorY = window.height > 0
                              ? 1.f - cursorPosition.y / static_cast<float>(window.height) : 0.5f;
        camera.addZoomInput(input.getMouseWheelDelta().y, glm::vec2(anchorX, anchorY));
    }
}

void Application::stepSimulation() {
    auto &blocks = *this->blocks;

    static double tickAccumulator = 0.0;
    if (blocks.simulate) {
        tickAccumulator += io->DeltaTime;
        double period = 1.0 / (double) std::clamp(blocks.TPS, 1, 65536);
        int steps = 0;
        while (tickAccumulator >= period && steps < 8192) {
            blocks.update();
            tickAccumulator -= period;
            steps++;
        }
        if (steps == 8192) tickAccumulator = 0.0;
    } else {
        tickAccumulator = 0.0;
    }
}

void Application::computeHoveredCell() {
    auto &blocks = *this->blocks;
    auto &camera = *this->camera;
    auto &window = *this->window;
    auto &input = *this->input;

    float cursorX = map(cursorPosition.x, (float) window.width, camera.left, camera.right);
    float cursorY = map(cursorPosition.y, (float) window.height, camera.bottom, camera.top);
    blockX = (int) floorf((cursorX + camera.position.x) / 32.f +
                          0.5f);
    blockY = (int) floorf((((float) window.height - cursorY) + camera.position.y) / 32.f +
                          0.5f);

    showGhost = !io->WantCaptureMouse && !hideUI && !blocks.pasting && !movingSelection && !panning &&
                !input.isDragging() && !blocks.has(blockX, blockY);
}

void Application::renderScene() {
    auto &blocks = *this->blocks;
    auto &camera = *this->camera;
    auto &pipeline = *this->pipeline;

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
}

void Application::handleInput() {
    auto &blocks = *this->blocks;
    auto &camera = *this->camera;
    auto &input = *this->input;

    if (io->WantCaptureKeyboard) return;

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
            saveScheme(input.isKeyPressed(GLFW_KEY_LEFT_SHIFT));
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
        if (input.isKeyJustPressed(GLFW_KEY_F)) {
            const bool selectedOnly = input.isKeyPressed(GLFW_KEY_LEFT_SHIFT) && blocks.selectedBlocks > 0;
            auto target = blocks.bounds(selectedOnly);
            if (!target && selectedOnly) target = blocks.bounds(false);
            if (target) {
                camera.frameWorldBounds(target->minX * 32.f - 16.f,
                                        target->minY * 32.f - 16.f,
                                        target->maxX * 32.f + 16.f,
                                        target->maxY * 32.f + 16.f);
            }
        }
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

    if (!io->WantCaptureMouse && !panning && !panGesture) {
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
                if (painting && blocks.currentBlock == BLOCK_WIRE &&
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

void Application::handleBoxSelect() {
    auto &input = *this->input;
    auto &camera = *this->camera;
    auto &window = *this->window;
    auto &pipeline = *this->pipeline;
    auto &blocks = *this->blocks;

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
}

void Application::autosave() {
    auto &blocks = *this->blocks;
    auto &camera = *this->camera;

    const double now = glfwGetTime();
    if (now >= nextAutosave) {
        nextAutosave = now + 5.0;
        if (blocks.dirty && blocks.revision() != lastAutosavedRevision) {
            if (blocks.saveSnapshot(&camera, storage.recoveryPath().string().c_str())) {
                lastAutosavedRevision = blocks.revision();
            }
        }
    }
}

void Application::drawGui() {
    Engine::HUD::begin();
    drawRecoveryModal();
    drawUnsavedModal();
    if (!hideUI) {
        drawDebugOverlay();
        drawSimulationPanel();
    }
    if (!hideUI && !io->WantCaptureMouse) {
        drawBlockInspector();
    }
    Engine::HUD::end();
}

void Application::drawRecoveryModal() {
    auto &blocks = *this->blocks;
    auto &camera = *this->camera;

    if (showRecovery && !ImGui::IsPopupOpen("Recover autosave")) {
        ImGui::OpenPopup("Recover autosave");
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Recover autosave", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("An autosaved scheme from the previous session was found.");
        ImGui::TextUnformatted("Recover it or discard the recovery copy.");
        ImGui::Spacing();
        if (ImGui::Button("Recover")) {
            if (blocks.load(&camera, storage.recoveryPath().string().c_str())) {
                blocks.currentFile.clear();
                blocks.dirty = true;
                showRecovery = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard")) {
            storage.clearRecovery();
            showRecovery = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void Application::drawUnsavedModal() {
    if (pending != PendingAction::None && !ImGui::IsPopupOpen("Unsaved changes")) {
        ImGui::OpenPopup("Unsaved changes");
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Unsaved changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("The current scheme has unsaved changes.");
        ImGui::Spacing();
        if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save")) {
            if (saveScheme(false)) {
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
            blocks->dirty = false;
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
}

void Application::drawDebugOverlay() {
    auto &blocks = *this->blocks;

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
}

void Application::drawSimulationPanel() {
    auto &blocks = *this->blocks;

    ImGui::SetNextWindowPos(ImVec2(5, 50), ImGuiCond_Once);
    if (ImGui::Begin("Simulation", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_MenuBar)) {
        if (ImGui::BeginMenuBar()) {
            drawMenuBar();
            ImGui::EndMenuBar();
        }
        ImGui::Checkbox("Play", &blocks.simulate);
        if (!blocks.simulate) {
            ImGui::SameLine();
            if (ImGui::Button("Tick")) {
                blocks.update();
            }
        }
        ImGui::SliderInt("TPS", &blocks.TPS, 2, 65536, "%d",
                         ImGuiSliderFlags_Logarithmic);
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("Simulation ticks per second (use Turbo rates for large computers)");
            ImGui::EndTooltip();
        }
        ImGui::SeparatorText("Blocks");
        drawPalette();
        const auto &currentDescription = describeBlock(static_cast<BlockId>(blocks.currentBlock));
        ImGui::TextUnformatted(currentDescription.name);
        ImGui::TextDisabled("0-9 select blocks; hold Shift for 10-15");
        ImGui::Combo("Rotation", &blocks.currentRotation, "Up\0Right\0Down\0Left\0");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("Use R for rotate clockwise\nand SHIFT for anti-clockwise");
            ImGui::EndTooltip();
        }
    }
    ImGui::End();
}

void Application::drawMenuBar() {
    auto &blocks = *this->blocks;
    auto &camera = *this->camera;
    auto &pipeline = *this->pipeline;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem(ICON_FA_FILE "  New", PRIMARY_SHORTCUT_MODIFIER " + N"))
            request(PendingAction::NewScheme);
        if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Open", PRIMARY_SHORTCUT_MODIFIER " + O"))
            request(PendingAction::OpenScheme);
        if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK "  Save", PRIMARY_SHORTCUT_MODIFIER " + S"))
            saveScheme(false);
        if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK "  Save As...", PRIMARY_SHORTCUT_MODIFIER " + Shift + S"))
            saveScheme(true);
        if (ImGui::BeginMenu("Recent files", !settings.recentFiles.empty())) {
            const auto recentFiles = settings.recentFiles;
            for (const std::string &recent: recentFiles) {
                std::error_code fileError;
                const bool exists = std::filesystem::is_regular_file(recent, fileError);
                const std::string label = std::filesystem::path(recent).filename().string();
                if (ImGui::MenuItem(label.c_str(), nullptr, false, exists)) {
                    requestRecent(recent);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", recent.c_str());
            }
            ImGui::EndMenu();
        }
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
        if (ImGui::MenuItem("8-bit click adder + decimal display"))
            blocks.load_example(&camera, "data/examples/click-adder.bson",
                                "8-bit click adder + decimal display");
        if (ImGui::MenuItem("16-bit SUBLEQ + 1 KiB RAM"))
            blocks.load_example(&camera, "data/examples/cpu16-1k.bson",
                                "16-bit SUBLEQ + 1 KiB RAM");
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
}

void Application::drawPalette() {
    auto &blocks = *this->blocks;

    const ImTextureRef palette(static_cast<ImTextureID>(blocks.paletteTexture));
    int hoveredPaletteBlock = -1;
    for (int id = 0; id < BLOCK_TYPE_COUNT; id++) {
        if (id % 4 != 0) ImGui::SameLine();
        const bool selected = blocks.currentBlock == id;
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        const float v0 = static_cast<float>(id) / BLOCK_TYPE_COUNT;
        const float v1 = static_cast<float>(id + 1) / BLOCK_TYPE_COUNT;
        const std::string buttonId = std::format("##block-{}", id);
        if (ImGui::ImageButton(buttonId.c_str(), palette, ImVec2(32, 32),
                               ImVec2(0, v0), ImVec2(1, v1))) {
            blocks.currentBlock = id;
        }
        if (selected) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) hoveredPaletteBlock = id;
    }
    if (hoveredPaletteBlock >= 0) {
        const auto &description = describeBlock(static_cast<BlockId>(hoveredPaletteBlock));
        constexpr float tooltipWidth = 280.f;
        ImVec2 tooltipPosition = ImGui::GetMousePos();
        tooltipPosition.x += 18.f;
        tooltipPosition.y += 18.f;
        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        tooltipPosition.x = std::min(tooltipPosition.x,
                                     viewport->WorkPos.x + viewport->WorkSize.x - tooltipWidth);
        tooltipPosition.y = std::min(tooltipPosition.y,
                                     viewport->WorkPos.y + viewport->WorkSize.y - 120.f);
        ImGui::SetNextWindowPos(tooltipPosition, ImGuiCond_Always);
        ImGui::SetNextWindowSizeConstraints(ImVec2(tooltipWidth, 0.f),
                                            ImVec2(tooltipWidth, 1000.f));
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(description.name);
        ImGui::Separator();
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + tooltipWidth - 24.f);
        ImGui::TextUnformatted(description.rule);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void Application::drawBlockInspector() {
    auto &blocks = *this->blocks;
    auto &input = *this->input;

    if (const Block *hovered = blocks.get(blockX, blockY)) {
        static constexpr const char *rotations[] = {"Up", "Right", "Down", "Left"};
        const auto &description = describeBlock(hovered->typeId);
        const glm::vec2 logicalCursor = input.getCursorPosition();
        ImGui::SetNextWindowPos(ImVec2(logicalCursor.x + 18.f, logicalCursor.y + 18.f),
                                ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.92f);
        if (ImGui::Begin("##Block inspector", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs)) {
            ImGui::TextUnformatted(description.name);
            ImGui::TextDisabled("Cell %d, %d", blockX, blockY);
            ImGui::Separator();
            ImGui::Text("State: %s", hovered->active ? "ON" : "OFF");
            ImGui::Text("Direction: %s", rotations[hovered->rotation & 3]);
            ImGui::TextWrapped("%s", description.rule);
        }
        ImGui::End();
    }
}

void Application::updateTitle() {
    auto &blocks = *this->blocks;
    auto &window = *this->window;

    static std::string lastTitle;
    std::string schemeName = blocks.currentFile.empty() ? "untitled" : blocks.currentFile;
    size_t nameStart = schemeName.find_last_of("/\\");
    if (nameStart != std::string::npos) schemeName.erase(0, nameStart + 1);
    std::string title = std::format("Logical system - {}{}", schemeName, blocks.dirty ? "*" : "");
    if (title != lastTitle) {
        window.setTitle(title.c_str());
        lastTitle = title;
    }
}

void Application::handleWindowClose() {
    auto &blocks = *this->blocks;
    auto &window = *this->window;

    if (!window.update()) {
        if (blocks.dirty) {
            glfwSetWindowShouldClose(window.getId(), GLFW_FALSE);
            if (pending == PendingAction::None) pending = PendingAction::Exit;
        } else {
            running = false;
        }
    }
}
