#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest/doctest.h>

#include <fstream>
#include <filesystem>
#include <iterator>
#include <vector>

#include "AppStorage.h"
#include "BlockCatalog.h"
#include "CameraFrame.h"
#include "ChunkIndex.h"
#include "Scheme.h"
#include "SmoothZoom.h"
#include "Simulation.h"
#include "io/Filesystem.h"

static void put(Circuit &c, int x, int y, BlockId type, BlockRotation rotation = 0, bool active = false) {
    Block block(type, rotation);
    block.active = active;
    c.blocks[Block_TO_LONG(x, y)] = block;
}

static bool lamp(Circuit &c, int x, int y) {
    auto it = c.blocks.find(Block_TO_LONG(x, y));
    REQUIRE(it != c.blocks.end());
    return it->second.active;
}

static void setSwitch(Circuit &c, int x, int y, bool on) {
    auto it = c.blocks.find(Block_TO_LONG(x, y));
    REQUIRE(it != c.blocks.end());
    REQUIRE(it->second.typeId == BLOCK_SWITCH);
    it->second.active = on;
    c.invalidate(x, y);
}

static void pressButton(Circuit &c, int x, int y) {
    auto it = c.blocks.find(Block_TO_LONG(x, y));
    REQUIRE(it != c.blocks.end());
    REQUIRE(it->second.typeId == BLOCK_BUTTON);
    it->second.active = true;
    c.invalidate(x, y);
}

static void settle(Circuit &c, int ticks = 40) {
    for (int i = 0; i < ticks; i++) c.tick();
}

static Circuit loadExample(const char *path) {
    std::ifstream file(path, std::ios::binary);
    REQUIRE_MESSAGE(file.good(), path);
    std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    Circuit c;
    SchemeCamera camera;
    REQUIRE(schemeFromMemory(data.data(), (int) data.size(), true, c.blocks, camera));
    c.rebuild();
    return c;
}

TEST_CASE("rotateBlock wraps in both directions") {
    CHECK(rotateBlock(0, 1) == 1);
    CHECK(rotateBlock(3, 1) == 0);
    CHECK(rotateBlock(0, -1) == 3);
    CHECK(rotateBlock(2, -1) == 1);
}

TEST_CASE("zoom treats a wheel tick and fractional touchpad deltas equally") {
    SmoothZoom wheel;
    SmoothZoom touchpad;

    wheel.addScroll(1.f);
    for (int i = 0; i < 10; i++) touchpad.addScroll(0.1f);

    CHECK(wheel.target() < 1.f);
    CHECK(touchpad.target() == doctest::Approx(wheel.target()).epsilon(0.00001));
}

TEST_CASE("zoom animation is frame-rate independent and bounded") {
    SmoothZoom oneFrame;
    SmoothZoom manyFrames;
    oneFrame.addScroll(-2.f);
    manyFrames.addScroll(-2.f);

    const float once = oneFrame.step(0.1f);
    for (int i = 0; i < 10; i++) manyFrames.step(0.01f);
    CHECK(manyFrames.value() == doctest::Approx(once).epsilon(0.00001));

    SmoothZoom limits;
    for (int i = 0; i < 100; i++) limits.addScroll(8.f);
    CHECK(limits.target() == doctest::Approx(SmoothZoom::MIN_ZOOM));
    for (int i = 0; i < 100; i++) limits.addScroll(-8.f);
    CHECK(limits.target() == doctest::Approx(SmoothZoom::MAX_ZOOM));
}

TEST_CASE("zoom approaches its target smoothly without overshooting") {
    SmoothZoom zoom;
    zoom.addScroll(1.f);
    const float target = zoom.target();
    float previous = zoom.value();

    for (int i = 0; i < 120; i++) {
        const float current = zoom.step(1.f / 120.f);
        CHECK(current <= previous);
        CHECK(current >= target);
        previous = current;
    }
    CHECK(zoom.value() == doctest::Approx(target).epsilon(0.0001));
}

