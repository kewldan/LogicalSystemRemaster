#include "BlockManager.h"

BlockManager::BlockManager(Engine::Window *window, const float vertices[], int count) {
    ASSERT("Window is nullptr", window != nullptr);
    ASSERT("Vertices is nullptr", vertices != nullptr);
    ASSERT("Count must be > 0", count > 0);
    glGenTextures(1, &atlas);
    ASSERT("Atlas invalid", atlas > 0);

    glBindTexture(GL_TEXTURE_2D_ARRAY, atlas);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int width, height;

    unsigned char *data = Engine::Texture::loadImage("data/textures/blocks.png", &width, &height);
    if (data) {
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA,
                     32, 32, 15, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    } else {
        PLOGE << "Failed to load texture [blocks.png]";
        PLOGE << stbi_failure_reason();
    }
    stbi_image_free(data);

    info = new BlockInfo[BLOCK_BATCHING];
    VBO = new unsigned int[2];
    simulate = true;
    this->window = window;
    TPS = 8;

    blocks.reserve(1024 * 1024);
    blocks.max_load_factor(0.25f);

    glGenVertexArrays(1, &VAO);
    glGenBuffers(2, VBO);
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
    glBufferData(GL_ARRAY_BUFFER, count, vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) (3 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(BlockInfo) * BLOCK_BATCHING, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(2, 1, GL_INT, sizeof(BlockInfo), (void *) nullptr);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(BlockInfo), (void *) sizeof(float));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(BlockInfo), (void *) (sizeof(float) + sizeof(glm::vec4)));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(BlockInfo),
                          (void *) (sizeof(float) + 2 * sizeof(glm::vec4)));
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(BlockInfo),
                          (void *) (sizeof(float) + 3 * sizeof(glm::vec4)));
    for (int i = 2; i < 7; i++) {
        glVertexAttribDivisor(i, 1);
    }

    const BlockActivationFunction defaultActivationFunction = [](unsigned char c) { return c > 0; };

    types = new BlockType[15];
    types[0] = BlockType(0x0, defaultActivationFunction); // Straight
    types[1] = BlockType(0x1, defaultActivationFunction); // Right
    types[2] = BlockType(0x2, defaultActivationFunction); // Left
    types[3] = BlockType(0x3, defaultActivationFunction); // Sides
    types[4] = BlockType(0x4, defaultActivationFunction); // Cross
    types[5] = BlockType(0x5, defaultActivationFunction); // Long
    types[6] = BlockType(0x6, defaultActivationFunction); // Very long
    types[7] = BlockType(0x7, [](unsigned char c) { return c == 0; }); // Not
    types[8] = BlockType(0x8, [](unsigned char c) { return c >= 2; }); // AND
    types[9] = BlockType(0x9, [](unsigned char c) { return c < 2; }); // NAND
    types[10] = BlockType(0xA, [](unsigned char c) { return c % 2; }); // XOR
    types[11] = BlockType(0xB, [](unsigned char c) { return !(c % 2); }); // NXOR
    types[12] = BlockType(0xC, [](unsigned char c) { return false; }); // Switch
    types[13] = BlockType(0xD, [](unsigned char c) { return c == 0; }); // Timer
    types[14] = BlockType(0xE, defaultActivationFunction); // Light

    thread = std::thread(&BlockManager::thread_tick, this);
}

void BlockManager::set(int x, int y, Block *block) {
    ASSERT("Block is nullptr", block != nullptr);
    blocks[Block_TO_LONG(x, y)] = block;
}

Block *BlockManager::get(int x, int y) {
    if (has(x, y))
        return blocks[Block_TO_LONG(x, y)];
    else
        return nullptr;
}

bool BlockManager::has(int x, int y) {
    return blocks.contains(Block_TO_LONG(x, y));
}

void BlockManager::erase(int x, int y) {
    if (has(x, y)) {
        blocks.erase(Block_TO_LONG(x, y));
    }
}

void BlockManager::rotate(int x, int y, BlockRotation rotation) {
    ASSERT("Rotation invalid", rotation >= 0 && rotation <= 3);
    Block *block = get(x, y);
    block->rotation = rotation;
    block->updateMvp(x << 5, y << 5);
}

int BlockManager::length() {
    return (int) blocks.size();
}

