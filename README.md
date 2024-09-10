# Mangaka 
**Mangaka**  is a manga-style renderer. It is made with [Lum-al](https://github.com/platonvin/lum-al) and offers (limited) support for `.gltf` models. Currently not available in a form of library, but you can start your own Lum-al project from Mangaka

##### Screenshot
![](screenshots/screenshot1.png)

## Installation 
 
- ### Prerequisites 
 
  - **C++ Compiler** : [MSYS2 MinGW](https://www.msys2.org/)  recommended for Windows. For Linux, prefer GNU C++.
 
  - **Vcpkg** : follow instructions at [Vcpkg](https://vcpkg.io/en/getting-started) .
 
  - **Make** : for Windows, install with MinGW64. For Linux, typically installed by default.
 
  - **Vulkan SDK** : ensure that you have the [Vulkan SDK](https://vulkan.lunarg.com/) if you want Lum-al debug features (**on** by default)
 
- ### Steps 

  - Ensure you have a C++20 compiler, Vcpkg, and Make.
 
  - Clone the repository with all submodules:
    `$ git clone https://github.com/platonvin/mangaka.git --recurse-submodules` 
 
  - Navigate to the project directory:
`$ cd mangaka`
 
  - Install dependencies via Vcpkg:
`$ vcpkg install` 
    - On Linux, GLFW may ask you to install multiple packages. Install them in advance:
`sudo apt install libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev pkg-config build-essential`.
 
    - For MinGW on Windows specify MinGW as triplet:
`$ vcpkg install --triplet=x64-mingw-static --host-triplet=x64-mingw-static`.
 
  - Build and run the project:
`$ make -j10`

## Renderer Overview 

```mermaid
flowchart TD
%% Nodes
    Frame[\"Frame"/]
    Rpass1(["lighmap pass"])
    Rpass2(["depth (pre) pass"])
        1("depth")
        2("normals") 
    Rpass3(["shading pass"])
            3("depth-based outline")
            4("normal-based outline")
            5("Ben-Day dots shading")
            6("hatches shading")
            7("blue noise shading")
    Present[["Present"]] 

%% Edge connections between nodes
    Frame --> Rpass1
        Rpass1 --> Rpass2
    %% Graphics --> Rpass2
        Rpass2 --> 1 --> Rpass3
        Rpass2 --> 2 --> Rpass3
    %% Graphics --> Rpass3
        Rpass3 --> 3 --> Present
        Rpass3 --> 4 --> Present
        Rpass3 --> 5 --> Present
        Rpass3 --> 6 --> Present
        Rpass3 --> 7 --> Present

%% Individual node styling
    style Frame color:#1E1A1D, fill:#AFAAB9, stroke:#1B2A41

    style Present color:#1E1A1D, fill:#AFAAB9, stroke:#1B2A41

    style Rpass1 color:#1E1A1D, fill:#2C6661, stroke:#1B2A41

    style Rpass2 color:#1E1A1D, fill:#7CA975, stroke:#1B2A41
    style 1 color:#1E1A1D, fill:#7CA982, stroke:#1B2A41
    style 2 color:#1E1A1D, fill:#7CA982, stroke:#1B2A41

    style Rpass3 color:#1E1A1D, fill:#83C5B1, stroke:#1B2A41
    style 3 color:#1E1A1D, fill:#83C5BE, stroke:#1B2A41
    style 4 color:#1E1A1D, fill:#83C5BE, stroke:#1B2A41
    style 5 color:#1E1A1D, fill:#83C5BE, stroke:#1B2A41
    style 6 color:#1E1A1D, fill:#83C5BE, stroke:#1B2A41
    style 7 color:#1E1A1D, fill:#83C5BE, stroke:#1B2A41
```
## Demo Controls 

- WASD for camera movement.

- Mouse for camera orientation.

- "Esc" to close the demo.