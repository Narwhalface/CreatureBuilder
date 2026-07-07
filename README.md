# CreatureBuilder OpenGL Shell

A minimal Win32 + OpenGL drawing shell for placing simple 2D shapes.

## Features

- Native Windows window and OpenGL context (WGL)
- Draw tools: circle, square, triangle
- Click-and-drag placement with live preview
- Persistent canvas (shapes remain after placement)
- Fill + stroke rendering for each shape
- Clear canvas command
- Keyboard tool switching with in-window tool indicator

## Controls

- `1`: Circle tool
- `2`: Square tool
- `3`: Triangle tool
- Left mouse drag: create and size a shape
- Right mouse button: cancel active drag
- `C`: clear all shapes

## Project Structure

- `src/main.cpp`: Win32 window lifecycle, input handling, tool switching, drag flow
- `src/shapeModel.h` / `src/shapeModel.cpp`: shape data model and drag state transitions
- `src/engineloader.h` / `src/engineloader.cpp`: OpenGL context setup and frame rendering
- `CMakeLists.txt`: CMake build configuration
- `build.bat`: convenience build script for Windows

## Build

Prerequisites:

- CMake 3.20+
- Visual Studio 2022 (or compatible MSVC toolchain)

From the repository root:

```bat
.\build.bat
```

Output executable:

```text
build\Release\CreatureBuilder.exe
```

## Notes

- Rendering uses compatibility-profile OpenGL (immediate mode) to keep the shell simple.
- This codebase is intended as a starting point for adding richer tools and editor features.
