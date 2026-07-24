#include "BlockManager.h"
#include "stb_image.h"

#include <algorithm>
#include <chrono>
#include <imgui_notify.h>
#include <libbase64.h>
#include <nlohmann/json.hpp>

BlockManager::BlockManager(Engine::Window *window, const float vertices[], int count) {
    assert(window != nullptr);
    assert(vertices != nullptr);
    assert(count > 0);
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
                     32, 32, BLOCK_TYPE_COUNT, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    } else {
        PLOGE << "Failed to load texture [blocks.png]";
        PLOGE << stbi_failure_reason();
    }
    stbi_image_free(data);

    this->window = window;
    TPS = 8;

    blocks.max_load_factor(0.5f);
    blocks.reserve(1 << 16);

    info = new BlockInfo[BLOCK_BATCHING];

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
    glVertexAttribIPointer(3, 2, GL_INT, sizeof(BlockInfo), (void *) sizeof(int));
    glVertexAttribDivisor(2, 1);
    glVertexAttribDivisor(3, 1);

    thread = std::thread(&BlockManager::thread_tick, this);
}

BlockManager::~BlockManager() {
    delete[] info;
}

void BlockManager::set(int x, int y, const Block &block) {
    std::lock_guard<std::mutex> lock(mutex);
    blocks[Block_TO_LONG(x, y)] = block;
}

void BlockManager::set(int x, int y) {
    set(x, y, Block(static_cast<BlockId>(currentBlock), static_cast<BlockRotation>(currentRotation)));
}

Block *BlockManager::get(int x, int y) {
    auto it = blocks.find(Block_TO_LONG(x, y));
    return it == blocks.end() ? nullptr : &it->second;
}

bool BlockManager::has(int x, int y) {
    return blocks.contains(Block_TO_LONG(x, y));
}

void BlockManager::erase(int x, int y) {
    std::lock_guard<std::mutex> lock(mutex);
    blocks.erase(Block_TO_LONG(x, y));
}

void BlockManager::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    blocks.clear();
    selectedBlocks = 0;
}

void BlockManager::rotate(int x, int y, BlockRotation rotation) {
    assert(rotation <= 3);
    Block *block = get(x, y);
    if (block != nullptr) {
        block->rotation = rotation;
    }
}

int BlockManager::length() {
    return (int) blocks.size();
}

void BlockManager::update() {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto &it: blocks) {
        Block &block = it.second;
        int x = Block_X(it.first);
        int y = Block_Y(it.first);
        BlockRotation r = block.rotation;

        if (block.typeId == BLOCK_CLOCK) {
            block.active ^= 1;
            if (block.active) setActive(x, y, r);
            continue;
        }

        if (!block.active) continue;

        switch (block.typeId) {
            case 0: // Straight wire
            case 7: // NOT
            case 8: // AND
            case 9: // NAND
            case 10: // XOR
            case 11: // NXOR
                setActive(x, y, r);
                break;
            case 5: // 2 wire
                setActive(x, y, r, 2);
                break;
            case 6: // 3 wire
                setActive(x, y, r, 3);
                break;
            case 1: // Right angled wire
                setActive(x, y, r);
                setActive(x, y, rotateBlock(r, 1));
                break;
            case 2: // Left angled wire
                setActive(x, y, r);
                setActive(x, y, rotateBlock(r, -1));
                break;
            case 3: // T wire
                setActive(x, y, rotateBlock(r, -1));
                setActive(x, y, rotateBlock(r, 1));
                break;
            case 4: // Cross wire
                setActive(x, y, rotateBlock(r, -1));
                setActive(x, y, r);
                setActive(x, y, rotateBlock(r, 1));
                break;
            case BLOCK_SWITCH:
                setActive(x + 1, y);
                setActive(x, y - 1);
                setActive(x, y + 1);
                setActive(x - 1, y);
                setActive(x, y);
                break;
            default:
                break;
        }
    }

    for (auto &it: blocks) {
        Block &block = it.second;
        if (block.typeId != BLOCK_CLOCK && block.typeId != BLOCK_SWITCH) {
            block.active = isBlockActive(block.typeId, block.connections);
        }
        block.connections = 0;
    }
}

void BlockManager::setActive(int x, int y) {
    Block *block = get(x, y);
    if (block != nullptr) {
        block->connections++;
    }
}

void BlockManager::setActive(int x, int y, BlockRotation rotation, int l) {
    assert(l > 0);

    switch (rotation) {
        case 0:
            setActive(x, y + l);
            break;
        case 1:
            setActive(x + l, y);
            break;
        case 2:
            setActive(x, y - l);
            break;
        case 3:
            setActive(x - l, y);
            break;
        default:
            break;
    }
}

