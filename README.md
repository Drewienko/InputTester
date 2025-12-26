# InputTester

Cross-platform input capture playground (C++20 + Qt 6).
Focus: start with Qt input experiments, then move lower (WinAPI).

## Project Goals

- Understand the difference between `virtualKey` and `scanCode` and how they affect keyboard mapping.
- Build a portable event pipeline: platform backend -> queue -> UI, with zero allocations on the hot path.
- Separate keyboard geometry (KLE) from input code mapping.
- Start with keyboard/mouse, gamepad later.

## Status

- The app opens a Qt window and draws a full keyboard; pressed keys highlight in the UI.
- Input capture is focus-only and works on Windows and Linux (Wayland).
- Keyboard layouts are loaded from KLE JSON + mapping JSON, with clear error messages if files are invalid.

## Screenshots

![Windows](screenshots/windows.png)
![WSL](screenshots/wsl.png)

## Event Flow

```
platform backend -> inputEventSink -> inputEventQueue (SPSC) -> UI timer -> keyboardView
```

Why SPSC? The backend is a single producer and the UI thread is a single consumer, so a lock-free ring buffer
keeps allocations and locks off the hot path.

## Requirements

- CMake 3.28+
- Qt 6.10.1 (Windows: msvc2022_64, Linux: gcc_64)
- `QT_PREFIX_PATH` env var pointing at the Qt install prefix

## Build (Windows, MSVC 2022)

```powershell
$env:QT_PREFIX_PATH="C:/Qt/6.10.1/msvc2022_64"
cmake --preset windows
cmake --build --preset windows-release
```

Run:
`build-win/Release/qtKeyLog.exe`

Note: Release builds use the Windows GUI subsystem (no console window). Debug keeps the console.

Deploy (Release):

```powershell
cmake --build build-win --config Release --target deployQtKeyLog
```

Deploy (Debug):

```powershell
cmake --build build-win --config Debug --target deployQtKeyLogDebug
```

## Build (Linux / Ubuntu 22.04)

```bash
export QT_PREFIX_PATH="$HOME/Qt/6.10.1/gcc_64"
cmake --preset linux-release
cmake --build --preset linux-release
./build-linux-release/qtKeyLog
```

Note: keep separate build directories for Windows and Linux (build-win vs build-linux-\*).

## One-Click Build

Linux:

```bash
./scripts/build-linux.sh Release
```

Windows:

```powershell
.\scripts\build-win.ps1 -Configuration Release -Deploy
```

Windows (preset deploy):

```powershell
cmake --build --preset windows-release-deploy
```

## Tests

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

## Layout Import (KLE + Mapping)

Geometry uses KLE JSON (Keyboard Layout Editor). Mapping is a separate JSON file that assigns
virtualKey/scanCode by key index (order of keys in the KLE file).

Sample files:

- `layouts/ansi_tkl/ansi_tkl_kle.json`
- `layouts/ansi_tkl/ansi_tkl_mapping.json`
- `layouts/ansi_full/ansi_full_kle.json`
- `layouts/ansi_full/ansi_full_mapping.json`

Mapping format:

```json
{
  "keys": [
    { "index": 0, "virtualKey": 27, "scanCode": 1 },
    { "index": 1, "virtualKey": 49, "scanCode": 2 }
  ]
}
```

Both `virtualKey` and `scanCode` must be present per key. Index must match the KLE order.
Fn does not emit key codes, so its entry should be `virtualKey: 0` and `scanCode: 0` (it will not highlight).
For extended keys (arrows, Insert/Delete, numpad /, right Ctrl/Alt), use `scanCode + 256` in mapping.
