# LogicalSystem
### Screenshots

![1 byte of RAM](https://img.itch.zone/aW1hZ2UvMTExNTgwOC8xMjAxMDY1Ny5wbmc=/original/YZbIZb.png)
![Blocks](https://img.itch.zone/aW1hZ2UvMTExNTgwOC8xMjAxMDY1Ni5wbmc=/original/%2FT1xPi.png)
![4 bit adder](https://img.itch.zone/aW1hZ2UvMTExNTgwOC8xMjAxMDY1OC5wbmc=/original/Ow45RS.png)
![File browser](https://img.itch.zone/aW1hZ2UvMTExNTgwOC8xMjAxMDY2MS5wbmc=/original/jS1Tjm.png)
![Settings](https://img.itch.zone/aW1hZ2UvMTExNTgwOC8xMjAxMDY1OS5wbmc=/original/3v9W5Y.png)
![Graphic settings](https://img.itch.zone/aW1hZ2UvMTExNTgwOC8xMjAxMDY2MC5wbmc=/original/3PkZMB.png)

---

### Build
- Dependencies
    - [CMake](https://cmake.org/)
    - [Glad](https://github.com/Dav1dde/glad) **(OpenGL 3.3 Core)**
    - [Glm](https://github.com/g-truc/glm)
    - [Glfw](https://www.glfw.org/)
    - [Plog](https://github.com/SergiusTheBest/plog)
    - [stb](https://github.com/nothings/stb)
    - [TurboBase64](https://github.com/powturbo/Turbo-Base64)
##### Windows
1. Download and install [**VCPKG**](https://vcpkg.io) ![](https://vcpkg.io/assets/mark/mark.svg)
2. Install dependencies
```cmd
> vcpkg install plog imgui[core,glfw-binding,opengl3-binding,freetype] glm glfw3 glad[gl-api-33] stb turbobase64
```
3. Download [My custom c++ engine](https://github.com/kewldan/Engine)
4. Replace path to Engine to your own in **CMakeLists.txt**
```cmake
include("C:/Users/kewldan/Desktop/Engine/Import.cmake")
```
##### Linux
- WIP
---

### System requirements

- **CPU:** Intel Core i5-4440 / AMD Ryzen 3 1200
- **GPU:** Intel HD 6000 / GeForce 550 / AMD R7 250 (AMD Radeon HD 7750)
- **RAM:** 2 GB
- **ROM:** 20 MB

<font style="color : orange">WARN! This is a prototype, if you find any bugs, please let me know about them or create a Pull-Request. I am not responsible for your saves at the moment, the game can just break them, you should do backups</font>