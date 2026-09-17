# CMakeList.txt : CMake project for LandOfDran, include source and define
#
# Note: Ubuntu/Debian build should work out of the box, just do cmake build.
# Windows builds need a vcpkg toolchain file passed in (see CMakePresets.json
# and the README's Windows section) but otherwise need no machine-specific
# edits to this file.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project (LandOfDran)

if(UNIX AND NOT APPLE)
	FIND_PATH( GLEW_INCLUDE_PATH GL/glew.h
	/usr/include
	/usr/local/include
	/sw/include
	/opt/local/include
	DOC "The directory where GL/glew.h resides")
	FIND_LIBRARY( GLEW_LIBRARY
	NAMES GLEW glew
	PATHS
	/usr/lib64
	/usr/lib
	/usr/local/lib64
	/usr/local/lib
	/sw/lib
	/opt/local/lib
	DOC "The GLEW library")

	IF (GLEW_INCLUDE_PATH)
	SET( GLEW_FOUND 1 CACHE STRING "Set to 1 if GLEW is found, 0 otherwise")
	ELSE (GLEW_INCLUDE_PATH)
	SET( GLEW_FOUND 0 CACHE STRING "Set to 1 if GLEW is found, 0 otherwise")
	ENDIF (GLEW_INCLUDE_PATH)

	MARK_AS_ADVANCED( GLEW_FOUND )

	FIND_PATH( ENET_INCLUDE_PATH /usr/include )
	FIND_LIBRARY( ENET_LIBRARY /usr/lib/x86_64-linux-gnu )

	FIND_PATH( OPUS_INCLUDE_PATH opus/opus.h )
	FIND_LIBRARY( OPUS_LIBRARY NAMES opus )
	if(NOT OPUS_INCLUDE_PATH OR NOT OPUS_LIBRARY)
		message(FATAL_ERROR "Opus (voice chat) not found, install libopus-dev")
	endif()
else()
	if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
		message(FATAL_ERROR
			"Windows builds need vcpkg's toolchain file. Either configure with:\n"
			"  -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake\n"
			"or set the VCPKG_ROOT environment variable and use the 'windows-debug' / "
			"'windows-release' presets in CMakePresets.json (see README.md).")
	endif()
	find_package(glew CONFIG REQUIRED)
	find_package(unofficial-enet CONFIG REQUIRED)
	find_package(Opus CONFIG REQUIRED)
endif()

