#pragma once

#include "AppStorage.h"

#include <cstdint>
#include <glm/glm.hpp>
#include <string>

namespace Engine {
    class Window;
    class Input;
}

class EditorCamera;
class RenderPipeline;
class BlockManager;
struct ImGuiIO;

class Application {
public:
    Application() = default;

    void setInitialFile(const std::string &path);

    int run();

private:
    enum class PendingAction {
        None, NewScheme, OpenScheme, OpenRecent, Exit
    };

    Engine::Window *window = nullptr;
    Engine::Input *input = nullptr;
    EditorCamera *camera = nullptr;
    RenderPipeline *pipeline = nullptr;
    BlockManager *blocks = nullptr;
    ImGuiIO *io = nullptr;

    AppStorage storage;
    AppSettings settings;
    std::string initialFile;

    bool vsync = true, hideUI = false, running = true;
    PendingAction pending = PendingAction::None;
    bool painting = false, movingSelection = false, panning = false, panGesture = false;
    bool showRecovery = false, showGhost = false;
    std::string pendingRecentFile;
    int lastPaintX = 0, lastPaintY = 0, moveStartX = 0, moveStartY = 0;
    int blockX = 0, blockY = 0;
    glm::vec2 panStartCursor{}, panStartCamera{}, cursorPosition{};
    float cursorScaleX = 1.f, cursorScaleY = 1.f;
    std::uint64_t lastAutosavedRevision = 0;
    double nextAutosave = 0.0;

    void beginFrame();

    void updateCameraPan();

    void updateZoom();

    void stepSimulation();

    void computeHoveredCell();

    void renderScene();

    void handleInput();

    void handleBoxSelect();

    void autosave();

    void drawGui();

    void drawRecoveryModal();

    void drawUnsavedModal();

    void drawDebugOverlay();

    void drawSimulationPanel();

    void drawMenuBar();

    void drawPalette();

    void drawBlockInspector();

    void updateTitle();

    void handleWindowClose();

    glm::vec2 toFramebuffer(glm::vec2 position) const;

    void paintWire(int fromX, int fromY, int toX, int toY);

    void executeAction(PendingAction action);

    void request(PendingAction action);

    void requestRecent(const std::string &path);

    bool loadSchemePath(const std::string &path);

    void openLoadDialog();

    bool saveScheme(bool forceDialog);
};