bool BlockManager::save(Engine::Camera2D *camera, const char *path) {
    assert(path != nullptr);
    nlohmann::json saveFile;
    saveFile["camera"]["position"]["x"] = camera->position.x;
    saveFile["camera"]["position"]["y"] = camera->position.y;
    saveFile["camera"]["zoom"] = camera->getZoom();
    saveFile["meta"]["version"] = 1;
    saveFile["meta"]["timestamp"] = duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    saveFile["blocks"] = nlohmann::json::array();
    auto &list = saveFile["blocks"];
    for (auto &it: blocks) {
        list.push_back({
                               {"pos",      it.first},
                               {"type",     it.second.typeId},
                               {"rotation", it.second.rotation},
                               {"active",   it.second.active}
                       });
    }
    auto binary = nlohmann::json::to_bson(saveFile);
    return Engine::Filesystem::writeFile(path, reinterpret_cast<const char *>(binary.data()), binary.size());
}

inline bool ends_with(const char *value, const char *ending) {
    if (strlen(value) >= strlen(ending)) {
        return memcmp(value + strlen(value) - strlen(ending), ending, strlen(ending)) == 0;
    } else {
        return false;
    }
}

bool BlockManager::load(Engine::Camera2D *camera, const char *path) {
    assert(path != nullptr);
    int size = 0;
    const char *bin = Engine::Filesystem::readFile(path, &size);
    if (!bin) return false;
    bool ok = load_from_memory(camera, bin, size, ends_with(path, ".bson"));
    delete[] bin;
    return ok;
}

