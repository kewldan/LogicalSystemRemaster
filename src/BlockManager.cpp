#include "BlockManager.h"

BlockManager::BlockManager(Engine::Window *window, const float vertices[], int count) {
    glGenTextures(1, &atlas);

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
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(BlockInfo), (void *) nullptr);
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

    glVertexAttribDivisor(2, 1);
    glVertexAttribDivisor(3, 1);
    glVertexAttribDivisor(4, 1);
    glVertexAttribDivisor(5, 1);
    glVertexAttribDivisor(6, 1);

    types = new BlockType[15];
    types[0] = BlockType(0x0, [](int c) { return c > 0; });
    types[1] = BlockType(0x1, [](int c) { return c > 0; });
    types[2] = BlockType(0x2, [](int c) { return c > 0; });
    types[3] = BlockType(0x3, [](int c) { return c > 0; });
    types[4] = BlockType(0x4, [](int c) { return c > 0; });
    types[5] = BlockType(0x5, [](int c) { return c > 0; });
    types[6] = BlockType(0x6, [](int c) { return c > 0; });
    types[7] = BlockType(0x7, [](int c) { return c > 0; });
    types[8] = BlockType(0x8, [](int c) { return c >= 2; });
    types[9] = BlockType(0x9, [](int c) { return c >= 2; });
    types[10] = BlockType(0xA, [](int c) { return c % 2 == 1; });
    types[11] = BlockType(0xB, [](int c) { return c % 2 == 1; });
    types[12] = BlockType(0xC, [](int c) { return false; });
    types[13] = BlockType(0xD, [](int c) { return c > 0; });
    types[14] = BlockType(0xE, [](int c) { return c > 0; });

    thread = std::thread(&BlockManager::thread_tick, this);
}

void BlockManager::set(int x, int y, Block *block) {
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
    Block *block = get(x, y);
    block->rotation = rotation;
    block->updateMvp(x << 5, y << 5);
}

int BlockManager::length() {
    return (int) blocks.size();
}

void BlockManager::update() {
    const std::lock_guard<std::mutex> lock(mutex);
    for (auto &it: blocks) {
        Block *block = it.second;
        int x = Block_X(it.first);
        int y = Block_Y(it.first);
        BlockRotation r = block->rotation;

        if (!block->active) { // Not family
            if (block->type->id == types[7].id || block->type->id == types[9].id ||
                block->type->id == types[11].id) { // Not, Nand, Nxor
                if (!block->active) setActive(x, y, r);
            }
        }

        if (block->type->id == types[13].id) { // Timer
            block->active ^= 1;
            if (block->active) setActive(x, y, r);
        }

        if (block->active) {
            if (block->type->id == types[0].id || block->type->id == types[8].id ||
                block->type->id == types[10].id) { // Wire, And, Xor
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
        Block *block = it.second;
        if (block->type->id != 13 && block->type->id != 12) { // Except timer
            block->active = block->type->isActive(block->connections);
        }
        block->connections = 0;
    }
}

void BlockManager::setActive(int x, int y) {
    if (has(x, y)) {
        get(x, y)->connections += 1;
    }
}

void BlockManager::setActive(int x, int y, BlockRotation rotation) {
    setActive(x, y, rotation, 1);
}

void BlockManager::setActive(int x, int y, BlockRotation rotation, int l) {
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

void BlockManager::draw(int count) const {
    glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (long long) sizeof(BlockInfo) * count, info);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, count);
}

bool BlockManager::save(Engine::Camera *camera, const char *path) {
    if(Engine::File::exists(path)) return false;
    char *bin = new char[blocks.size() * 11 + 4];
    memcpy(bin, &camera->position.x, 4);
    memcpy(bin + 4, &camera->position.y, 4);
    float zoom = camera->getZoom();
    memcpy(bin + 8, &zoom, 4);
    int size = (int) blocks.size();
    memcpy(bin + 12, &size, 4);

    int o = 16;
    for (auto &block: blocks) {
        block.second->write(bin + o, block.first);
        o += 11;
    }
    bool ok = Engine::File::writeFile(path, bin, size * 11 + 16);
    return ok;
}

bool BlockManager::load(Engine::Camera *camera, const char *path) {
    const char *bin = Engine::File::readFile(path);
    if (!bin) return false;
    load_from_memory(camera, bin);
    delete[] bin;
    return true;
}

void BlockManager::thread_tick() {
    static double lastUpdate = 0;
    static double elapsed = 0;
    while (!glfwWindowShouldClose(window->getId())) {
        if (glfwGetTime() > lastUpdate + (1. / TPS) && simulate) {
            elapsed = glfwGetTime();
            update();
            tickTime = glfwGetTime() - elapsed;
            lastUpdate = glfwGetTime();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

void BlockManager::copy(int selected, int blockX, int blockY) {
    auto *b = (unsigned char *) malloc(selected * 11);
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
    auto *deflated = Engine::File::compress(b, selected * 11, &length);

    const std::string exportString = Base64::base64_encode(deflated, length);
    glfwSetClipboardString(window->getId(), exportString.c_str());
}

void BlockManager::cut(int selected, int blockX, int blockY) {
    copy(selected, blockX, blockY);
    delete_selected();
}

int BlockManager::paste(int blockX, int blockY) {
    const char *importString = glfwGetClipboardString(window->getId());
    std::vector<BYTE> bytes = Base64::base64_decode(std::string(importString));
    if (bytes.size() > 4) {
        unsigned long length = 0;
        auto *inflated = Engine::File::decompress(bytes.data(), bytes.size(), &length);

        if (length % 11 == 0) {
            unsigned long count = length / 11UL;
            long long pos = 0;
            for (unsigned long i = 0; i < count; i++) {
                auto *block = new Block(reinterpret_cast<char *>(inflated) + i * 11UL, types, &pos);
                int x = Block_X(pos) + blockX;
                int y = Block_Y(pos) + blockY;
                blocks[Block_TO_LONG(x, y)] = block;
                block->updateMvp(x << 5, y << 5);
            }
            return (int) count;
        } else {
            return -1;
        }
    } else {
        return -1;
    }
}

void BlockManager::select_all() {
    for (auto &it: blocks) {
        it.second->selected = true;
    }
}

void BlockManager::delete_selected() {
    Blocks b = blocks;
    for (auto &it: b) {
        if (it.second->selected) blocks.erase(it.first);
    }
}

void BlockManager::load_from_memory(Engine::Camera *camera, const char *data) {
    memcpy(&camera->position.x, data, 4);
    memcpy(&camera->position.y, data + 4, 4);
    float z = 0;
    memcpy(&z, data + 8, 4);
    camera->setZoom(z);
    int size = 0;
    memcpy(&size, data + 12, 4);
    blocks.clear();

    long long pos = 0LL;
    for (int i = 0; i < size; i++) {
        auto *block = new Block(data + i * 11L + 16, types, &pos);
        blocks[pos] = block;
    }
}
