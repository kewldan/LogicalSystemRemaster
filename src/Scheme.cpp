#include "Scheme.h"

#include <chrono>
#include <cstring>
#include <nlohmann/json.hpp>
#include "plog/Log.h"

bool schemeFromMemory(const char *data, int length, bool isBson, Blocks &outBlocks, SchemeCamera &outCamera) {
    if (data == nullptr || length <= 0) return false;

    Blocks loaded;
    SchemeCamera camera;

    if (isBson) {
        try {
            nlohmann::json loadFile = nlohmann::json::from_bson(
                    std::vector<std::uint8_t>(data, data + length));

            camera.zoom = loadFile["camera"]["zoom"].get<float>();
            camera.x = loadFile["camera"]["position"]["x"].get<float>();
            camera.y = loadFile["camera"]["position"]["y"].get<float>();

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
        memcpy(&camera.x, data, 4);
        memcpy(&camera.y, data + 4, 4);
        memcpy(&camera.zoom, data + 8, 4);
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

    outBlocks = std::move(loaded);
    outCamera = camera;
    return true;
}

std::vector<std::uint8_t> schemeToBson(const Blocks &blocks, const SchemeCamera &camera) {
    nlohmann::json saveFile;
    saveFile["camera"]["position"]["x"] = camera.x;
    saveFile["camera"]["position"]["y"] = camera.y;
    saveFile["camera"]["zoom"] = camera.zoom;
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
    return nlohmann::json::to_bson(saveFile);
}