void BlockManager::update() {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto &it: blocks) {
        ASSERT("Block in map is nullptr", it.second != nullptr);
        Block *block = it.second;
        int x = Block_X(it.first);
        int y = Block_Y(it.first);
        BlockRotation r = block->rotation;

        if (block->type->id == types[13].id) { // Timer
            block->active ^= 1;
            if (block->active) setActive(x, y, r);
        }

        if (block->active) {
            if (
                    block->type->id == types[0].id || block->type->id == types[7].id ||
                    block->type->id == types[8].id || block->type->id == types[9].id ||
                    block->type->id == types[10].id || block->type->id == types[11].id
                    ) { // Wire, And, Xor
                setActive(x, y, r);
            } else if (block->type->id == types[5].id) { // 2 wire
                setActive(x, y, r, 2);
            } else if (block->type->id == types[6].id) { // 3 wire
                setActive(x, y, r, 3);
            } else if (block->type->id == types[1].id) {
                setActive(x, y, r);
                setActive(x, y, rotateBlock(r, 1));
            } else if (block->type->id == types[2].id) {
                setActive(x, y, r);
                setActive(x, y, rotateBlock(r, -1));
            } else if (block->type->id == types[3].id) {
                setActive(x, y, rotateBlock(r, -1));
                setActive(x, y, rotateBlock(r, 1));
            } else if (block->type->id == types[4].id) {
                setActive(x, y, rotateBlock(r, -1));
                setActive(x, y, r);
                setActive(x, y, rotateBlock(r, 1));
            } else if (block->type->id == types[12].id) { // Button
                setActive(x + 1, y);
                setActive(x, y - 1);
                setActive(x, y + 1);
                setActive(x - 1, y);
                setActive(x, y);
            }
        }
    }

    for (auto &it: blocks) {
        if (it.second->type->id != 13 && it.second->type->id != 12) { // Except timer
            it.second->active = it.second->type->isActive(it.second->connections);
        }
        it.second->connections = 0;
    }
}

void BlockManager::setActive(int x, int y) {
    if (has(x, y)) {
        get(x, y)->connections++;
    }
}

void BlockManager::setActive(int x, int y, BlockRotation rotation, int l) {
    ASSERT("Block length must be > 0", l > 0);
    if (rotation == 0) {
        setActive(x, y + l);
    } else if (rotation == 1) {
        setActive(x + l, y);
    } else if (rotation == 2) {
        setActive(x, y - l);
    } else if (rotation == 3) {
        setActive(x - l, y);
    }
}

bool BlockManager::save(Engine::Camera2D *camera, const char *path) {
    if (Engine::Filesystem::exists(path)) return false;
    ASSERT("Path is nullptr", path != nullptr);
    nlohmann::json saveFile;
    saveFile["camera"]["position"]["x"] = camera->position.x;
    saveFile["camera"]["position"]["y"] = camera->position.y;
    saveFile["camera"]["zoom"] = camera->getZoom();
    saveFile["meta"]["version"] = 1;
    saveFile["meta"]["timestamp"] = duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    int j = 0;
    for (auto &it: blocks) {
        saveFile["blocks"][j]["pos"] = it.first;
        saveFile["blocks"][j]["type"] = it.second->type->id;
        saveFile["blocks"][j]["rotation"] = it.second->rotation;
        saveFile["blocks"][j]["active"] = it.second->active;
        j++;
    }
    auto binary = nlohmann::json::to_bson(saveFile);
    bool ok = Engine::Filesystem::writeFile(path, reinterpret_cast<const char *>(binary.data()), binary.size());
    return ok;
}

inline bool ends_with(const char *value, const char *ending) {
    if (strlen(value) >= strlen(ending)) {
        return memcmp(value + strlen(value) - strlen(ending), ending, strlen(ending)) == 0;
    } else {
        return false;
    }
}

bool BlockManager::load(Engine::Camera2D *camera, const char *path) {
    ASSERT("Path is nullptr", path != nullptr);
    int size = 0;
    const char *bin = Engine::Filesystem::readFile(path, &size);
    if (!bin) return false;
    load_from_memory(camera, bin, size, ends_with(path, ".bson"));
    delete[] bin;
    return true;
}