TEST_CASE("camera framing centers content and respects zoom limits") {
    const CameraFrame frame = calculateCameraFrame(-16.f, -16.f, 16.f, 16.f, 1280, 720);
    CHECK(frame.positionX == doctest::Approx(-640.f));
    CHECK(frame.positionY == doctest::Approx(-360.f));
    CHECK(frame.zoom == doctest::Approx(SmoothZoom::MIN_ZOOM));

    const CameraFrame wide = calculateCameraFrame(-1600.f, -16.f, 1600.f, 16.f, 800, 600, 50.f);
    CHECK(wide.positionX == doctest::Approx(-400.f));
    CHECK(wide.positionY == doctest::Approx(-300.f));
    CHECK(wide.zoom > 1.f);
    CHECK(wide.zoom <= SmoothZoom::MAX_ZOOM);
}

TEST_CASE("recent files are normalized, deduplicated and capped") {
    const auto directory = std::filesystem::temp_directory_path() / "logical-system-storage-test";
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    AppStorage storage(directory);
    AppSettings settings;
    storage.rememberRecentFile(settings, "./first.bson");
    storage.rememberRecentFile(settings, "./second.bson");
    storage.rememberRecentFile(settings, "./first.bson");
    CHECK(settings.recentFiles.size() == 2);
    CHECK(std::filesystem::path(settings.recentFiles.front()).filename() == "first.bson");

    for (int i = 0; i < 12; i++) {
        storage.rememberRecentFile(settings, "scheme-" + std::to_string(i) + ".bson");
    }
    CHECK(settings.recentFiles.size() == 8);
    CHECK(std::filesystem::path(settings.recentFiles.front()).filename() == "scheme-11.bson");

    settings.width = 1920;
    settings.tps = 42;
    CHECK(storage.saveSettings(settings));
    const AppSettings loaded = storage.loadSettings();
    CHECK(loaded.width == 1920);
    CHECK(loaded.tps == 42);
    CHECK(loaded.recentFiles == settings.recentFiles);
    std::filesystem::remove_all(directory, error);
}

TEST_CASE("every block has palette metadata") {
    for (BlockId id = 0; id < BLOCK_TYPE_COUNT; id++) {
        CHECK(describeBlock(id).name[0] != '\0');
        CHECK(describeBlock(id).rule[0] != '\0');
    }
}

TEST_CASE("isBlockActive truth tables") {
    for (BlockConnectionCount c = 0; c < 5; c++) {
        CHECK(isBlockActive(0, c) == (c > 0));            // wire
        CHECK(isBlockActive(7, c) == (c == 0));           // NOT
        CHECK(isBlockActive(8, c) == (c >= 2));           // AND
        CHECK(isBlockActive(9, c) == (c < 2));            // NAND
        CHECK(isBlockActive(10, c) == (c % 2 == 1));      // XOR
        CHECK(isBlockActive(11, c) == (c % 2 == 0));      // NXOR
        CHECK(isBlockActive(BLOCK_SWITCH, c) == false);
        CHECK(isBlockActive(14, c) == (c > 0));           // lamp
    }
}

TEST_CASE("block record round trip") {
    Block original(10, 3);
    original.active = true;
    char buffer[BLOCK_RECORD_SIZE];
    original.write(buffer, Block_TO_LONG(-5, 7));

    long long pos = 0;
    Block parsed(buffer, &pos);
    CHECK(Block_X(pos) == -5);
    CHECK(Block_Y(pos) == 7);
    CHECK(parsed.typeId == 10);
    CHECK(parsed.rotation == 3);
    CHECK(parsed.active);

    buffer[8] = 99; // invalid type must degrade to 0, not crash
    Block corrupted(buffer, &pos);
    CHECK(corrupted.typeId == 0);
}