find_package(ZLIB REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(Bullet CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(Lua REQUIRED)
find_package(OpenAL REQUIRED)

# libvpx decodes the VP8/VP9 in a .webm print, see Graphics/VideoPlayer.h. Only the library is looked
# for: its headers are in External/vpx, so a machine with just the runtime (a media player or browser
# pulls it in) still builds them. Without the library video prints are left out and .webm prints are
# skipped with a log line; everything else builds the same.
# The sonames are there for that runtime-only case, where nothing has made the libvpx.so symlink a
# development package would. Decoding has had the same ABI since libvpx 1.11, so any of them will do.
find_library(VPX_LIBRARY NAMES vpx vpxmd libvpx libvpx.so.9 libvpx.so.8 libvpx.so.7)
if(VPX_LIBRARY)
	message(STATUS "Video prints: using libvpx at ${VPX_LIBRARY}")
else()
	message(STATUS "Video prints: no libvpx found, .webm prints will be skipped. Install libvpx-dev (or any package with the libvpx runtime) to get them")
endif()

# Add source to this project's executable.
add_executable (LandOfDran 
"LandOfDran.cpp" 
"LandOfDran.h" 

"Utility/Logger.h" 
"Utility/Logger.cpp" 
"Utility/SettingManager.h" 
"Utility/SettingManager.cpp" 
"Utility/StringFunctions.h" 
"Utility/StringFunctions.cpp" 
"Utility/DefaultPreferences.cpp" 
"Utility/DefaultPreferences.h" 
"Utility/DefaultPreferences.cpp" 
"Utility/GlobalStartup.h" 
"Utility/GlobalStartup.cpp"
"Utility/FileFunctions.h" 
"Utility/FileFunctions.cpp"
"Utility/ExecutableArguments.h" 
"Utility/ExecutableArguments.cpp"

"External/CRC.h" 
"External/stb_image.h" 
"External/RTree.h"
"External/dr_wav.h"
"External/dr_mp3.h"
"External/vpx/vpx/vpx_decoder.h"

"External/Imgui/imgui.h"
"External/Imgui/imgui.cpp"
"External/Imgui/imgui_impl_opengl3.cpp"
"External/Imgui/imgui_impl_opengl3.h"
"External/Imgui/imgui_impl_sdl2.cpp"
"External/Imgui/imgui_impl_sdl2.h"
"External/Imgui/imgui_stdlib.cpp"
"External/Imgui/imgui_stdlib.h"
"External/Imgui/imgui_tables.cpp"
"External/Imgui/imgui_widgets.cpp"
"External/Imgui/imstb_rectpack.h"
"External/Imgui/imstb_textedit.h"
"External/Imgui/imstb_truetype.h"
"External/Imgui/imgui_internal.h"
"External/Imgui/imgui_draw.cpp"
 
"Graphics/RenderContext.h"
"Graphics/RenderContext.cpp" 
"Graphics/Shader.h" 
"Graphics/Shader.cpp" 
"Graphics/Program.h"  
"Graphics/Program.cpp"  
"Graphics/ShaderSpecification.h" 
"Graphics/ShaderSpecification.cpp" 
"Graphics/GraphicsHelpers.h" 
"Graphics/GraphicsHelpers.cpp" 
"Graphics/Texture.h" 
"Graphics/Texture.cpp"
"Graphics/Material.h" 
"Graphics/Material.cpp" 
"Graphics/Mesh.h" 
"Graphics/Mesh.cpp" 
"Graphics/DtsShape.h" 
"Graphics/DtsShape.cpp" 
"Networking/ClickAction.h" 
"Networking/ClickAction.cpp" 
"GameLoop/ClickActionPlayer.h" 
"GameLoop/ClickActionPlayer.cpp" 
"Networking/PacketsFromServer/SetClickAction.h" 
"Networking/PacketsFromServer/SetClickAction.cpp" 
"Graphics/PlayerCamera.h" 
"Graphics/PlayerCamera.cpp"  
"Graphics/Interpolator.h" 
"Graphics/Interpolator.cpp" 
"Graphics/BrickRenderer.h"
"Graphics/BrickRenderer.cpp"
"Graphics/RenderTarget.h" 
"Graphics/RenderTarget.cpp"
"Graphics/Environment.h"
"Graphics/Environment.cpp"
"Graphics/Skybox.h"
"Graphics/Skybox.cpp"
"LuaFunctions/SkyLua.h"
"LuaFunctions/SkyLua.cpp"
"Networking/PacketsFromServer/SkyboxPaths.h"
"Networking/PacketsFromServer/SkyboxPaths.cpp"
"Graphics/DayCycle.h"
"Graphics/InstancedBrickRenderer.h"
"Graphics/InstancedBrickRenderer.cpp"
"Graphics/PointLights.h"
"Graphics/PointLights.cpp"
"Graphics/GpuProfiler.cpp"
"Graphics/ItemIconRenderer.h"
"Graphics/ItemIconRenderer.cpp"
"Graphics/WaterRipples.h"
"Graphics/WaterRipples.cpp"
"Graphics/ParticleSystem.h"
"Graphics/ParticleSystem.cpp"
"Graphics/Rain.h"
"Graphics/Rain.cpp"
"Graphics/VideoPlayer.h"
"Graphics/VideoPlayer.cpp"
"Graphics/PrintVideos.h"
"Graphics/PrintVideos.cpp"

"Interface/InputMap.h"  
"Interface/InputMap.cpp" 
"Interface/UserInterface.cpp" 
"Interface/UserInterface.h" 
"Interface/SettingsMenu.h" 
"Interface/SettingsMenu.cpp" 
"Interface/DebugMenu.cpp" 
"Interface/DebugMenu.h"  
"Interface/ServerBrowser.h" 
"Interface/ServerBrowser.cpp"
"Interface/EscapeMenu.h" 
"Interface/EscapeMenu.cpp" 
"Interface/ChatWindow.h" 
"Interface/ChatWindow.cpp"

"NetTypes/NetType.h" 
"NetTypes/DynamicType.h" 
"NetTypes/DynamicType.cpp" 

"SimObjects/SimObject.h" 
"SimObjects/SimObject.cpp"
"SimObjects/Dynamic.h" 
"SimObjects/Dynamic.cpp" 
"SimObjects/StaticObject.h" 
"SimObjects/StaticObject.cpp"
"SimObjects/Light.h"
"SimObjects/Light.cpp"
"SimObjects/ParticleTypes.h"
"SimObjects/ParticleTypes.cpp"
"SimObjects/Emitter.h"
"SimObjects/Emitter.cpp"
"SimObjects/Item.h"
"SimObjects/Item.cpp"
"SimObjects/Vehicle.h"
"SimObjects/Vehicle.cpp"
 
"Networking/Server.h" 
"Networking/Server.cpp" 
"Networking/Client.cpp" 
"Networking/Client.h" 
"Networking/JoinedClient.h" 
"Networking/JoinedClient.cpp" 
"Networking/PacketEnums.h" 
"Networking/HeldServerPacket.h" 
"Networking/ClientPacketCreators.h"
"Networking/ObjHolder.h"
"Networking/Quantization.h" 
"Networking/Quantization.cpp"
"Networking/handleDisconnect.cpp"

"Networking/PacketsFromServer/AcceptConnection.h" 
"Networking/PacketsFromServer/AcceptConnection.cpp"  
"Networking/PacketsFromServer/AddSimObjectType.h" 
"Networking/PacketsFromServer/AddSimObjectType.cpp" 
"Networking/PacketsFromServer/UpdateSimObjects.h" 
"Networking/PacketsFromServer/UpdateSimObjects.cpp" 
"Networking/PacketsFromServer/DeleteSimObjects.h" 
"Networking/PacketsFromServer/DeleteSimObjects.cpp"
"Networking/PacketsFromServer/AddSimObjects.h" 
"Networking/PacketsFromServer/AddSimObjects.cpp" 
"Networking/PacketsFromServer/ChatMessageFromServer.h" 
"Networking/PacketsFromServer/ChatMessageFromServer.cpp" 
"Networking/PacketsFromServer/EvalLoginResponse.h" 
"Networking/PacketsFromServer/EvalLoginResponse.cpp" 
"Networking/PacketsFromServer/DisplayConsoleLine.h" 
"Networking/PacketsFromServer/DisplayConsoleLine.cpp" 
"Networking/PacketsFromServer/TakeOverPhysics.h" 
"Networking/PacketsFromServer/TakeOverPhysics.cpp" 
"Networking/PacketsFromServer/CameraSettings.h" 
"Networking/PacketsFromServer/CameraSettings.cpp" 
"Networking/PacketsFromServer/MovementSettings.h" 
"Networking/PacketsFromServer/MovementSettings.cpp" 
"Networking/PacketsFromServer/MeshAppearance.h" 
"Networking/PacketsFromServer/MeshAppearance.cpp" 
"Networking/PacketsFromServer/ServerPerformanceDetails.cpp"
"Networking/PacketsFromServer/ServerPerformanceDetails.h"
"Networking/PacketsFromServer/CenterPrint.h"
"Networking/PacketsFromServer/CenterPrint.cpp"
"Networking/PacketsFromServer/HighlightAppearance.h"
"Networking/PacketsFromServer/HighlightAppearance.cpp"
"Networking/PacketsFromServer/NameTag.h"
"Networking/PacketsFromServer/NameTag.cpp"
"Networking/PacketsFromServer/WorldStateUpdate.h"
"Networking/PacketsFromServer/WorldStateUpdate.cpp"
"Networking/PacketsFromServer/AddBricks.h"
"Networking/PacketsFromServer/AddBricks.cpp"
"Networking/PacketsFromServer/RemoveBricks.h"
"Networking/PacketsFromServer/RemoveBricks.cpp"
"Networking/PacketsFromServer/SpecialBrickTypes.h"
"Networking/PacketsFromServer/SpecialBrickTypes.cpp"
"Networking/PacketsFromServer/BrickPrintTypes.h"
"Networking/PacketsFromServer/BrickPrintTypes.cpp"
"Networking/PacketsFromServer/AddSoundType.h"
"Networking/PacketsFromServer/AddSoundType.cpp"
"Networking/PacketsFromServer/OneShotSound.h"
"Networking/PacketsFromServer/OneShotSound.cpp"
"Networking/PacketsFromServer/SoundLoop.h"
"Networking/PacketsFromServer/SoundLoop.cpp"
"Networking/PacketsFromServer/AudioEffect.h"
"Networking/PacketsFromServer/AudioEffect.cpp"
"Networking/PacketsFromServer/VoiceFrameFromServer.h"
"Networking/PacketsFromServer/VoiceFrameFromServer.cpp"
"Networking/PacketsFromServer/VoiceStatus.h"
"Networking/PacketsFromServer/VoiceStatus.cpp"
"Networking/PacketsFromServer/DynamicBuoyancy.h"
"Networking/PacketsFromServer/DynamicBuoyancy.cpp"
"Networking/PacketsFromServer/ParticleEmitterType.h"
"Networking/PacketsFromServer/ParticleEmitterType.cpp"
"Networking/PacketsFromServer/PlayerAbilities.h"
"Networking/PacketsFromServer/PlayerAbilities.cpp"
"Networking/PacketsFromServer/MeshDecal.h"
"Networking/PacketsFromServer/MeshDecal.cpp"
"Networking/PacketsFromServer/ItemState.h"
"Networking/PacketsFromServer/ItemState.cpp"
"Networking/PacketsFromServer/InventoryContents.h"
"Networking/PacketsFromServer/InventoryContents.cpp"
"Networking/PacketsFromServer/VehicleBricks.h"
"Networking/PacketsFromServer/VehicleBricks.cpp"
"Networking/PacketsFromServer/VehicleDriver.h"
"Networking/PacketsFromServer/VehicleDriver.cpp"
"Networking/PacketsFromServer/OpenVehicleWrench.h"
"Networking/PacketsFromServer/OpenVehicleWrench.cpp"

"Networking/PacketsFromClient/ConnectionRequest.cpp" 
"Networking/PacketsFromClient/FinishedLoading.cpp" 
"Networking/PacketsFromClient/ChatMessageSent.cpp"
"Networking/PacketsFromClient/AttemptEvalLogin.cpp" 
"Networking/PacketsFromClient/EvalCommand.cpp" 
"Networking/PacketsFromClient/ControlledPhysics.cpp" 
"Networking/PacketsFromClient/MovementInputs.cpp"
"Networking/PacketsFromClient/ClickDetails.cpp"

"GameLoop/ClientProgramData.h" 
"GameLoop/ServerProgramData.h"   
"GameLoop/Simulation.h" 
"GameLoop/LoopClient.h" 
"GameLoop/LoopClient.cpp" 
"GameLoop/LoopServer.h" 
"GameLoop/LoopServer.cpp" 
"GameLoop/ClientData.h" 
"GameLoop/ClientData.cpp"
"GameLoop/PlayerController.h" 
"GameLoop/PlayerController.cpp"

"Audio/AudioSystem.h"
"Audio/AudioSystem.cpp"
"Audio/AcousticProbe.h"
"Audio/AcousticProbe.cpp"
"Audio/SoundFile.h"
"Audio/SoundFile.cpp"
"Audio/StbVorbis.cpp"
"Audio/ReverbPresets.h"
"Audio/VoiceChat.h"
"Audio/VoiceChat.cpp"
"LuaFunctions/SoundLua.h"
"LuaFunctions/SoundLua.cpp"

"LuaFunctions/OtherFunctions.h" 
"LuaFunctions/OtherFunctions.cpp" 
"LuaFunctions/Scheduler.h" 
"LuaFunctions/Scheduler.cpp" 
"LuaFunctions/EventManager.h" 
"LuaFunctions/EventManager.cpp" 
"LuaFunctions/ClientLua.h" 
"LuaFunctions/ClientLua.cpp"  
"LuaFunctions/Dynamic.cpp" 
"LuaFunctions/Dynamic.h" 
"LuaFunctions/Static.h" 
"LuaFunctions/Static.cpp"
"LuaFunctions/LightLua.h"
"LuaFunctions/LightLua.cpp"
"LuaFunctions/EmitterLua.h"
"LuaFunctions/EmitterLua.cpp"
"LuaFunctions/ItemLua.h"
"LuaFunctions/ItemLua.cpp"
"LuaFunctions/VehicleLua.h"
"LuaFunctions/VehicleLua.cpp"

"Physics/PhysicsWorld.h" 
"Physics/PhysicsWorld.cpp" 
"Physics/SweepTest.h"

"Bricks/Brick.h"
"Bricks/BrickHolder.h"
"Bricks/BrickHolder.cpp"
"Bricks/BrickTypes.h"
"Bricks/BrickTypes.cpp"
"Bricks/PrintTypes.h"
"Bricks/PrintTypes.cpp"
"Bricks/BrickSaves.h"
"Bricks/BrickSaves.cpp"
"Bricks/GhostBrick.h"
"Bricks/GhostBrick.cpp"
"Bricks/BrickDebris.h"
"Bricks/BrickDebris.cpp"
"Bricks/SelectionBox.h"
"Bricks/SelectionBox.cpp"
"Bricks/VehicleGhost.h"
"Bricks/VehicleGhost.cpp"
"Interface/BrickHotbar.h"
"Interface/BrickHotbar.cpp"
"Interface/BrickSelector.h"
"Interface/BrickSelector.cpp"
"Interface/PaintMenu.h"
"Interface/PaintMenu.cpp"
"Interface/ItemHotbar.h"
"Interface/ItemHotbar.cpp"
"Networking/PacketsFromClient/PlantBrick.cpp"
"Networking/PacketsFromClient/VoiceFrame.cpp"
"Networking/PacketsFromClient/Inventory.cpp"
"Networking/PacketsFromClient/PaintChoice.cpp"
"Networking/PacketsFromClient/FlashlightRequest.cpp"
"Networking/PacketsFromClient/AppearanceChoice.cpp"
"GameLoop/PlayerAppearance.h"
"Interface/AppearanceEditor.h"
"Interface/AppearanceEditor.cpp"
"LuaFunctions/BrickLua.h"
"LuaFunctions/BrickLua.cpp"
"Bricks/BrickAttachments.h"
"Bricks/BrickAttachments.cpp"
"Networking/PacketsFromClient/Wrench.cpp"
"Networking/PacketsFromClient/Print.cpp"
"Networking/PacketsFromClient/PlayerGrab.cpp"
"Networking/PacketsFromClient/Slice.cpp"
"Networking/PacketsFromClient/VehicleFiles.cpp"
"Networking/PacketsFromServer/VehicleSaveData.h"
"Networking/PacketsFromServer/VehicleSaveData.cpp"
"Networking/PacketsFromServer/VehicleBricksBroken.h"
"Networking/PacketsFromServer/VehicleBricksBroken.cpp"
"Physics/RadiusImpulse.h"
"Interface/VehicleLoader.h"
"Interface/VehicleLoader.cpp"
"Networking/PacketsFromServer/OpenWrenchDialog.h"
"Networking/PacketsFromServer/OpenWrenchDialog.cpp"
"Interface/WrenchDialog.h"
"Interface/WrenchDialog.cpp"
"Networking/PacketsFromServer/OpenPrintMenu.h"
"Networking/PacketsFromServer/OpenPrintMenu.cpp"
"Interface/PrintMenu.h"
"Interface/PrintMenu.cpp"   )

target_include_directories(LandOfDran PRIVATE ${LUA_INCLUDE_DIR})

target_link_libraries(LandOfDran PRIVATE ZLIB::ZLIB)
target_link_libraries(LandOfDran PRIVATE SDL2::SDL2 SDL2::SDL2main)
target_link_libraries(LandOfDran PRIVATE OpenGL::GL)
target_link_libraries(LandOfDran PRIVATE assimp::assimp)
target_link_libraries(LandOfDran PRIVATE ${LUA_LIBRARIES})

if(VPX_LIBRARY)
	target_link_libraries(LandOfDran PRIVATE ${VPX_LIBRARY})
	target_include_directories(LandOfDran PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/External/vpx")
	target_compile_definitions(LandOfDran PRIVATE HAVE_LIBVPX)
endif()

# FindOpenAL's include dir is the AL folder itself, and the code includes <AL/al.h>
if(OPENAL_INCLUDE_DIR)
	get_filename_component(OPENAL_PARENT_INCLUDE_DIR "${OPENAL_INCLUDE_DIR}" DIRECTORY)
	target_include_directories(LandOfDran PRIVATE "${OPENAL_PARENT_INCLUDE_DIR}")
endif()
if(TARGET OpenAL::OpenAL)
	target_link_libraries(LandOfDran PRIVATE OpenAL::OpenAL)
else()
	target_link_libraries(LandOfDran PRIVATE ${OPENAL_LIBRARY})
endif()

if(UNIX AND NOT APPLE)
	target_include_directories(LandOfDran PRIVATE /usr/include/bullet)
	target_link_libraries(LandOfDran PRIVATE GLEW)
	target_link_libraries(LandOfDran PRIVATE BulletDynamics BulletCollision LinearMath)
	target_link_libraries(LandOfDran PRIVATE enet)
	target_include_directories(LandOfDran PRIVATE ${OPUS_INCLUDE_PATH})
	target_link_libraries(LandOfDran PRIVATE ${OPUS_LIBRARY})
else()
	target_link_libraries(LandOfDran PRIVATE GLEW::glew)
	target_link_libraries(LandOfDran PRIVATE ${BULLET_LIBRARIES})
	target_link_libraries(LandOfDran PRIVATE wsock32 ws2_32 winmm)
	target_link_libraries(LandOfDran PRIVATE unofficial::enet::enet)
	target_link_libraries(LandOfDran PRIVATE Opus::opus)
endif()

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET LandOfDran PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.

