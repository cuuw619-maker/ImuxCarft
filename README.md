# ImuxCarft

Long-term voxel sandbox project with a custom engine.

## Stack

- C++ — native engine and performance-critical systems.
- OpenGL + GLSL — initial renderer and shader pipeline.
- C# — high-level gameplay/tools layer.
- Rust — isolated systems such as networking/tooling.
- CMake — build orchestration.
- SDL2 — platform window/input layer.

The project is split into engine and game layers so the engine can evolve without rewriting gameplay.

## Initial milestone

CMake foundation, C++ engine loop, SDL2 window, OpenGL context, shader assets, voxel chunk data model, and explicit C#/Rust boundaries.

The renderer is designed for low-end integrated graphics and 8 GB RAM: chunk meshes, culling and streaming will be preferred over one render object per block.

## Build

Requirements: CMake 3.24+, C++17 compiler, SDL2 development files and OpenGL development files. .NET 8 and Rust/Cargo are optional until their modules are enabled.

    cmake -S . -B build
    cmake --build build --config Release

## Roadmap

Core → renderer → voxel chunks → meshing → terrain generation → player/physics → lighting → gameplay → networking → mod API → advanced shaders.