TEST_CASE("signals travel one wire per tick and decay") {
    Circuit c;
    put(c, 0, 0, BLOCK_SWITCH, 0, true);
    put(c, 1, 0, 0, 1);
    put(c, 2, 0, 0, 1);
    put(c, 3, 0, 14);
    c.rebuild();

    c.tick();
    CHECK(lamp(c, 1, 0));
    CHECK_FALSE(lamp(c, 3, 0));
    c.tick();
    c.tick();
    CHECK(lamp(c, 3, 0));

    setSwitch(c, 0, 0, false);
    c.tick();
    CHECK_FALSE(lamp(c, 1, 0));
    CHECK(lamp(c, 3, 0)); // still lit by the wire in front of it
    c.tick();
    c.tick();
    CHECK_FALSE(lamp(c, 3, 0));
}

TEST_CASE("NOT is active with no inputs, clock toggles") {
    Circuit c;
    put(c, 0, 0, 7, 1);
    put(c, 5, 5, BLOCK_CLOCK, 1);
    c.rebuild();
    settle(c, 3);
    CHECK(lamp(c, 0, 0));

    bool state = c.blocks[Block_TO_LONG(5, 5)].active;
    c.tick();
    CHECK(c.blocks[Block_TO_LONG(5, 5)].active != state);
}

TEST_CASE("editing while running keeps bookkeeping consistent") {
    Circuit c;
    put(c, 0, 0, 7, 1); // NOT feeding right
    put(c, 1, 0, 0, 1);
    put(c, 2, 0, 14);
    c.rebuild();
    settle(c);
    CHECK(lamp(c, 2, 0));

    c.blocks.erase(Block_TO_LONG(0, 0)); // remove the source
    c.invalidate(0, 0);
    settle(c);
    CHECK_FALSE(lamp(c, 2, 0));

    put(c, 0, 0, 7, 1); // place it back
    c.invalidate(0, 0);
    settle(c);
    CHECK(lamp(c, 2, 0));
}

TEST_CASE("scheme BSON round trip") {
    Blocks blocks;
    Block a(10, 2);
    a.active = true;
    blocks[Block_TO_LONG(-3, 9)] = a;
    blocks[Block_TO_LONG(100, -100)] = Block(0, 1);
    SchemeCamera camera{12.f, -34.f, 1.5f};

    auto bson = schemeToBson(blocks, camera);

    Blocks parsed;
    SchemeCamera parsedCamera;
    REQUIRE(schemeFromMemory(reinterpret_cast<const char *>(bson.data()), (int) bson.size(), true,
                             parsed, parsedCamera));
    REQUIRE(parsed.size() == 2);
    CHECK(parsed[Block_TO_LONG(-3, 9)].typeId == 10);
    CHECK(parsed[Block_TO_LONG(-3, 9)].rotation == 2);
    CHECK(parsed[Block_TO_LONG(-3, 9)].active);
    CHECK(parsedCamera.zoom == doctest::Approx(1.5f));
    CHECK(parsedCamera.x == doctest::Approx(12.f));
}

TEST_CASE("corrupted schemes are rejected, not fatal") {
    Blocks blocks;
    SchemeCamera camera;
    CHECK_FALSE(schemeFromMemory("garbage data here", 17, true, blocks, camera));
    CHECK_FALSE(schemeFromMemory("short", 5, false, blocks, camera));
    CHECK_FALSE(schemeFromMemory(nullptr, 0, true, blocks, camera));
}

