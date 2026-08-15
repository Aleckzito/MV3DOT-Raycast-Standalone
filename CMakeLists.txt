cmake_minimum_required(VERSION 3.16)

# 1. Prototipo Standalone MV3D — CERO RED / CERO SOCKETS / CERO bridge_server
project(OTRaycast-MV3D-Engine-Standalone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
endif()

# Motor original: math/grid compartidos. No se copia red.
set(RC_ENGINE_ROOT "C:/OTRaycast-MV3D" CACHE PATH "OTRaycast-MV3D root (shared math/grid only)")
set(RC_OSGVERSE_ROOT "C:/MV3D/osgverse-mv3d" CACHE PATH "osgVerse repository root")
set(RC_OSGVERSE_BUILD "${RC_OSGVERSE_ROOT}/build" CACHE PATH "osgVerse build directory")

# 1.2 Shared local: matematicas + grid DDA. EXCLUIDO: net_socket, protocolo de red.
set(RC_SHARED_MATH_GRID_SOURCES
    "${RC_ENGINE_ROOT}/shared/grid_dda.cpp"
    "${RC_ENGINE_ROOT}/shared/world_defaults.cpp"
    "${RC_ENGINE_ROOT}/shared/data_paths.cpp"
)

foreach(src IN LISTS RC_SHARED_MATH_GRID_SOURCES)
    if(NOT EXISTS "${src}")
        message(FATAL_ERROR "1.2 Missing shared source: ${src}. Set RC_ENGINE_ROOT.")
    endif()
endforeach()

# Guardia explicita: estos archivos NO deben entrar al target standalone.
set(RC_STANDALONE_FORBIDDEN_SOURCES
    "${RC_ENGINE_ROOT}/shared/net_socket.cpp"
    "${RC_ENGINE_ROOT}/bridge/ServerNetwork.cpp"
    "${RC_ENGINE_ROOT}/client/ClientNetwork.cpp"
)
foreach(forbidden IN LISTS RC_STANDALONE_FORBIDDEN_SOURCES)
    list(FIND RC_SHARED_MATH_GRID_SOURCES "${forbidden}" _idx)
    if(NOT _idx EQUAL -1)
        message(FATAL_ERROR "1.2 Network source leaked into standalone: ${forbidden}")
    endif()
endforeach()

add_library(rc_shared_math_grid STATIC
    ${RC_SHARED_MATH_GRID_SOURCES}
    "${RC_ENGINE_ROOT}/shared/grid_dda.h"
    "${RC_ENGINE_ROOT}/shared/raycast_common.h"
    "${RC_ENGINE_ROOT}/shared/world_defaults.h"
    "${RC_ENGINE_ROOT}/shared/data_paths.h"
)
target_include_directories(rc_shared_math_grid PUBLIC "${RC_ENGINE_ROOT}/shared")
target_compile_definitions(rc_shared_math_grid PUBLIC RC_STANDALONE=1)

# --- OSG / SDL2 (mismo layout que raycast_client, sin ws2_32) ---
list(APPEND CMAKE_PREFIX_PATH
    "${RC_OSGVERSE_BUILD}/sdk"
    "${RC_OSGVERSE_BUILD}/vcpkg_installed/x64-windows"
)

find_package(OpenGL REQUIRED)
find_package(SDL2 CONFIG REQUIRED)

find_package(OpenSceneGraph QUIET COMPONENTS osgViewer osgGA osgDB osgUtil OpenThreads osgText)
if(NOT OpenSceneGraph_FOUND)
    set(RC_OSG_INCLUDE_DIRS
        "${RC_OSGVERSE_BUILD}/sdk/include"
        "${RC_OSGVERSE_ROOT}/3rdparty"
    )
    set(RC_OSG_LIB_DIRS
        "${RC_OSGVERSE_BUILD}/verse_def/lib"
        "${RC_OSGVERSE_BUILD}/sdk_def/lib"
        "${RC_OSGVERSE_BUILD}/sdk/lib"
        "${RC_OSGVERSE_BUILD}/lib"
    )
    foreach(dir IN LISTS RC_OSG_LIB_DIRS)
        if(EXISTS "${dir}")
            list(APPEND RC_OSG_LIB_SEARCH "${dir}")
        endif()
    endforeach()
    foreach(lib IN ITEMS OpenThreads osg osgDB osgUtil osgGA osgViewer osgText)
        find_library(${lib}_LIB NAMES ${lib} PATHS ${RC_OSG_LIB_SEARCH} NO_DEFAULT_PATH)
        if(NOT ${lib}_LIB)
            message(FATAL_ERROR "OpenSceneGraph library ${lib} not found. Build osgVerse or set RC_OSGVERSE_BUILD.")
        endif()
        list(APPEND RC_OSG_LIBS ${${lib}_LIB})
    endforeach()
else()
    set(RC_OSG_INCLUDE_DIRS ${OPENSCENEGRAPH_INCLUDE_DIRS})
    set(RC_OSG_LIBS ${OPENSCENEGRAPH_LIBRARIES})
endif()

# Luau MIT from BASE v0.3. No sockets. Optional if path missing.
set(RC_LUAU_ROOT "C:/OTRaycast-MV3D_v0.3_BASE_2026-08-01/thirdparty/luau" CACHE PATH "Luau SDK (MIT)")
if(EXISTS "${RC_LUAU_ROOT}/CMakeLists.txt")
    set(LUAU_BUILD_WEB OFF CACHE BOOL "" FORCE)
    add_subdirectory("${RC_LUAU_ROOT}" "${CMAKE_BINARY_DIR}/luau_thirdparty" EXCLUDE_FROM_ALL)
    set(RC_HAS_LUAU ON)
else()
    set(RC_HAS_LUAU OFF)
    message(WARNING "Luau not found at RC_LUAU_ROOT; scripts disabled")
endif()

# 1.1 Target ejecutable standalone. 1.2 Sin ServerNetwork / ClientNetwork / Sockets.
add_executable(raycast_standalone
    src/standalone/main_standalone.cpp
    src/standalone/StandaloneEngine.cpp
    src/standalone/StandaloneEngine.h
    src/standalone/LocalContentRegistry.cpp
    src/standalone/LocalContentRegistry.h
    src/standalone/LocalScriptHost.cpp
    src/standalone/LocalScriptHost.h
    src/standalone/LocalInventory.cpp
    src/standalone/LocalInventory.h
    src/standalone/LocalNpc.cpp
    src/standalone/LocalNpc.h
    src/standalone/LocalQuestLog.cpp
    src/standalone/LocalQuestLog.h
    src/standalone/LocalPhysicsSolver.cpp
    src/standalone/LocalPhysicsSolver.h
    src/standalone/MiniVoxelGrid.cpp
    src/standalone/MiniVoxelGrid.h
    src/standalone/PhantomCursor.cpp
    src/standalone/PhantomCursor.h
    src/standalone/OsgPhantomRenderer.cpp
    src/standalone/OsgPhantomRenderer.h
    src/standalone/OsgDebugFloor.cpp
    src/standalone/OsgDebugFloor.h
    src/standalone/LocalChunkMesher.cpp
    src/standalone/LocalChunkMesher.h
    src/standalone/StandaloneInputHandler.cpp
    src/standalone/StandaloneInputHandler.h
    src/standalone/StandaloneWorldIO.cpp
    src/standalone/StandaloneWorldIO.h
    src/standalone/DummyActor.cpp
    src/standalone/DummyActor.h
    src/standalone/LocalProjectile.cpp
    src/standalone/LocalProjectile.h
    src/standalone/LocalEnemy.cpp
    src/standalone/LocalEnemy.h
    src/standalone/OsgHud.cpp
    src/standalone/OsgHud.h
    src/standalone/LocalLoot.cpp
    src/standalone/LocalLoot.h
    src/standalone/LocalExp.cpp
    src/standalone/LocalExp.h
    src/standalone/LocalDebris.cpp
    src/standalone/LocalDebris.h
    src/standalone/LocalFloatingText.cpp
    src/standalone/LocalFloatingText.h
    src/standalone/LocalBoxWorld.cpp
    src/standalone/LocalBoxWorld.h
    src/standalone/LocalBoulder.cpp
    src/standalone/LocalBoulder.h
    src/standalone/LocalBuddyController.cpp
    src/standalone/LocalBuddyController.h
    src/standalone/LocalArchitect.cpp
    src/standalone/LocalArchitect.h
    src/standalone/LocalFlyingBat.cpp
    src/standalone/LocalFlyingBat.h
    src/standalone/LocalCrawler.cpp
    src/standalone/LocalCrawler.h
    src/standalone/LocalCentipede.cpp
    src/standalone/LocalCentipede.h
    src/standalone/LocalCameraRig.cpp
    src/standalone/LocalCameraRig.h
)

target_include_directories(raycast_standalone PRIVATE
    src/standalone
    ${RC_OSG_INCLUDE_DIRS}
    "${RC_OSGVERSE_BUILD}/vcpkg_installed/x64-windows/include"
)

target_compile_definitions(raycast_standalone PRIVATE
    SDL_MAIN_HANDLED
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    RC_STANDALONE=1
)

# Cliente: OSG + SDL2 + OpenGL. Shared: math/grid. NO ws2_32.
target_link_libraries(raycast_standalone PRIVATE
    rc_shared_math_grid
    ${RC_OSG_LIBS}
    SDL2::SDL2
    OpenGL::GL
)

if(RC_HAS_LUAU)
    target_compile_definitions(raycast_standalone PRIVATE RC_HAS_LUAU=1)
    target_include_directories(raycast_standalone PRIVATE
        "${RC_LUAU_ROOT}/VM/include"
        "${RC_LUAU_ROOT}/Compiler/include"
        "${RC_LUAU_ROOT}/Common/include"
    )
    target_link_libraries(raycast_standalone PRIVATE Luau.VM Luau.Compiler)
endif()

add_custom_command(TARGET raycast_standalone POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/data"
        "$<TARGET_FILE_DIR:raycast_standalone>/data"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/config.json"
        "$<TARGET_FILE_DIR:raycast_standalone>/config.json"
)
