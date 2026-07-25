#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>

#include "Scheme.h"
#include "Simulation.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: %s <scheme.bson> [ticks] [warmup]\n", argv[0]);
        return 1;
    }
    const int ticks = argc > 2 ? atoi(argv[2]) : 100000;
    const int warmup = argc > 3 ? atoi(argv[3]) : 2000;
    const int repetitions = argc > 4 ? atoi(argv[4]) : 5;

    std::ifstream file(argv[1], std::ios::binary);
    if (!file.good()) {
        printf("cannot open %s\n", argv[1]);
        return 1;
    }
    std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    Circuit circuit;
    SchemeCamera camera;
    if (!schemeFromMemory(data.data(), (int) data.size(), true, circuit.blocks, camera)) {
        printf("failed to parse %s\n", argv[1]);
        return 1;
    }
    circuit.rebuild();

    for (int i = 0; i < warmup; i++) circuit.tick();

    double best = 1e18, total = 0;
    for (int r = 0; r < repetitions; r++) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < ticks; i++) circuit.tick();
        const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        best = elapsed < best ? elapsed : best;
        total += elapsed;
    }
    const double mean = total / repetitions;

    printf("%s\n", argv[1]);
    printf("  blocks:      %zu\n", circuit.blocks.size());
    printf("  ticks/rep:   %d x %d reps\n", ticks, repetitions);
    printf("  best:        %.3f us/tick  (%.0f ticks/s)\n", best / ticks * 1e6, ticks / best);
    printf("  mean:        %.3f us/tick  (%.0f ticks/s)\n", mean / ticks * 1e6, ticks / mean);
    return 0;
}