TEST_CASE("compress/decompress round trips and rejects garbage") {
    unsigned char tiny[] = {42};
    unsigned long compressedLength = 0;
    auto *compressed = Engine::Filesystem::compress(tiny, 1, &compressedLength);
    REQUIRE(compressed != nullptr);
    REQUIRE(compressedLength > 0);

    unsigned long decompressedLength = 0;
    auto *decompressed = Engine::Filesystem::decompress(compressed, compressedLength, &decompressedLength);
    REQUIRE(decompressed != nullptr);
    REQUIRE(decompressedLength == 1);
    CHECK(decompressed[0] == 42);
    delete[] compressed;
    free(decompressed);

    unsigned char garbage[64];
    for (int i = 0; i < 64; i++) garbage[i] = (unsigned char) (i * 37 + 1);
    unsigned long length = 0;
    CHECK(Engine::Filesystem::decompress(garbage, 64, &length) == nullptr);

    unsigned char big[10000];
    for (int i = 0; i < 10000; i++) big[i] = (unsigned char) (i % 251);
    compressed = Engine::Filesystem::compress(big, 10000, &compressedLength);
    REQUIRE(compressed != nullptr);
    decompressed = Engine::Filesystem::decompress(compressed, compressedLength, &decompressedLength);
    REQUIRE(decompressed != nullptr);
    REQUIRE(decompressedLength == 10000);
    CHECK(memcmp(decompressed, big, 10000) == 0);
    // truncated stream must fail cleanly instead of hanging
    CHECK(Engine::Filesystem::decompress(compressed, compressedLength / 2, &length) == nullptr);
    delete[] compressed;
    free(decompressed);
}

TEST_CASE("example: blinker") {
    Circuit c = loadExample("data/examples/blinker.bson");
    settle(c, 10);
    bool seenOn = false, seenOff = false;
    for (int i = 0; i < 8; i++) {
        c.tick();
        (lamp(c, 3, 0) ? seenOn : seenOff) = true;
    }
    CHECK(seenOn);
    CHECK(seenOff);
}

TEST_CASE("example: logic gates") {
    Circuit c = loadExample("data/examples/gates.bson");
    for (int a = 0; a <= 1; a++) {
        setSwitch(c, 0, 8, a);
        settle(c);
        CHECK(lamp(c, 4, 8) == !a); // NOT
        for (int b = 0; b <= 1; b++) {
            setSwitch(c, 0, 5, a);
            setSwitch(c, 0, 3, b);
            setSwitch(c, 0, 0, a);
            setSwitch(c, 0, -2, b);
            settle(c);
            CHECK(lamp(c, 5, 4) == (a && b));  // AND
            CHECK(lamp(c, 5, -1) == (a != b)); // XOR
        }
    }
}

TEST_CASE("example: RS latch holds state") {
    Circuit c = loadExample("data/examples/rs-latch.bson");
    auto press = [&](int x, int y) {
        setSwitch(c, x, y, true);
        settle(c);
        setSwitch(c, x, y, false);
        settle(c);
    };
    press(2, 7); // S
    CHECK(lamp(c, 5, 4));
    CHECK_FALSE(lamp(c, -1, 0));
    for (int i = 0; i < 10; i++) { // stable, not oscillating
        c.tick();
        CHECK(lamp(c, 5, 4));
        CHECK_FALSE(lamp(c, -1, 0));
    }
    press(2, -3); // R
    CHECK_FALSE(lamp(c, 5, 4));
    CHECK(lamp(c, -1, 0));
}

TEST_CASE("button emits a latch-friendly momentary pulse") {
    Circuit c;
    put(c, 0, 0, BLOCK_BUTTON, 0, true); // just pressed
    put(c, 1, 0, 0, 1);
    put(c, 2, 0, 14);
    c.rebuild();

    c.tick();
    CHECK(c.blocks[Block_TO_LONG(0, 0)].active);
    CHECK(lamp(c, 1, 0));
    c.tick();
    CHECK(lamp(c, 2, 0));
    settle(c, BUTTON_PULSE_TICKS);
    CHECK_FALSE(c.blocks[Block_TO_LONG(0, 0)].active);
    CHECK_FALSE(lamp(c, 1, 0));
    c.tick();
    CHECK_FALSE(lamp(c, 2, 0));
}

