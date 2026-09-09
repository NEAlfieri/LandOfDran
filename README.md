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

## Packaging a release

Once you've built a release binary, `./package_release.sh` will bundle it with the files it needs at runtime (`Assets/`, `Shaders/`, `serverstart.lua`) into a single `LandOfDran-release-<hash>.tar.gz` archive.

By default it looks for the binary in `cmake-build-release/`; pass a different build directory as the first argument if yours lives elsewhere, e.g. `./package_release.sh build`.

Several of the binary's shared library dependencies (Bullet, assimp, ENet, Lua, GLEW, SDL2) use sonames that aren't stable across distro releases — a `libbullet3.24` built on Ubuntu 24.04 won't satisfy a system that only has `libbullet3.06`, for example. To avoid making players chase down exact package versions, the script copies those specific libraries into a `lib/` folder inside the archive and ships a `LandOfDran.sh` launcher that points `LD_LIBRARY_PATH` at it.

**Known limitation:** the binary (and the bundled libraries) are only guaranteed to run on a system with a glibc version *at least as new* as the one on the machine that built them — glibc compatibility only goes forward, not backward. A release built on Ubuntu 24.04 (glibc 2.39) will fail with errors like `version 'GLIBC_2.38' not found` on Ubuntu 22.04 (glibc 2.35) or older, even with the bundled libraries in place. There's currently no build step that pins an older glibc baseline, so **for now, build and package the release on the oldest distro you want players to be able to run it on** (or on the same distro/version as the players you're targeting). The proper fix would be building inside a container pinned to an older base image (e.g. Ubuntu 22.04) so the output is forward-compatible with newer systems too — not currently set up.

### Running a packaged release

Extract the archive and run `./LandOfDran.sh` (not the `LandOfDran` binary directly) — it loads the bundled libraries above and should work out of the box on most Linux desktops running a distro version at or newer than the one the release was built on (see the glibc limitation above).

The libraries left unbundled are tied to your graphics driver, display server, and audio daemon (OpenGL, X11/Wayland, ALSA/PulseAudio), which need to match the host system anyway and are already present on essentially any Linux desktop install. If launching still fails with a missing shared library error, it'll name the library — install its runtime package with your distro's package manager (e.g. `sudo apt-get install <package>` on Debian/Ubuntu; `apt-cache search <libname>` or `apt-file search <libname>` will find the package name if you're not sure). A `GLIBC_x.xx not found` error instead means the release was built on a newer distro than yours — see the limitation above.

## Community

### Website / forum

https://dran.land

### Discord

https://discord.com/invite/X9pPq2z9us
