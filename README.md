# InputTester

Cross-platform input capture playground (C++20 + Qt 6).
Focus: start with Qt input experiments, then move lower (WinAPI).

## Goals

- Separate core event model from platform backends.
- Support capture modes: global and focus-only.
- Keep event pipeline zero-allocation on hot path.
- Start with keyboard/mouse; gamepad later.

## Status

Qt Widgets window with platform backends and keyboard view.

## Requirements

- CMake 3.28+
- Qt 6.10.1 (Windows: msvc2022_64, Linux: gcc_64)

## Build (Windows, MSVC 2022)

```powershell
cmake -S . -B build-win -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/msvc2022_64"
cmake --build build-win --config Release
```

Run:
`build-win/Release/qtKeyLog.exe`

Deploy (Release):
```powershell
cmake --build build-win --config Release --target deployQtKeyLog
```

Deploy (Debug):
```powershell
cmake --build build-win --config Debug --target deployQtKeyLogDebug
```

## Build (WSL / Ubuntu 22.04)

```bash
cmake -S . -B build-wsl -G Ninja -DCMAKE_PREFIX_PATH="$HOME/Qt/6.10.1/gcc_64"
cmake --build build-wsl
./build-wsl/qtKeyLog
```

Note: always keep separate build directories for Windows and WSL (build-win vs build-wsl).

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

At least one of `virtualKey` or `scanCode` must be present per key. Index must match the KLE order.
For extended keys (arrows, Insert/Delete, numpad /, right Ctrl/Alt), use `scanCode + 256` in mapping.