TEST_CASE("chunk index matches brute force") {
    Blocks blocks;
    ChunkIndex index;
    for (int i = -100; i <= 100; i += 7) {
        long long key = Block_TO_LONG(i, -i * 3);
        blocks[key] = Block(static_cast<BlockId>(0), 0);
        index.insert(key);
    }
    // erase a few
    for (int i = -100; i <= 100; i += 21) {
        long long key = Block_TO_LONG(i, -i * 3);
        blocks.erase(key);
        index.erase(key);
    }

    int x0 = -50, y0 = -160, x1 = 60, y1 = 90;
    std::unordered_set<long long> visited;
    index.visit(x0, y0, x1, y1, [&](long long key) { visited.insert(key); });

    for (auto &entry: blocks) {
        int x = Block_X(entry.first), y = Block_Y(entry.first);
        if (x >= x0 && x <= x1 && y >= y0 && y <= y1) {
            CHECK_MESSAGE(visited.contains(entry.first), "missing ", x, ",", y);
        }
    }
    for (long long key: visited) {
        CHECK(blocks.contains(key)); // no stale keys
    }

    ChunkIndex rebuilt;
    rebuilt.rebuild(blocks);
    CHECK(rebuilt.chunkCount() == index.chunkCount());
}

TEST_CASE("example: full adder truth table") {
    Circuit c = loadExample("data/examples/full-adder.bson");
    for (int a = 0; a <= 1; a++) {
        for (int b = 0; b <= 1; b++) {
            for (int cin = 0; cin <= 1; cin++) {
                setSwitch(c, 0, 8, a);
                setSwitch(c, 0, -4, b);
                setSwitch(c, -2, 4, cin);
                settle(c);
                int total = a + b + cin;
                CHECK_MESSAGE(lamp(c, 8, 4) == (total % 2 == 1), "Sum(", a, b, cin, ")");
                CHECK_MESSAGE(lamp(c, 9, 0) == (total >= 2), "Cout(", a, b, cin, ")");
            }
        }
    }
}

TEST_CASE("example: 4-bit ripple-carry adder, all 512 combinations") {
    Circuit c = loadExample("data/examples/adder-4bit.bson");
    const int STEP = 16;
    for (int a = 0; a < 16; a++) {
        for (int b = 0; b < 16; b++) {
            for (int cin = 0; cin <= 1; cin++) {
                for (int i = 0; i < 4; i++) {
                    setSwitch(c, 0, -i * STEP + 8, (a >> i) & 1);
                    setSwitch(c, 0, -i * STEP - 4, (b >> i) & 1);
                }
                setSwitch(c, -2, 4, cin);
                settle(c, 220);
                int total = a + b + cin;
                for (int i = 0; i < 4; i++) {
                    CHECK_MESSAGE(lamp(c, 8, -i * STEP + 4) == (((total >> i) & 1) != 0),
                                  "Sum", i, "(", a, "+", b, "+", cin, ")");
                }
                CHECK_MESSAGE(lamp(c, 9, -3 * STEP) == (total >= 16), "Cout(", a, "+", b, "+", cin, ")");
            }
        }
    }
}

static unsigned readCpuRamWord(Circuit &c, int address) {
    constexpr int BANK_X0 = 700;
    constexpr int BANK_PITCH = 520;
    constexpr int RAM_Y0 = -200;
    constexpr int ROW_PITCH = 20;
    constexpr int BIT_PITCH = 22;
    int bank = address / 128;
    int row = address % 128;
    int base = BANK_X0 + bank * BANK_PITCH;
    int y = RAM_Y0 - row * ROW_PITCH;
    unsigned value = 0;
    for (int bit = 0; bit < 16; bit++) {
        int qx = base + 80 + bit * BIT_PITCH + 7;
        auto found = c.blocks.find(Block_TO_LONG(qx, y + 2));
        REQUIRE(found != c.blocks.end());
        REQUIRE(found->second.typeId == 9); // Q side of the NAND latch
        if (found->second.active) value |= 1u << bit;
    }
    return value;
}

