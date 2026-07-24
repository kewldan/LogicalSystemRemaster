#include "BlockManager.h"
#include "Scheme.h"
#include "stb_image.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <filesystem>
#include <imgui_notify.h>
#include <libbase64.h>
#include <vector>

static constexpr size_t UNDO_LIMIT = 200;

BlockManager::BlockManager(Engine::Window *window, const float vertices[], int count) : blocks(circuit.blocks) {
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

        glGenTextures(1, &paletteTexture);
        glBindTexture(GL_TEXTURE_2D, paletteTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        std::vector<unsigned char> paletteData(data, data + width * height * 4);
        constexpr float symbolColor[3] = {235.f, 79.f, 51.f};
        for (std::size_t i = 0; i < paletteData.size(); i += 4) {
            // Atlas alpha is a material mask consumed by the world shader,
            // not display transparency. Preserve the atlas artwork where the
            // mask is opaque, and replace its transparent symbol with the
            // palette blue before forcing the finished thumbnail opaque.
            const float mask = static_cast<float>(paletteData[i + 3]) / 255.f;
            for (int channel = 0; channel < 3; channel++) {
                paletteData[i + channel] = static_cast<unsigned char>(
                        symbolColor[channel] +
                        (static_cast<float>(paletteData[i + channel]) - symbolColor[channel]) * mask);
            }
            paletteData[i + 3] = 255;
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, paletteData.data());
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
}

BlockManager::~BlockManager() {
    delete[] info;
    glDeleteTextures(1, &paletteTexture);
    glDeleteTextures(1, &atlas);
    glDeleteBuffers(2, VBO);
    glDeleteVertexArrays(1, &VAO);
}

void BlockManager::record(long long key, const Block *before, const Block *after) {
    UndoChange change;
    change.key = key;
    change.hadBefore = before != nullptr;
    change.hasAfter = after != nullptr;
    if (before) change.before = *before;
    if (after) change.after = *after;
    pendingChanges.push_back(change);
    dirty = true;
    editRevision++;
}

void BlockManager::commitUndo() {
    if (pendingChanges.empty()) return;
    undoStack.push_back(std::move(pendingChanges));
    pendingChanges.clear();
    if (undoStack.size() > UNDO_LIMIT) {
        undoStack.erase(undoStack.begin());
    }
    redoStack.clear();
}

void BlockManager::clearHistory() {
    undoStack.clear();
    redoStack.clear();
    pendingChanges.clear();
}

void BlockManager::applyChanges(const std::vector<UndoChange> &changes, bool forward) {
    auto apply = [this](const UndoChange &change, bool fwd) {
        bool has = fwd ? change.hasAfter : change.hadBefore;
        const Block &value = fwd ? change.after : change.before;
        if (has) {
            blocks[change.key] = value;
        } else {
            blocks.erase(change.key);
        }
    };
    if (forward) {
        for (const auto &change: changes) apply(change, true);
    } else {
        for (auto it = changes.rbegin(); it != changes.rend(); ++it) apply(*it, false);
    }
}

void BlockManager::undo() {
    if (undoStack.empty()) return;
    applyChanges(undoStack.back(), false);
    redoStack.push_back(std::move(undoStack.back()));
    undoStack.pop_back();
    chunkIndex.rebuild(blocks);
    circuit.rebuild();
    dirty = true;
    editRevision++;
}

void BlockManager::redo() {
    if (redoStack.empty()) return;
    applyChanges(redoStack.back(), true);
    undoStack.push_back(std::move(redoStack.back()));
    redoStack.pop_back();
    chunkIndex.rebuild(blocks);
    circuit.rebuild();
    dirty = true;
    editRevision++;
}

bool BlockManager::canUndo() const {
    return !undoStack.empty();
}

bool BlockManager::canRedo() const {
    return !redoStack.empty();
}

void BlockManager::set(int x, int y, const Block &block) {
    long long key = Block_TO_LONG(x, y);
    auto it = blocks.find(key);
    record(key, it != blocks.end() ? &it->second : nullptr, &block);
    blocks[key] = block;
    chunkIndex.insert(key);
    circuit.invalidate(x, y);
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
    long long key = Block_TO_LONG(x, y);
    auto it = blocks.find(key);
    if (it == blocks.end()) return;
    record(key, &it->second, nullptr);
    blocks.erase(it);
    chunkIndex.erase(key);
    circuit.invalidate(x, y);
}

void BlockManager::clear() {
    for (auto &it: blocks) {
        record(it.first, &it.second, nullptr);
    }
    blocks.clear();
    chunkIndex.rebuild(blocks);
    circuit.rebuild();
    commitUndo();
    selectedBlocks = 0;
    currentFile.clear();
    dirty = false;
}

void BlockManager::rotate(int x, int y, BlockRotation rotation) {
    assert(rotation <= 3);
    long long key = Block_TO_LONG(x, y);
    auto it = blocks.find(key);
    if (it == blocks.end()) return;
    Block after = it->second;
    after.rotation = rotation;
    record(key, &it->second, &after);
    it->second = after;
    circuit.invalidate(x, y);
}

void BlockManager::toggle(int x, int y) {
    Block *block = get(x, y);
    if (block == nullptr) return;
    block->active ^= 1;
    circuit.invalidate(x, y);
}

int BlockManager::length() {
    return (int) blocks.size();
}

void BlockManager::update() {
    auto start = std::chrono::steady_clock::now();
    circuit.tick();
    tickTime = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

bool BlockManager::save(EditorCamera *camera, const char *path) {
    if (!saveSnapshot(camera, path)) return false;
    currentFile = path;
    dirty = false;
    return true;
}

bool BlockManager::saveSnapshot(EditorCamera *camera, const char *path) const {
    assert(path != nullptr);
    std::error_code error;
    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, error);
    if (error) return false;

    SchemeCamera schemeCamera{camera->position.x, camera->position.y, camera->getZoom()};
    auto binary = schemeToBson(blocks, schemeCamera);
    return Engine::Filesystem::writeFile(path, reinterpret_cast<const char *>(binary.data()), binary.size());
}

inline bool ends_with(const char *value, const char *ending) {
    if (strlen(value) >= strlen(ending)) {
        return memcmp(value + strlen(value) - strlen(ending), ending, strlen(ending)) == 0;
    } else {
        return false;
    }
}

bool BlockManager::load(EditorCamera *camera, const char *path) {
    assert(path != nullptr);
    int size = 0;
    const char *bin = Engine::Filesystem::readFile(path, &size);
    if (!bin) return false;
    bool ok = load_from_memory(camera, bin, size, ends_with(path, ".bson"));
    delete[] bin;
    if (ok) {
        currentFile = path;
        dirty = false;
    }
    return ok;
}

bool BlockManager::load_from_memory(EditorCamera *camera, const char *data, int length, bool is_bson) {
    Blocks loaded;
    SchemeCamera schemeCamera;
    if (!schemeFromMemory(data, length, is_bson, loaded, schemeCamera)) {
        return false;
    }
    blocks = std::move(loaded);
    chunkIndex.rebuild(blocks);
    circuit.rebuild();
    clearHistory();
    selectedBlocks = 0;
    camera->setZoom(schemeCamera.zoom);
    camera->position.x = schemeCamera.x;
    camera->position.y = schemeCamera.y;
    return true;
}

void BlockManager::load_example(EditorCamera *camera, const char *path, const char *title) {
    int size = 0;
    auto data = (const char *) Engine::Filesystem::readResourceFile(path, &size);
    if (data != nullptr && load_from_memory(camera, data, size, true)) {
        currentFile.clear();
        dirty = false;
        ImGuiToast toast(ImGuiToastType_Success, 2000);
        toast.set_title("%s loaded", title);
        ImGui::InsertNotification(toast);
    } else {
        ImGuiToast toast(ImGuiToastType_Error, 2000);
        toast.set_title("Failed to load %s", title);
        ImGui::InsertNotification(toast);
    }
}

// Serializes `count` records selected by `filter` into a compressed
// base64 clipboard string. Returns false when there is nothing to share
// or compression fails.
static bool blocksToClipboard(GLFWwindow *window, const Blocks &blocks, long long originX, long long originY,
                              bool selectedOnly, size_t count) {
    if (count == 0) return false;

    auto *raw = new unsigned char[count * BLOCK_RECORD_SIZE];
    int offset = 0;
    for (auto &it: blocks) {
        if (selectedOnly && !it.second.selected) continue;
        int x = Block_X(it.first) - (int) originX;
        int y = Block_Y(it.first) - (int) originY;
        it.second.write(reinterpret_cast<char *>(raw) + offset, Block_TO_LONG(x, y));
        offset += BLOCK_RECORD_SIZE;
    }

    unsigned long len = 0;
    auto *deflated = Engine::Filesystem::compress(raw, offset, &len);
    delete[] raw;
    if (deflated == nullptr) return false;

    size_t b64cap = ((size_t) len + 2) / 3 * 4 + 1;
    auto *buf = new char[b64cap];
    size_t b64len = 0;
    base64_encode(reinterpret_cast<const char *>(deflated), len, buf, &b64len, 0);
    delete[] deflated;
    buf[b64len] = 0;
    glfwSetClipboardString(window, buf);
    delete[] buf;
    return true;
}

// Decodes a clipboard string produced by blocksToClipboard.
// Returns the number of parsed records, or -1 on failure.
static long long blocksFromClipboard(GLFWwindow *window, Blocks &out, int offsetX, int offsetY) {
    const char *importString = glfwGetClipboardString(window);
    if (importString == nullptr || importString[0] == 0) return -1;

    size_t inLength = strlen(importString);
    auto *bytes = new char[inLength / 4 * 3 + 4];
    size_t written = 0;
    long long count = -1;
    if (base64_decode(importString, inLength, bytes, &written, 0) == 1 && written > 0) {
        unsigned long length = 0;
        auto *inflated = Engine::Filesystem::decompress(reinterpret_cast<unsigned char *>(bytes),
                                                        (unsigned int) written, &length);
        if (inflated != nullptr && length > 0 && length % BLOCK_RECORD_SIZE == 0) {
            count = (long long) (length / BLOCK_RECORD_SIZE);
            long long pos = 0;
            for (long long i = 0; i < count; i++) {
                Block block(reinterpret_cast<char *>(inflated) + i * BLOCK_RECORD_SIZE, &pos);
                out[Block_TO_LONG(Block_X(pos) + offsetX, Block_Y(pos) + offsetY)] = block;
            }
        }
        free(inflated);
    }
    delete[] bytes;
    return count;
}

void BlockManager::copy(int blockX, int blockY, bool notify) {
    size_t count = 0;
    for (auto &it: blocks) {
        count += it.second.selected;
    }
    selectedBlocks = (int) count;

    if (!blocksToClipboard(window->getId(), blocks, blockX, blockY, true, count)) {
        if (notify) {
            ImGuiToast toast(ImGuiToastType_Warning, 2000);
            toast.set_title(count == 0 ? "Nothing to copy" : "Failed to copy");
            ImGui::InsertNotification(toast);
        }
        return;
    }

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

void BlockManager::beginPaste() {
    Blocks parsed;
    long long count = blocksFromClipboard(window->getId(), parsed, 0, 0);
    if (count <= 0) {
        ImGuiToast toast(ImGuiToastType_Error, 2000);
        toast.set_title("Failed to paste");
        ImGui::InsertNotification(toast);
        return;
    }
    pasteBuffer = std::move(parsed);
    pasting = true;
}

void BlockManager::commitPaste(int blockX, int blockY) {
    if (!pasting) return;
    for (auto &it: pasteBuffer) {
        long long key = Block_TO_LONG(Block_X(it.first) + blockX, Block_Y(it.first) + blockY);
        auto existing = blocks.find(key);
        record(key, existing != blocks.end() ? &existing->second : nullptr, &it.second);
        blocks[key] = it.second;
    }
    chunkIndex.rebuild(blocks);
    circuit.rebuild();
    commitUndo();

    ImGuiToast toast(ImGuiToastType_Success, 2000);
    toast.set_title("%d blocks pasted", (int) pasteBuffer.size());
    ImGui::InsertNotification(toast);
    pasting = false;
    pasteBuffer.clear();
}

void BlockManager::cancelPaste() {
    pasting = false;
    pasteBuffer.clear();
}

void BlockManager::export_scheme() {
    ImGuiToast toast(0);
    if (blocksToClipboard(window->getId(), blocks, 0, 0, false, blocks.size())) {
        toast.set_type(ImGuiToastType_Success);
        toast.set_title("Scheme exported to clipboard (%d blocks)", length());
    } else {
        toast.set_type(ImGuiToastType_Warning);
        toast.set_title(blocks.empty() ? "Nothing to export" : "Failed to export");
    }
    ImGui::InsertNotification(toast);
}

void BlockManager::import_scheme(EditorCamera *camera) {
    Blocks imported;
    long long count = blocksFromClipboard(window->getId(), imported, 0, 0);

    ImGuiToast toast(0);
    if (count > 0) {
        blocks = std::move(imported);
        chunkIndex.rebuild(blocks);
        circuit.rebuild();
        clearHistory();
        selectedBlocks = 0;
        dirty = true;
        editRevision++;
        camera->position = glm::vec3(0.f);
        toast.set_type(ImGuiToastType_Success);
        toast.set_title("Scheme imported (%d blocks)", (int) count);
    } else {
        toast.set_type(ImGuiToastType_Error);
        toast.set_title("Clipboard has no scheme");
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
    for (auto it = blocks.begin(); it != blocks.end();) {
        if (it->second.selected) {
            record(it->first, &it->second, nullptr);
            it = blocks.erase(it);
        } else {
            ++it;
        }
    }
    chunkIndex.rebuild(blocks);
    circuit.rebuild();
    commitUndo();
    selectedBlocks = 0;
}

void BlockManager::deselect_all() {
    for (auto &it: blocks) {
        it.second.selected = false;
    }
    selectedBlocks = 0;
}

void BlockManager::move_selected(int dx, int dy) {
    if (dx == 0 && dy == 0) return;
    std::vector<std::pair<long long, Block>> moved;
    for (auto it = blocks.begin(); it != blocks.end();) {
        if (it->second.selected) {
            moved.emplace_back(it->first, it->second);
            record(it->first, &it->second, nullptr);
            it = blocks.erase(it);
        } else {
            ++it;
        }
    }
    for (auto &entry: moved) {
        long long key = Block_TO_LONG(Block_X(entry.first) + dx, Block_Y(entry.first) + dy);
        auto existing = blocks.find(key);
        record(key, existing != blocks.end() ? &existing->second : nullptr, &entry.second);
        blocks[key] = entry.second;
    }
    chunkIndex.rebuild(blocks);
    circuit.rebuild();
    commitUndo();
}

void BlockManager::rotate_selected(int k) {
    std::vector<std::pair<long long, Block>> rotated;
    int minX = INT_MAX, maxX = INT_MIN, minY = INT_MAX, maxY = INT_MIN;
    for (auto &it: blocks) {
        if (!it.second.selected) continue;
        rotated.emplace_back(it.first, it.second);
        minX = std::min(minX, Block_X(it.first));
        maxX = std::max(maxX, Block_X(it.first));
        minY = std::min(minY, Block_Y(it.first));
        maxY = std::max(maxY, Block_Y(it.first));
    }
    if (rotated.empty()) return;
    int cx = (minX + maxX) / 2, cy = (minY + maxY) / 2;

    for (auto &entry: rotated) {
        record(entry.first, &entry.second, nullptr);
        blocks.erase(entry.first);
    }
    for (auto &entry: rotated) {
        int rx = Block_X(entry.first) - cx;
        int ry = Block_Y(entry.first) - cy;
        int nx = k > 0 ? cx + ry : cx - ry;
        int ny = k > 0 ? cy - rx : cy + rx;
        Block block = entry.second;
        block.rotation = rotateBlock(block.rotation, k);
        long long key = Block_TO_LONG(nx, ny);
        auto existing = blocks.find(key);
        record(key, existing != blocks.end() ? &existing->second : nullptr, &block);
        blocks[key] = block;
    }
    chunkIndex.rebuild(blocks);
    circuit.rebuild();
    commitUndo();
}

void BlockManager::draw(EditorCamera *camera) {
    int j = 0;
    int LB = (int) camera->position.x + (int) camera->left - 16;
    int RB = (int) camera->position.x + (int) camera->right + 16;
    int BB = (int) camera->position.y + (int) camera->bottom - 16;
    int TB = (int) camera->position.y + (int) camera->top + 16;

    auto flush = [&]() {
        glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, (long long) sizeof(BlockInfo) * j, info);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, j);
        j = 0;
    };

    // walk only the chunks that overlap the view
    chunkIndex.visit((LB >> 5) - 1, (BB >> 5) - 1, (RB >> 5) + 1, (TB >> 5) + 1, [&](long long key) {
        int x = Block_X(key);
        int y = Block_Y(key);
        int px = x << 5;
        int py = y << 5;
        if (px > LB && px < RB && py > BB && py < TB) {
            const Block &block = blocks.find(key)->second;
            info[j] = BlockInfo(block.typeId, block.active, block.selected, block.rotation, x, y);
            j++;
            if (j == BLOCK_BATCHING) flush();
        }
    });
    if (j > 0) flush();
}

void BlockManager::drawInstances(const std::vector<BlockInfo> &list) {
    size_t offset = 0;
    while (offset < list.size()) {
        int batch = (int) std::min((size_t) BLOCK_BATCHING, list.size() - offset);
        glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, (long long) sizeof(BlockInfo) * batch, list.data() + offset);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, batch);
        offset += batch;
    }
}

