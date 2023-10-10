#pragma once

#include <thread>
#include <mutex>
#include <Window.h>
#include <Camera2D.h>
#include "Block.h"

typedef std::unordered_map<long long, Block *> Blocks;

struct BlockInfo {
public:
    int info;
    glm::mat4 mvp{};

    BlockInfo(BlockId id, bool active, bool selection, glm::mat4 mat) {
        info = id << 2;
        if (active) info |= 2;
        if (selection) info |= 1;
        this->mvp = mat;
    }

    BlockInfo() {
        info = 0;
        mvp = glm::mat4(1);
    };
};

class BlockManager {
private:
    std::vector<BlockInfo> info;

    void thread_tick();

public:
    unsigned int atlas{}, VAO{}, VBO[2];
    Blocks blocks;
    BlockType *types;
    bool simulate = true;
    int TPS, selectedBlocks{};
    double tickTime{};
    std::thread thread;
    std::mutex mutex;
    Engine::Window *window;
    struct PlayerInput {
        int currentBlock;
        int currentRotation;
    } playerInput{};

    BlockManager(Engine::Window *window, const float vertices[], int count);

    void set(int x, int y, Block *block);

    void set(int x, int y);

    Block *get(int x, int y);

    bool has(int x, int y);

    void erase(int x, int y);

    void rotate(int x, int y, BlockRotation rotation);

    int length();

    void update();

    void setActive(int x, int y);

    void setActive(int x, int y, BlockRotation rotation, int l = 1);

    void draw(Engine::Camera2D *camera);

    bool save(Engine::Camera2D *camera, const char *path);

    bool load(Engine::Camera2D *camera, const char *path);

    void load_from_memory(Engine::Camera2D *camera, const char *data, int length, bool is_bson = false);

    void load_example(Engine::Camera2D *camera, const char *path);

    void select_all();

    void delete_selected();

    void copy(int blockX, int blockY, bool notify = true);

    void paste(int blockX, int blockY);

    void cut(int blockX, int blockY);
};