static unsigned readCpuRegister(Circuit &c, int y) {
    constexpr int X0 = 700 + 80;
    constexpr int BIT_PITCH = 22;
    unsigned value = 0;
    for (int bit = 0; bit < 16; bit++) {
        auto found = c.blocks.find(Block_TO_LONG(X0 + bit * BIT_PITCH + 7, y + 2));
        REQUIRE(found != c.blocks.end());
        if (found->second.active) value |= 1u << bit;
    }
    return value;
}

TEST_CASE("example: autonomous 16-bit SUBLEQ decrements gate-level 1 KiB RAM") {
    Circuit c = loadExample("data/examples/cpu16-1k.bson");
    CHECK(c.blocks.size() > 900000); // no hidden word-level RAM component
    REQUIRE(readCpuRamWord(c, 100) == 1);
    REQUIRE(readCpuRamWord(c, 101) == 5);

    // One complete 18-phase instruction is 88,632 ticks. Leave margin for
    // initial propagation from the timing ring into the control matrix.
    settle(c, 100000);

    CHECK(readCpuRegister(c, 240) == 102); // next instruction has begun
    CHECK(readCpuRegister(c, 220) == 101);
    CHECK(readCpuRegister(c, 200) == 6);
    CHECK(readCpuRegister(c, 180) == 1);
    CHECK(readCpuRegister(c, 160) == 5);
    CHECK(readCpuRamWord(c, 101) == 4);

    // Let the demo finish all five decrements. If the <= 0 branch is broken,
    // execution returns through address 3 and underflows the counter instead
    // of remaining in the halt loop at address 6.
    settle(c, 900000);
    CHECK(readCpuRamWord(c, 101) == 0);
}

static unsigned readClickAdderRegister(Circuit &c, int y, int bits) {
    constexpr int X0 = 700 + 80;
    constexpr int PITCH = 22;
    unsigned value = 0;
    for (int bit = 0; bit < bits; bit++) {
        auto found = c.blocks.find(Block_TO_LONG(X0 + bit * PITCH + 7, y + 2));
        REQUIRE(found != c.blocks.end());
        if (found->second.active) value |= 1u << bit;
    }
    return value;
}

static void checkDecimalDisplay(Circuit &c, int decoderBase, unsigned value) {
    constexpr unsigned masks[10] = {
        0x3f, 0x06, 0x5b, 0x4f, 0x66,
        0x6d, 0x7d, 0x07, 0x7f, 0x6f
    };
    constexpr int segmentX[7] = {12, 30, 30, 18, 9, 9, 15};
    constexpr int segmentY[7] = {30, 22, 7, 0, 7, 22, 15};
    const unsigned digits[3] = {
        value / 100, (value / 10) % 10, value % 10
    };

    for (int digit = 0; digit < 3; digit++) {
        for (int segment = 0; segment < 7; segment++) {
            CAPTURE(value);
            CAPTURE(digit);
            CAPTURE(segment);
            CHECK(lamp(c,
                       decoderBase + 70 + digit * 45 + segmentX[segment],
                       1120 + segmentY[segment]) ==
                  ((masks[digits[digit]] >> segment) & 1u));
        }
    }
}

TEST_CASE("example: click adder counts two inputs and latches their sum") {
    Circuit c = loadExample("data/examples/click-adder.bson");

    pressButton(c, 1540, 1400);
    settle(c, 8000);
    CHECK(readClickAdderRegister(c, 300, 8) == 1);

    pressButton(c, 1570, 1400);
    settle(c, 5000);
    CHECK(readClickAdderRegister(c, 240, 8) == 1);
    CHECK(readClickAdderRegister(c, 260, 8) == 1);
    pressButton(c, 1570, 1400);
    settle(c, 5000);
    CHECK(readClickAdderRegister(c, 260, 8) == 2);

    pressButton(c, 1600, 1400);
    settle(c, 4000);
    CHECK(readClickAdderRegister(c, 220, 16) == 3);
    checkDecimalDisplay(c, 1250, 1);
    checkDecimalDisplay(c, 1550, 2);
    checkDecimalDisplay(c, 1850, 3);
}
