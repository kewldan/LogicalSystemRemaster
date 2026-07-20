# 🔌 Logical System

> A digital logic sandbox — build circuits from wires, gates, switches, clocks and lamps, and watch them run in real time.

[![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat&logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/20)
[![OpenGL](https://img.shields.io/badge/OpenGL-glad-5586A4?style=flat&logo=opengl&logoColor=white)](https://www.opengl.org/)
[![GLFW](https://img.shields.io/badge/GLFW-3.3-orange?style=flat)](https://www.glfw.org/)
[![Dear ImGui](https://img.shields.io/badge/Dear%20ImGui-1.89-4b8bbe?style=flat)](https://github.com/ocornut/imgui)
[![vcpkg](https://img.shields.io/badge/deps-vcpkg-blue?style=flat)](https://vcpkg.io/)
[![Platform](https://img.shields.io/badge/platform-Windows-0078D6?style=flat&logo=windows&logoColor=white)](#)
[![License](https://img.shields.io/badge/license-MIT-green?style=flat)](LICENSE)
[![itch.io](https://img.shields.io/badge/itch.io-logical--system-fa5c5c?style=flat&logo=itchdotio&logoColor=white)](https://kewldan.itch.io/logical-system)

A remaster of [Logical System](https://kewldan.itch.io/logical-system) (v2.0.9). Place logic blocks on an infinite 2D grid, wire them together and simulate whole devices — from a single gate to adders and RAM.

## ✨ Features

- 🧩 **15 block types** — 7 wire variants (straight, angled, T, cross, ...), NOT, AND, NAND, XOR, NXOR, Switch, Clock and Lamp
- ⚡ **Multithreaded simulation** — the circuit ticks on a dedicated thread with an adjustable rate (2–256 TPS), plus pause & single-step mode
- 🚀 **Batched instanced rendering** — blocks are drawn from a texture atlas in batches of 8192 instances per draw call
- 🌟 **HDR bloom** — active blocks glow via a ping-pong Gaussian blur post-processing pipeline (can be toggled off)
- 💾 **Save / load schemes** — BSON-based `.ls` / `.bson` files through native Windows file dialogs
- ✂️ **Clipboard workflow** — box-select, copy, cut, paste, select-all and mass delete
- 🔔 **Toast notifications** — feedback for saving, loading and selections
- 🖥️ **Clean ImGui HUD** — FPS / tick-time overlay, block & rotation pickers, VSync and graphics options, hideable UI

## 📷 Screenshots

![1 byte of RAM](https://img.itch.zone/aW1hZ2UvMTExNTgwOC8xMjAxMDY1Ny5wbmc=/original/YZbIZb.png)
![Blocks](https://img.itch.zone/aW1hZ2UvMTExNTgwOC8xMjAxMDY1Ni5wbmc=/original/%2FT1xPi.png)
![4 bit adder](https://img.itch.zone/aW1hZ2UvMTExNTgwOC8xMjAxMDY1OC5wbmc=/original/Ow45RS.png)
![File browser](https://img.itch.zone/aW1hZ2UvMTExNTgwOC8xMjAxMDY2MS5wbmc=/original/jS1Tjm.png)
![Settings](https://img.itch.zone/aW1hZ2UvMTExNTgwOC8xMjAxMDY1OS5wbmc=/original/3v9W5Y.png)
![Graphic settings](https://img.itch.zone/aW1hZ2UvMTExNTgwOC8xMjAxMDY2MC5wbmc=/original/3PkZMB.png)

## 🎮 Controls

| Input | Action |
|---|---|
| `W` `A` `S` `D` | Pan the camera |
| Mouse wheel | Zoom in / out |
| `LMB` | Place block / toggle switch / rotate existing block |
| `RMB` | Erase block |
| `Shift` + drag | Box-select blocks |
| `0`–`9` | Pick block type (hold `Shift` for types 10–14) |
| `R` | Rotate current block clockwise (`Shift` + `R` — counter-clockwise) |
| `Ctrl` + `S` / `O` / `N` | Save / open / new scheme |
| `Ctrl` + `C` / `V` / `X` / `A` | Copy / paste / cut / select all |
| `Delete` | Delete selected blocks |
| `F1` | Block info toast (debug) |
| `F2` | Toggle UI |
| `F3` | Fill a 256×256 area (stress test) |

## 🛠️ Tech stack

| Dependency | Purpose |
|---|---|
| [glad](https://github.com/Dav1dde/glad) + [GLFW](https://www.glfw.org/) | OpenGL loading & windowing |
| [glm](https://github.com/g-truc/glm) | Math |
| [Dear ImGui](https://github.com/ocornut/imgui) (freetype, GLFW & OpenGL3 bindings) | UI |
| [plog](https://github.com/SergiusTheBest/plog) | Logging |
| [nativefiledialog](https://github.com/mlabbe/nativefiledialog) | Native save/open dialogs |
| [nlohmann-json](https://github.com/nlohmann/json), BSON | Scheme serialization |
| [aklomp/base64](https://github.com/aklomp/base64), [zlib](https://zlib.net/), [stb](https://github.com/nothings/stb) | Encoding, compression, image loading |

All third-party libraries are managed through the **vcpkg manifest** ([`vcpkg.json`](vcpkg.json)).

## 🔨 Building

Requirements: **CMake ≥ 3.23**, a C++20 compiler (MSVC), [vcpkg](https://vcpkg.io/).

> **Note:** the project also links against the author's in-house `Engine` library, which is **not part of this repository**. `CMakeLists.txt` expects it at the path set in `ENGINE_DIR` (`E:\Projects\Engine` by default) — adjust that variable to your local checkout of the engine before configuring.

```powershell
git clone https://github.com/kewldan/LogicalSystemRemaster.git
cd LogicalSystemRemaster

cmake -B build -S . -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

vcpkg installs all manifest dependencies automatically during the configure step. Run the executable from the repository root so it can find the `data/` folder.

## ⚠️ Status

This is a prototype. If you find bugs, please open an issue or a pull request. Back up your saves — a broken build may corrupt them.

## 📄 License

[MIT](LICENSE) © kewldan
