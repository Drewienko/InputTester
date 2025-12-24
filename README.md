# InputTester

Cross-platform input capture playground (C++20 + Qt 6).
Focus: driver abstraction first, console debugging, GUI later.

## Goals
- Separate core event model from platform backends.
- Support capture modes: global and focus-only.
- Keep event pipeline zero-allocation on hot path.
- Start with keyboard/mouse; gamepad later.

## Status
Skeleton repository. See docs/architecture.md for the design sketch.
