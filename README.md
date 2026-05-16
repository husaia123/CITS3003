# CITS3003 Project

## Operating System

This project is configured and tested on **Windows**.

## Requirements

- CMake (version 3.5 or newer recommended)
- A C++ compiler compatible with your chosen Visual Studio or MSVC toolchain
- OpenGL-compatible graphics drivers

## Project Structure

- `Project Base/` - main project source and build files
- `Project Base/src/` - C++ source code
- `Project Base/res/` - models, shaders, and textures
- `Project Base/lib/` - third-party libraries used by the project

## Build and Run Instructions

1. Open a terminal and navigate to the project root:
   ```powershell
   cd "Project Base"
   ```
2. Generate the build files:
   ```powershell
   cmake -S . -B build
   ```
3. Build the project in Debug configuration:
   ```powershell
   cmake --build build --config Debug
   ```
4. Run the executable:
   ```powershell
   .\build\Debug\cits3003_project.exe
   ```
