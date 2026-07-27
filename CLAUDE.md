# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

Lightpack/Prismatik is an open-source USB ambient-lighting ("Ambilight") system with three parts that live in this one repo:

- `Software/` — **Prismatik**, the cross-platform Qt (C++17) desktop app that grabs the screen, computes per-LED colors, and drives the device (this is where almost all day-to-day work happens).
- `Firmware/` — AVR firmware (LUFA USB stack) that runs on the Lightpack hardware itself.
- `Hardware/` — schematics/board files (Eagle `.sch`/`.brd`).
- `docs/` — design/research docs written for this fork (pipeline walkthrough, bottleneck analysis, UI redesign prototype). Read `docs/README.md` first; `docs/pipeline-captura-processamento-leds.md` is the best single reference for how capture → color → device → firmware → LED fits together, with file-level pointers.

This is a fork ("Prismatik flavour") of the original woodenshark/Lightpack project, with Windows-focused improvements (notably Desktop Duplication API grabbing).

## Build commands

All software work happens under `Software/`.

**Linux:**
```sh
cd Software
./update_locales.sh   # only if translations changed
qmake -r
make                  # binary lands in Software/bin
```
Requires: `qtbase5-dev qtchooser qttools5-dev-tools libqt5serialport5-dev build-essential pkg-config libusb-1.0-0-dev libudev-dev`, plus `libpulse-dev libfftw3-dev` for the sound visualizer (toggle via `PULSEAUDIO_SUPPORT` in `Software/build-vars.prf`, copy from `build-vars.prf.default`).

**macOS / Windows:** see the build sections in `README.md` (Xcode/qmake for macOS; Visual Studio + `scripts/win32/generate_sln.bat` for Windows). Windows-only subprojects (`libraryinjector`, `hooks`, `unhook`, `offsetfinder`) only build under `win32` per `Lightpack.pro`.

**Packaging (Linux):**
```sh
cd Software/dist_linux
./build-natively.sh <package-backend>          # e.g. dpkg, pacman, flatpak
./build-in-docker.sh <backend> <image> <tag>    # cross-distro via Docker
```

**Firmware:**
```sh
cd Firmware
make LIGHTPACK_HW=7          # hardware versions 4-7
./build_batch.sh             # builds all HW versions into Firmware/hex
make dfu LIGHTPACK_HW=7      # flash (device must be in bootloader mode)
```

## Tests

Tests use QtTest and build as a separate qmake subproject (`Software/tests`, only added to `SUBDIRS` on `win32` in `Lightpack.pro` — on Linux/macOS build it directly):
```sh
cd Software/tests
qmake tests.pro && make
./bin/LightpackTests                       # runs all suites
./bin/LightpackTests -functions            # list test cases in a suite
```
`TestsMain.cpp` registers each suite (`GrabCalculationTest`, `LightpackMathTest`, `LightpackApiTest`, `AppVersionTest`, `LightpackCommandLineParserTest`, plus `HooksTest` on Windows). To run one suite standalone, pass its class name via `argv` the way `QTest::qExec` expects, or comment out the others in `TestsMain.cpp` temporarily.

## Architecture: the Ambilight pipeline

The core data flow (fully diagrammed in `docs/pipeline-captura-processamento-leds.md`) is:

```
screen pixels → per-zone average color → GrabManager post-processing
  → LedDeviceManager (device thread) → USB HID / Serial / UDP
  → Firmware (smoothing) → SPI/PWM → LED
```

Key pieces, in the order data flows through them:

1. **`Software/src/LightpackApplication.cpp`** — app entry point; wires `GrabManager` to `LedDeviceManager` and starts the backlight. The critical cross-thread connection:
   ```cpp
   connect(m_grabManager, &GrabManager::updateLedsColors,
           m_ledDeviceManager, &LedDeviceManager::setColors,
           Qt::QueuedConnection);
   ```
