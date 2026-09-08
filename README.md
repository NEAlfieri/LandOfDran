# LandOfDran
 Another rewrite of LoD that unifies server and client into one program
 
 This is a total rewrite of the old client and server projects you can find on my profile.
 I'm cleaning up the code and publishing this in the hopes of attracting people who would like to contribute.

## Compiling / Linking

Land of Dran uses entirely free and open source cross platform libraries so getting it to compile and run on other systems should not be difficult. The full list of used dependencies as of 03 July 2024 is as follows:

* Zlib     (Dependency of Assimp)
* Assimp   (Model loading)
* SDL2     (Context creation, input handling, client only)
* GLEW     (Context creation, client only)
* OpenGL   (Graphics, client only)
* Bullet3  (Physics engine)
* Choice of underlying networking library
  * Windows: wsock32, ws2_32, winmm
* ENet     (Reliable UDP)
* Lua      (Scripting language)

Eventually a few other libraries will be added:

* CURL     (HTTP requests for logging in and reading/posting to master server)
* OpenAL   (Playing audio, client only)

About half the dependencies are used on the client only, but at the moment they still need to be linked even if you are just building it to use as a server. Eventually dynamic libraries will be loaded at runtime.

### Windows

At the moment I'm primarily making the project as a MSVC CMake project using vcpkg to manage libraries. ImGui, stb image, and CRC++ are included with the project code itself, and all other libraries can be found on vcpkg except for ENet. 

I am using the origional version of ENet: 
https://github.com/lsalzman/enet 
It only requires system default libraries and is somewhat small and cross platform, so it should not be too difficult to build. There is a separate header only version ZPL/Enet but I have no clue how much work it would take to get to work with Land of Dran.

### Unix

Getting this working on debian/ubuntu should be as easy as:

(Have CMake and Make working beforehand, obviously)
1. Run `sudo apt-get install liblua5.4-dev libglm-dev libenet-dev zlib1g-dev libbullet-dev libassimp-dev libsdl2-dev mesa-utils libglew-dev` to get the required dependencies.
2. Clone repo / unzip to folder
3. You may need to add a build folder and move CMakeLists.txt in there
4. Navigate to folder in terminal
5. `cmake build`
6. `make`

## Community

### Website / forum

https://dran.land

### Discord

https://discord.com/invite/X9pPq2z9us
