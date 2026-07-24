#pragma once

#include <mutex>
#include <thread>
#include <unordered_map>
#include <Window.h>
#include <Camera2D.h>
#include "Block.h"

typedef std::unordered_map<long long, Block> Blocks;

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

class BlockManager {
private:
    BlockInfo *info;

    void thread_tick();

public:
    unsigned int atlas{}, VAO{}, VBO[2];
    Blocks blocks;
    bool simulate = true;
    int TPS, selectedBlocks{};
    double tickTime{};
    std::thread thread;
    std::mutex mutex;
    Engine::Window *window;
    int currentBlock = 0;
    int currentRotation = 0;

    BlockManager(Engine::Window *window, const float vertices[], int count);

    ~BlockManager();

    void set(int x, int y, const Block &block);

    void set(int x, int y);

    Block *get(int x, int y);

    bool has(int x, int y);

    void erase(int x, int y);

    void clear();

    void rotate(int x, int y, BlockRotation rotation);

    int length();

    void update();

    void setActive(int x, int y);

    void setActive(int x, int y, BlockRotation rotation, int l = 1);

    void draw(Engine::Camera2D *camera);

    bool save(Engine::Camera2D *camera, const char *path);

    bool load(Engine::Camera2D *camera, const char *path);

    bool load_from_memory(Engine::Camera2D *camera, const char *data, int length, bool is_bson = false);

    void load_example(Engine::Camera2D *camera, const char *path);

    void select_all();

    void delete_selected();

    void copy(int blockX, int blockY, bool notify = true);

    void paste(int blockX, int blockY);

    void cut(int blockX, int blockY);
};