2. **`Software/grab/GrabberBase.cpp`** — the actual "main loop" is a `QTimer` (default ~50ms, configurable 1–1000ms via `Settings::getGrabSlowdown()`), not a thread loop. Each tick: find monitors intersecting LED zones, grab the framebuffer per-platform, then average pixels per `GrabWidget` zone.
3. **Platform grabbers** (`Software/grab/`): `DDuplGrabber` (Windows, Desktop Duplication API — preferred, this fork's headline feature), `WinAPIGrabber` (GDI fallback), `D3D10Grabber` (fullscreen games), `X11Grabber` (Linux, XShm), `MacOSCGGrabber`/`MacOSAVGrabber` (macOS). All fill a common `GrabbedScreen` struct; averaging itself is centralized in `Software/grab/calculations.cpp` (`calculateAvgColor`, with scalar/SSE4.1/AVX2/AVX512 paths chosen at runtime).
4. **`Software/src/GrabManager.cpp`** — owns the `GrabWidget` zones (one per LED) and post-processes the averaged colors: color temperature / Night Light, optional global averaging across all LEDs, overbrighten, then diffs against the previous frame before emitting `updateLedsColors`.
5. **`Software/src/LedDeviceManager.*`** — runs device I/O on a dedicated `QThread`, queues at most one in-flight command, handles reconnect/backoff on failure.
6. **`Software/src/AbstractLedDevice.cpp`** — device-agnostic color pipeline: 8-bit → 12-bit, gamma correction, Lab-space luminosity threshold, brightness, per-LED white balance, brightness caps, power-supply current limiting, then dithering.
7. **Device backends** (`Software/src/LedDevice*.cpp`) — one per protocol: `LedDeviceLightpack` (USB HID, `hid_write`), `LedDeviceAdalight`/`LedDeviceArdulight` (serial), `LedDeviceWarls`/`LedDeviceDrgb`/`LedDeviceDnrgb` (Wi-Fi UDP, WLED-compatible), `LedDeviceAlienFx`, `LedDeviceVirtual`. All implement `AbstractLedDevice`/`AbstractLedDeviceUdp`; only the final packing/transport differs.
8. **`Firmware/`** — `LightpackUSB.c` parses `CMD_UPDATE_LEDS` (command IDs shared with the host via `CommonHeaders/COMMANDS.h`), `LedManager.c` linearly interpolates `start → end` per-frame on the Timer1 ISR (temporal smoothing), `LedDriver.c` pushes 12-bit values over bit-banged SPI (or software PWM on older hardware) to the LED strip.

Other modes — **Mood Lamp** (`MoodLamp*.cpp`) and **Sound Visualizer** (`SoundVisualizer.cpp`, `*SoundManager.cpp`) — feed `LedDeviceManager` directly and skip the grabber entirely.

Other components worth knowing about:
- **`Software/src/ApiServer.cpp`** — the network API (TCP, documented externally) that lets other apps control LEDs; see `Software/apiexamples` for client samples.
- **`Software/src/PluginsManager.cpp` / `Plugin.cpp` / `LightpackPluginInterface.cpp`** — plugin loading; see `Software/res/plugin-template.ini`.
- **`Software/src/Settings.cpp` / `SettingsDefaults.hpp`** — persisted configuration; `--config-dir` allows running multiple instances (one per monitor) per the multi-monitor workaround documented in the README.
- **`Software/math/`** and **`Software/grab/`** build as static libs (`libprismatik-math`, `libgrab`) that `src` and `tests` both link against — qmake subdirs (`Lightpack.pro`) enforce `src.depends = math grab`.

## Notes

- C++17, Qt 5 widgets/network (+ serialport on win32/macx). Qt Creator `.pro`/qmake project files define the build graph — there is no CMake.
- Grabber availability per platform is controlled by `Software/grab/configure-grabbers.prf` (`X11_GRAB_SUPPORT`, `MAC_OS_CG_GRAB_SUPPORT`, `MAC_OS_AV_GRAB_SUPPORT`, `WINAPI_GRAB_SUPPORT`, `DDUPL_GRAB_SUPPORT`, `D3D10_GRAB_SUPPORT`), not by `#ifdef Q_OS_*` scattered in code.
- Optional features are toggled via `DEFINES` in `Software/build-vars.prf` (copy from `build-vars.prf.default`): `PULSEAUDIO_SUPPORT`/BASS for sound viz, `NIGHTLIGHT_SUPPORT`, `NO_OPENSSL`.
- `Software/UpdateElevate` is a git submodule (Windows auto-updater elevation helper) — run `git submodule update --init` if it's missing.

## agy-bridge (delegação opcional)

Se as ferramentas MCP do `agy-bridge` estiverem disponíveis, você pode usá-las
para tarefas que consumiriam muito contexto — analisar muitos arquivos grandes,
investigação extensa de git, ou lookups na web. É opcional, não obrigatório:
use seu julgamento sobre se delegar realmente compensa. Sempre mantenha os
prompts normais de permissão ativos.

Quando aplicável, você pode rodar o `agy` em paralelo ao seu próprio trabalho
(ex.: delegar uma investigação enquanto segue com outra parte da tarefa) em vez
de esperar a resposta antes de continuar — mas só quando as duas linhas de
trabalho forem de fato independentes.
