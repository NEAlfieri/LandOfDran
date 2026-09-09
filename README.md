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

The project is an MSVC CMake project using vcpkg (manifest mode) to manage libraries, including ENet — every dependency in `vcpkg.json` is fetched and built by vcpkg, so there's nothing to download or configure by hand. ImGui, stb_image, and CRC++ are included with the project code itself.

1. Install Visual Studio 2022 (or the standalone Build Tools) with the "Desktop development with C++" workload — this brings MSVC and CMake.
2. Install [vcpkg](https://github.com/microsoft/vcpkg) somewhere (`git clone https://github.com/microsoft/vcpkg && .\vcpkg\bootstrap-vcpkg.bat`), then set a `VCPKG_ROOT` environment variable pointing at that folder.
3. Open the project folder in CLion (it will pick up `CMakePresets.json` and offer the `windows` profile), or from a plain command prompt in the project root:
   ```
   cmake --preset windows
   cmake --build --preset windows-release
   ```
   The first configure will take a while — vcpkg is compiling every dependency from source for your triplet. The preset uses the Visual Studio generator, so no Developer Command Prompt or extra Ninja component is required.
4. Run `cmake-build-windows/Release/LandOfDran.exe` (or `Debug/` if you built the `windows-debug` preset). vcpkg's default `VCPKG_APPLOCAL_DEPS` behavior copies the required DLLs (SDL2, assimp, etc.) next to the executable automatically, so no manual DLL wrangling is needed.

If you'd rather not use the presets, pass `-DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake` when configuring.

### Unix

Getting this working on debian/ubuntu should be as easy as:

(Have CMake and Make working beforehand, obviously)
1. Run `sudo apt-get install liblua5.4-dev libglm-dev libenet-dev zlib1g-dev libbullet-dev libassimp-dev libsdl2-dev mesa-utils libglew-dev` to get the required dependencies.
2. Clone repo / unzip to folder
3. Navigate to folder in terminal
4. `cmake -B cmake-build-release -DCMAKE_BUILD_TYPE=Release`
5. `cmake --build cmake-build-release`

## Packaging a release

### Linux

`./package_release.sh` builds a release binary and bundles it with the files it needs at runtime (`Assets/`, `Shaders/`, `serverstart.lua`) into a single `LandOfDran-release-<hash>.tar.gz` archive. Requires Docker.

By default it builds inside a Docker container pinned to Ubuntu 22.04 (`docker/release.Dockerfile`), rather than using whatever's already built on your machine. This matters because glibc compatibility only goes forward, not backward — a binary compiled on your dev machine (say, Ubuntu 24.04's glibc 2.39) will fail with errors like `version 'GLIBC_2.38' not found` on a player's older system (e.g. Ubuntu 22.04's glibc 2.35), no matter what else you bundle with it. Building against an older, pinned baseline keeps the result compatible with that system and everything newer. Bump the base image in the Dockerfile if you need to support even older distros, or newer ones once 22.04 falls out of relevance.

Several of the binary's shared library dependencies (Bullet, assimp, ENet, Lua, GLEW, SDL2) also use sonames that aren't stable across distro releases — a `libbullet3.24` built on one distro won't satisfy a system that only has `libbullet3.06`, for example. To avoid making players chase down exact package versions, the build copies those specific libraries into a `lib/` folder inside the archive (`scripts/bundle-libs.sh`) alongside a `LandOfDran.sh` launcher that points `LD_LIBRARY_PATH` at it.

### Windows

`.\package_release.ps1` (run from a PowerShell prompt) builds the `windows-release` preset and zips the result together with `Assets/`, `Shaders/`, and `serverstart.lua` into `LandOfDran-release-<hash>-windows.zip`. There's no glibc-style ABI baseline to worry about on Windows, and vcpkg's `VCPKG_APPLOCAL_DEPS` behavior already copies every DLL the exe needs next to it in the build output, so packaging is just build-then-zip. Pass `-SkipBuild` to package whatever's already built instead of rebuilding.

For a quick local iteration loop without Docker, `./package_release.sh --local [build_dir]` packages whatever's already built in `cmake-build-release/` (or `build_dir`) instead — faster, but the result is only guaranteed to run on systems with a glibc at least as new as your own machine's, same as before.

### Running a packaged release

Extract the archive and run `./LandOfDran.sh` (not the `LandOfDran` binary directly) — it loads the bundled libraries above and should work out of the box on most Linux desktops at or newer than the Docker image's base distro.

The libraries left unbundled are tied to your graphics driver, display server, and audio daemon (OpenGL, X11/Wayland, ALSA/PulseAudio), which need to match the host system anyway and are already present on essentially any Linux desktop install. If launching still fails with a missing shared library error, it'll name the library — install its runtime package with your distro's package manager (e.g. `sudo apt-get install <package>` on Debian/Ubuntu; `apt-cache search <libname>` or `apt-file search <libname>` will find the package name if you're not sure). A `GLIBC_x.xx not found` error means the release was built for a newer baseline than your system — ask whoever packaged it to lower the Dockerfile's base image version (or use `--local` builds only for testing on your own machine).

## Community

### Website / forum

https://dran.land

### Discord

https://discord.com/invite/X9pPq2z9us
