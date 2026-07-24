# Changelog

## 2.3.0 — 2026-07-25

### Added

- Autosave writes a recovery copy after edits and offers Recover / Discard on the next launch after an interrupted session.
- The File menu now keeps the eight most recently opened or saved schemes.
- A visual 4×4 block palette replaces the text-only picker; every icon has a short behavior tooltip.
- Hovering a block shows its type, coordinates, live state, direction and logic rule.
- `F` frames the entire scheme, `Shift+F` frames the selection, and `Space` + left-drag pans the canvas.

### Changed

- Settings and recovery files now live in the platform user-data directory instead of inside the application bundle or beside the executable.

## Последний месяц — июль 2026

- Добавлены кнопка, ghost-preview, рисование проводов, перемещение и поворот выделения, undo/redo, paste под курсором и защита от несохранённых изменений.
- Появились готовые схемы, включая 4-битный сумматор, документация по блокам и тесты для логики, сериализации и примеров.
- Симуляция стала событийной, а рендеринг — chunked: большие и пустые схемы работают заметно быстрее.
- Исправлены гонки и краши при редактировании, вставке из буфера, повреждённых файлах и масштабировании; сохранения и настройки окна стали надёжнее.
- Добавлены CMake presets, CI с тестами и релизной упаковкой, а также нативная macOS-сборка `.app` с Retina-исправлениями, Cmd-сочетаниями и иконкой Dock.

## 2.2.2 — 2026-07-25

### Improved

- Reworked cursor-anchored zoom with frame-rate-independent spring motion, proportional scaling and smooth handling of both mouse-wheel ticks and fractional touchpad input.
- Windows releases now include a standalone static `.exe`; macOS releases include the complete `.app` bundle.
- Fixed asset placement in clean macOS bundles so downloaded builds find their shaders, fonts, textures and examples at runtime.

## 2.2.1 — 2026-07-24

### Added

- Native macOS support: the project builds into a self-contained `.app` bundle on both Apple Silicon and Intel Macs. Assets and settings resolve inside the bundle, and Help links open with the system browser.

## 2.2.0 — 2026-07-24

### Added

- **Button** block (16th type): a momentary switch — click it and it emits a single-tick pulse, then releases itself.
- **4-bit ripple-carry adder** in the Examples menu: four full-adder modules with carry buses, 348 blocks. Its complete 512-row truth table is asserted both by the generator and by the C++ test suite against the real simulation core.
- Ghost preview of the block about to be placed, honoring rotation.
- Paste now floats under the cursor: click to place, right-click or `Esc` to cancel.
- Wire painting: dragging with the straight wire draws a connected trace that rotates itself along the stroke, corners included.
- Selection tools: drag a selected block to move the whole selection (with a live ghost), `R` rotates the selection as a group around its center, `Esc` deselects.
- `Ctrl+S` saves silently into the current file; `Ctrl+Shift+S` is Save As. A missing extension gets `.bson` appended.
- Unsaved-changes confirmation (Save / Don't save / Cancel) on New, Open and exit.
- `settings.json` next to the executable: window size, VSync, bloom and TPS survive restarts.
- `docs/blocks.md`: reference of every block's rule and of the file/clipboard formats.

### Performance

- Chunked spatial index: rendering walks only the 32x32-block chunks overlapping the view instead of the whole map every frame.

### Fixed

- Zoom is smooth again: animation progress is a pure function of time (its speed used to depend on how often it was polled) and the cursor-anchor shift is applied gradually over the animation instead of jumping on scroll (Engine 1.1.1).
- CI has permission to create releases on `v*` tags.

## 2.1.0 — 2026-07-24

### Fixed

- Placing or erasing blocks while the simulation was running could corrupt the block map and crash: the render and simulation threads raced on the same hash map. The simulation now runs on the main thread from a fixed-timestep accumulator (a 65k-block tick takes ~1 ms), which removes the whole class of races.
- Copying a small selection produced a truncated zlib stream, and pasting it — or any foreign clipboard text — hung the app in an infinite decompression loop with unbounded memory growth.
- `Ctrl+V` with an empty or non-text clipboard crashed.
- Opening a corrupted `.bson` terminated the app. Loading is now atomic: a bad file is rejected with a toast and the scheme you have open stays untouched. Block type and rotation are validated on load.
- Saving over an existing file was silently rejected ("was not saved!" on every `Ctrl+S` to the same file).
- Keys unknown to GLFW (Fn, media keys) caused out-of-bounds writes in the input layer.
- Cut/copy recounted a stale selection counter and could put garbage in the clipboard; every intermediate buffer in the clipboard pipeline leaked.
- Mouse wheel events arriving in the same frame are accumulated instead of overwriting each other.
- Cursor-anchored zoom is smooth: animation progress is a pure function of time and the anchor offset is applied incrementally over the animation.
- Renderer details: invalid mag filter enum, fragment shader outputs without explicit locations, shader source leaks, shader objects never deleted, uniform cache keyed by pointer instead of string.

### Performance

- Event-driven simulation: a tick only visits currently emitting blocks and cells whose inputs could have changed, so idle parts of a scheme cost nothing regardless of size.
- Per-instance GPU data packed from a 68-byte matrix struct down to 12 bytes; the rotation matrix is built in the vertex shader from 2 bits.
- Blocks are stored by value (8 bytes each) instead of heap pointers — no leaks on erase/clear/load.
- Bloom is blurred at half resolution (~4x less fill rate across 8 blur passes).
- Mass delete erases in place instead of copying the whole map.

### Added

- Undo/redo (`Ctrl+Z` / `Ctrl+Y`, Edit menu) with gesture grouping: one paint stroke, paste or mass delete is one undo step.
- Examples menu with bundled schemes — blinker, logic gates, RS latch, full adder. They are generated by `tools/gen_examples.py`, which simulates every scheme and asserts its full truth table before writing the file.
- Whole-scheme export/import as a clipboard text string (File menu) — schemes can be shared as plain text.
- Zoom anchors to the cursor; middle mouse button picks the block type and rotation under the cursor.
- Window title shows the current scheme name and an unsaved-changes marker.
- Assets resolve relative to the executable, so it can be started from anywhere.
- doctest test suite (`ctest --test-dir build -C Release`): activation truth tables, serialization round trips, signal propagation, corrupted-input rejection, and every bundled example verified against its truth table.
- GitHub Actions CI: build + tests on every push, zip artifact, automatic GitHub release on `v*` tags.
- `static` CMake preset produces a single self-contained ~2.4 MB executable.

### Build

- One-command build via CMake presets (`cmake --preset default && cmake --build --preset release`).
- The in-house Engine library is picked up from a sibling checkout or fetched from GitHub automatically — no more hardcoded paths and manual pre-building.