void BlockManager::drawGhost(int x, int y) {
    std::vector<BlockInfo> one{BlockInfo(static_cast<BlockId>(currentBlock), false, false,
                                         static_cast<BlockRotation>(currentRotation), x, y)};
    drawInstances(one);
}

void BlockManager::drawPasteGhost(int blockX, int blockY) {
    std::vector<BlockInfo> list;
    list.reserve(pasteBuffer.size());
    for (auto &it: pasteBuffer) {
        list.emplace_back(it.second.typeId, it.second.active, false, it.second.rotation,
                          Block_X(it.first) + blockX, Block_Y(it.first) + blockY);
    }
    drawInstances(list);
}

void BlockManager::drawSelectionGhost(int dx, int dy) {
    std::vector<BlockInfo> list;
    for (auto &it: blocks) {
        if (!it.second.selected) continue;
        list.emplace_back(it.second.typeId, it.second.active, true, it.second.rotation,
                          Block_X(it.first) + dx, Block_Y(it.first) + dy);
    }
    drawInstances(list);
}

std::uint64_t BlockManager::revision() const {
    return editRevision;
}

std::optional<BlockBounds> BlockManager::bounds(bool selectedOnly) const {
    BlockBounds result{INT_MAX, INT_MAX, INT_MIN, INT_MIN};
    bool found = false;
    for (const auto &entry: blocks) {
        if (selectedOnly && !entry.second.selected) continue;
        const int x = Block_X(entry.first);
        const int y = Block_Y(entry.first);
        result.minX = std::min(result.minX, x);
        result.minY = std::min(result.minY, y);
        result.maxX = std::max(result.maxX, x);
        result.maxY = std::max(result.maxY, y);
        found = true;
    }
    return found ? std::optional<BlockBounds>(result) : std::nullopt;
}