void BlockManager::thread_tick() {
    auto lastUpdate = std::chrono::system_clock::now();
    while (!glfwWindowShouldClose(window->getId())) {
        auto n = std::chrono::system_clock::now();
        if (n >= lastUpdate + std::chrono::milliseconds(static_cast<int>(floor(1000.f / (float) TPS)))) {
            update();
            tickTime = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now() - n).count());
            lastUpdate = n;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void BlockManager::copy(int blockX, int blockY) {
    auto *b = (unsigned char *) malloc(selectedBlocks * 11);
    int o = 0;
    for (auto &it: blocks) {
        if (it.second->selected) {
            int x = Block_X(it.first) - blockX;
            int y = Block_Y(it.first) - blockY;
            it.second->write(reinterpret_cast<char *>(b) + o, Block_TO_LONG(x, y));
            o += 11;
        }
    }

    unsigned long length = 0;
    auto *deflated = Engine::Filesystem::compress(b, selectedBlocks * 11, &length);

    size_t b64len = tb64enclen(length);
    auto *buf = new unsigned char[b64len + 1];
    tb64enc(deflated, length, buf);
    buf[b64len] = 0;
    glfwSetClipboardString(window->getId(), reinterpret_cast<const char *>(buf));

    ImGuiToast toast(ImGuiToastType_Success, 2000);
    toast.set_title("%d blocks copied", selectedBlocks);
    ImGui::InsertNotification(toast);
}

void BlockManager::cut(int blockX, int blockY) {
    copy(blockX, blockY);
    delete_selected();
    ImGuiToast toast(ImGuiToastType_Success, 2000);
    toast.set_title("%d blocks cut", selectedBlocks);
    ImGui::InsertNotification(toast);
}

void BlockManager::paste(int blockX, int blockY) {
    const char *importString = glfwGetClipboardString(window->getId());
    unsigned long count;
    auto *bytes = new unsigned char[tb64declen(reinterpret_cast<const unsigned char *>(importString),
                                               strlen(importString))];
    size_t written = tb64dec(reinterpret_cast<const unsigned char *>(importString), strlen(importString), bytes);
    if (written > 4) {
        unsigned long length = 0;
        auto *inflated = Engine::Filesystem::decompress(bytes, written, &length);

        if (length % 11 == 0) {
            count = length / 11UL;
            long long pos = 0;
            for (unsigned long i = 0; i < count; i++) {
                auto *block = new Block(reinterpret_cast<char *>(inflated) + i * 11UL, types, &pos);
                int x = Block_X(pos) + blockX;
                int y = Block_Y(pos) + blockY;
                blocks[Block_TO_LONG(x, y)] = block;
                block->updateMvp(x << 5, y << 5);
            }
        } else {
            count = -1;
        }
    } else {
        count = -1;
    }

    ImGuiToast toast(0);
    if (count != -1) {
        toast.set_type(ImGuiToastType_Success);
        toast.set_title("%d blocks pasted", count);
    } else {
        toast.set_type(ImGuiToastType_Error);
        toast.set_title("Failed to paste");
    }
    ImGui::InsertNotification(toast);
}

void BlockManager::select_all() {
    for (auto &it: blocks) {
        it.second->selected = true;
    }
    selectedBlocks = length();
    ImGuiToast toast(ImGuiToastType_Info, 2000);
    toast.set_title("%d blocks selected", selectedBlocks);
    ImGui::InsertNotification(toast);
}

void BlockManager::delete_selected() {
    Blocks b = blocks;
    for (auto &it: b) {
        if (it.second->selected) blocks.erase(it.first);
    }
}

void BlockManager::load_from_memory(Engine::Camera2D *camera, const char *data, int length, bool is_bson) {
    ASSERT("Data is nullptr", data != nullptr);
    blocks.clear();

    float z = 0.f;
    if (is_bson) {
        std::vector<std::uint8_t> v;
        for (int i = 0; i < length; i++) {
            v.push_back(data[i]);
        }
        nlohmann::json loadFile = nlohmann::json::from_bson(v);

        z = loadFile["camera"]["zoom"].get<float>();
        camera->setZoom(z);
        camera->position.x = loadFile["camera"]["position"]["x"].get<float>();
        camera->position.y = loadFile["camera"]["position"]["y"].get<float>();

        for (auto it: loadFile["blocks"]) {
            int x = Block_X(it["pos"].get<long long>());
            int y = Block_Y(it["pos"].get<long long>());
            auto *block = new Block(x, y, &types[it["type"].get<unsigned char>()], it["rotation"].get<BlockRotation>());
            blocks[it["pos"].get<long long>()] = block;
        }
    } else {
        memcpy(&camera->position.x, data, 4);
        memcpy(&camera->position.y, data + 4, 4);
        memcpy(&z, data + 8, 4);
        camera->setZoom(z);
        int size = 0;
        memcpy(&size, data + 12, 4);
        long long pos = 0LL;
        for (int i = 0; i < size; i++) {
            auto *block = new Block(data + i * 11L + 16, types, &pos);
            blocks[pos] = block;
        }
    }
}

void BlockManager::draw(Engine::Camera2D *camera) {
    int j = 0;
    int LB = (int) camera->position.x + (int) camera->left - 16;
    int RB = (int) camera->position.x + (int) camera->right + 16;
    int BB = (int) camera->position.y + (int) camera->bottom - 16;
    int TB = (int) camera->position.y + (int) camera->top + 16;
    for (auto &it: blocks) {
        int x = Block_X(it.first) << 5;
        int y = Block_Y(it.first) << 5;
        if (x > LB && x < RB && y > BB && y < TB) {
            info[j] = BlockInfo(it.second->type->id, it.second->active,
                                it.second->selected,
                                it.second->getMVP());
            j++;
        }
        if (j == BLOCK_BATCHING) {
            glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
            glBufferSubData(GL_ARRAY_BUFFER, 0, (long long) sizeof(BlockInfo) * j, info);
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, j);
            j = 0;
        }
    }
    if (j > 0) {
        glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, (long long) sizeof(BlockInfo) * j, info);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, j);
    }
}

void BlockManager::set(int x, int y) {
    auto *block = new Block(x, y, &types[playerInput.currentBlock],
                            static_cast<BlockRotation>(playerInput.currentRotation));
    set(x, y, block);
}

void BlockManager::load_example(Engine::Camera2D *camera, const char *path) {
    int size = 0;
    auto data = (const char *) Engine::Filesystem::readResourceFile(path, &size);
    if (data) {
        load_from_memory(camera, data, size);
        ImGuiToast toast(ImGuiToastType_Success, 2000);
        toast.set_title("%s loaded successfully", path);
        ImGui::InsertNotification(toast);
    } else {
        ImGuiToast toast(ImGuiToastType_Error, 2000);
        toast.set_title("Failed to load %s", path);
        ImGui::InsertNotification(toast);
    }
}
