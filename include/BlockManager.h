#pragma once

#include "Block.h"
#include <unordered_map>
#include "Engine.h"
#include <thread>
#include <mutex>
#include "File.h"
#include "Base64.h"
#include "Window.h"
#include <json.hpp>
#include "HUD.h"
#include "Camera2D.h"

typedef std::unordered_map<long long, Block *> Blocks;

struct BlockInfo {
public:
    float info;
    glm::mat4 mvp{};

    BlockInfo(int id, bool active, bool selection, glm::mat4 mat) {
        int i = id << 2;
        if (active) i |= 2;
        if (selection) i |= 1;
        info = static_cast<float>(i);
        this->mvp = mat;
    }

    BlockInfo() {
        info = 0.f;
        mvp = glm::mat4(1);
    };
};

class BlockManager {
private:
    BlockInfo *info;
    void thread_tick();
public:
    unsigned int atlas{}, VAO{}, *VBO;
    Blocks blocks;
    BlockType *types;
    bool simulate;
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

    void draw(Engine::Camera2D* camera);

    bool save(Engine::Camera2D *camera, const char *path);

    bool load(Engine::Camera2D *camera, const char *path);

    void load_from_memory(Engine::Camera2D *camera, const char *data, int length, bool is_bson = false);

    void load_example(Engine::Camera2D* camera, const char *path);

    void select_all();

    void delete_selected();

    void copy(int blockX, int blockY);

    void paste(int blockX, int blockY);

    void cut(int blockX, int blockY);
};