void BlockManager::thread_tick() {
    auto lastUpdate = std::chrono::steady_clock::now();
    while (!glfwWindowShouldClose(window->getId())) {
        int tps = std::clamp(TPS, 1, 1000);
        auto period = std::chrono::microseconds(1000000 / tps);
        auto n = std::chrono::steady_clock::now();
        if (n >= lastUpdate + period && simulate) {
            update();
            tickTime = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - n).count();
            lastUpdate = n;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void BlockManager::copy(int blockX, int blockY, bool notify) {
    size_t count = 0;
    for (auto &it: blocks) {
        count += it.second.selected;
    }
    selectedBlocks = (int) count;
    if (count == 0) {
        if (notify) {
            ImGuiToast toast(ImGuiToastType_Warning, 2000);
            toast.set_title("Nothing to copy");
            ImGui::InsertNotification(toast);
        }
        return;
    }

    auto *b = new unsigned char[count * BLOCK_RECORD_SIZE];
    int o = 0;
    for (auto &it: blocks) {
        if (it.second.selected) {
            int x = Block_X(it.first) - blockX;
            int y = Block_Y(it.first) - blockY;
            it.second.write(reinterpret_cast<char *>(b) + o, Block_TO_LONG(x, y));
            o += BLOCK_RECORD_SIZE;
        }
    }

    unsigned long len = 0;
    auto *deflated = Engine::Filesystem::compress(b, o, &len);
    delete[] b;
    if (deflated == nullptr) {
        if (notify) {
            ImGuiToast toast(ImGuiToastType_Error, 2000);
            toast.set_title("Failed to copy");
            ImGui::InsertNotification(toast);
        }
        return;
    }

    size_t b64cap = ((size_t) len + 2) / 3 * 4 + 1;
    auto *buf = new char[b64cap];
    size_t b64len = 0;
    base64_encode(reinterpret_cast<const char *>(deflated), len, buf, &b64len, 0);
    delete[] deflated;
    buf[b64len] = 0;
    glfwSetClipboardString(window->getId(), buf);
    delete[] buf;

    if (notify) {
        ImGuiToast toast(ImGuiToastType_Success, 2000);
        toast.set_title("%d blocks copied", (int) count);
        ImGui::InsertNotification(toast);
    }
}

void BlockManager::cut(int blockX, int blockY) {
    copy(blockX, blockY, false);
    int count = selectedBlocks;
    delete_selected();
    ImGuiToast toast(ImGuiToastType_Success, 2000);
    toast.set_title("%d blocks cut", count);
    ImGui::InsertNotification(toast);
}

void BlockManager::paste(int blockX, int blockY) {
    const char *importString = glfwGetClipboardString(window->getId());
    long long count = -1;

    if (importString != nullptr && importString[0] != 0) {
        size_t inLength = strlen(importString);
        auto *bytes = new char[inLength / 4 * 3 + 4];
        size_t written = 0;
        if (base64_decode(importString, inLength, bytes, &written, 0) == 1 && written > 0) {
            unsigned long length = 0;
            auto *inflated = Engine::Filesystem::decompress(reinterpret_cast<unsigned char *>(bytes),
                                                            (unsigned int) written, &length);
            if (inflated != nullptr && length > 0 && length % BLOCK_RECORD_SIZE == 0) {
                count = (long long) (length / BLOCK_RECORD_SIZE);
                std::lock_guard<std::mutex> lock(mutex);
                long long pos = 0;
                for (long long i = 0; i < count; i++) {
                    Block block(reinterpret_cast<char *>(inflated) + i * BLOCK_RECORD_SIZE, &pos);
                    blocks[Block_TO_LONG(Block_X(pos) + blockX, Block_Y(pos) + blockY)] = block;
                }
            }
            free(inflated);
        }
        delete[] bytes;
    }

    ImGuiToast toast(0);
    if (count >= 0) {
        toast.set_type(ImGuiToastType_Success);
        toast.set_title("%d blocks pasted", (int) count);
    } else {
        toast.set_type(ImGuiToastType_Error);
        toast.set_title("Failed to paste");
    }
    ImGui::InsertNotification(toast);
}

void BlockManager::select_all() {
    for (auto &it: blocks) {
        it.second.selected = true;
    }
    selectedBlocks = length();
    ImGuiToast toast(ImGuiToastType_Info, 2000);
    toast.set_title("%d blocks selected", selectedBlocks);
    ImGui::InsertNotification(toast);
}

void BlockManager::delete_selected() {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto it = blocks.begin(); it != blocks.end();) {
        if (it->second.selected) {
            it = blocks.erase(it);
        } else {
            ++it;
        }
    }
    selectedBlocks = 0;
}

bool BlockManager::load_from_memory(Engine::Camera2D *camera, const char *data, int length, bool is_bson) {
    assert(data != nullptr);

    Blocks loaded;
    float zoom;
    glm::vec2 position;

    if (is_bson) {
        try {
            nlohmann::json loadFile = nlohmann::json::from_bson(
                    std::vector<std::uint8_t>(data, data + length));

            zoom = loadFile["camera"]["zoom"].get<float>();
            position.x = loadFile["camera"]["position"]["x"].get<float>();
            position.y = loadFile["camera"]["position"]["y"].get<float>();

            for (auto &it: loadFile["blocks"]) {
                auto typeId = it["type"].get<unsigned char>();
                auto rotation = it["rotation"].get<BlockRotation>();
                if (typeId >= BLOCK_TYPE_COUNT || rotation > 3) {
                    PLOGW << "Scheme contains invalid block (type " << (int) typeId << "), skipped";
                    continue;
                }
                Block block(typeId, rotation);
                block.active = it["active"].get<bool>();
                loaded[it["pos"].get<long long>()] = block;
            }
        } catch (const std::exception &e) {
            PLOGE << "Failed to parse scheme: " << e.what();
            return false;
        }
    } else {
        if (length < 16) {
            PLOGE << "Scheme is too short: " << length << " bytes";
            return false;
        }
        memcpy(&position.x, data, 4);
        memcpy(&position.y, data + 4, 4);
        memcpy(&zoom, data + 8, 4);
        int size = 0;
        memcpy(&size, data + 12, 4);
        if (size < 0 || 16LL + (long long) size * BLOCK_RECORD_SIZE > (long long) length) {
            PLOGE << "Scheme is truncated: " << size << " blocks in " << length << " bytes";
            return false;
        }
        long long pos = 0LL;
        for (int i = 0; i < size; i++) {
            Block block(data + (long long) i * BLOCK_RECORD_SIZE + 16, &pos);
            loaded[pos] = block;
        }
    }

    camera->setZoom(zoom);
    camera->position.x = position.x;
    camera->position.y = position.y;
    {
        std::lock_guard<std::mutex> lock(mutex);
        blocks = std::move(loaded);
        selectedBlocks = 0;
    }
    return true;
}

void BlockManager::draw(Engine::Camera2D *camera) {
    int j = 0;
    int LB = (int) camera->position.x + (int) camera->left - 16;
    int RB = (int) camera->position.x + (int) camera->right + 16;
    int BB = (int) camera->position.y + (int) camera->bottom - 16;
    int TB = (int) camera->position.y + (int) camera->top + 16;

    for (auto &it: blocks) {
        int x = Block_X(it.first);
        int y = Block_Y(it.first);
        int px = x << 5;
        int py = y << 5;
        if (px > LB && px < RB && py > BB && py < TB) {
            const Block &block = it.second;
            info[j] = BlockInfo(block.typeId, block.active, block.selected, block.rotation, x, y);
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

void BlockManager::load_example(Engine::Camera2D *camera, const char *path) {
    int size = 0;
    auto data = (const char *) Engine::Filesystem::readResourceFile(path, &size);
    if (data != nullptr && load_from_memory(camera, data, size, true)) {
        ImGuiToast toast(ImGuiToastType_Success, 2000);
        toast.set_title("%s loaded successfully", path);
        ImGui::InsertNotification(toast);
    } else {
        ImGuiToast toast(ImGuiToastType_Error, 2000);
        toast.set_title("Failed to load %s", path);
        ImGui::InsertNotification(toast);
    }
}
