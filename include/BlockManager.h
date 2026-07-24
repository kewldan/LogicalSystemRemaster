#pragma once

#include <string>
#include <vector>
#include <Window.h>
#include "EditorCamera.h"
#include "ChunkIndex.h"
#include "Simulation.h"

// Per-instance data uploaded to the GPU: packed state + grid coordinates.
// info bits: 0 - selected, 1 - active, 2..5 - type id, 6..7 - rotation
struct BlockInfo {
public:
    int info;
    int x, y;

    BlockInfo(BlockId id, bool active, bool selection, BlockRotation rotation, int x, int y)
            : info((id << 2) | (active ? 2 : 0) | (selection ? 1 : 0) | (rotation << 6)), x(x), y(y) {
    }

    BlockInfo() : info(0), x(0), y(0) {
    }
};

struct UndoChange {
    long long key{};
    bool hadBefore{}, hasAfter{};
    Block before, after;
};

class BlockManager {
private:
    BlockInfo *info;
    ChunkIndex chunkIndex;
    std::vector<std::vector<UndoChange>> undoStack, redoStack;
    std::vector<UndoChange> pendingChanges;

    void record(long long key, const Block *before, const Block *after);

    void applyChanges(const std::vector<UndoChange> &changes, bool forward);

    void drawInstances(const std::vector<BlockInfo> &list);

public:
    unsigned int atlas{}, VAO{}, VBO[2];
    Circuit circuit;
    Blocks &blocks;
    bool simulate = true;
    int TPS, selectedBlocks{};
    double tickTime{};
    Engine::Window *window;
    int currentBlock = 0;
    int currentRotation = 0;
    bool dirty = false;
    std::string currentFile;
    Blocks pasteBuffer;
    bool pasting = false;

    BlockManager(Engine::Window *window, const float vertices[], int count);

    ~BlockManager();

    void set(int x, int y, const Block &block);

    void set(int x, int y);

    Block *get(int x, int y);

    bool has(int x, int y);

    void erase(int x, int y);

    void clear();

    void rotate(int x, int y, BlockRotation rotation);

    void toggle(int x, int y);

    int length();

    void update();

    void draw(EditorCamera *camera);

    bool save(EditorCamera *camera, const char *path);

    bool load(EditorCamera *camera, const char *path);

    bool load_from_memory(EditorCamera *camera, const char *data, int length, bool is_bson = false);

    void load_example(EditorCamera *camera, const char *path, const char *title);

    void select_all();

    void delete_selected();

    void copy(int blockX, int blockY, bool notify = true);

    void beginPaste();

    void commitPaste(int blockX, int blockY);

    void cancelPaste();

    void cut(int blockX, int blockY);

    void move_selected(int dx, int dy);

    void rotate_selected(int k);

    void deselect_all();

    void drawGhost(int x, int y);

    void drawPasteGhost(int blockX, int blockY);

    void drawSelectionGhost(int dx, int dy);

    void export_scheme();

    void import_scheme(EditorCamera *camera);

    void commitUndo();

    void clearHistory();

    void undo();

    void redo();

    [[nodiscard]] bool canUndo() const;

    [[nodiscard]] bool canRedo() const;
};
