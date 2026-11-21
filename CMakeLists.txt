cmake_minimum_required(VERSION 3.20)
project(sh4pc C)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Who the port is by: the launcher's credit roll and its title.
set(PORT_AUTHOR "phoenixd123" CACHE STRING "Name shown in the launcher's credits")
set(PORT_NAME "SH4-PC" CACHE STRING "Title shown under the menu")

# The game's C code is generated from the player's own SILENT HILL 4.exe (tools/regen.sh); it is never versioned.
file(GLOB GEN CONFIGURE_DEPENDS src/recomp/gen/*.c)
file(GLOB RT CONFIGURE_DEPENDS
     src/rt/*.c src/win32/*.c src/d3d8/*.c src/dx/*.c src/pc/*.c src/mods/*.c src/third_party/tlsf/*.c src/third_party/miniz/*.c)

add_executable(sh4-pc ${GEN} ${RT})
target_include_directories(sh4-pc PRIVATE src src/rt src/win32 src/d3d8 src/dx src/mods src/third_party src/recomp/gen)
# The lifted code wraps on purpose and reads guest memory through casts.
target_compile_options(sh4-pc PRIVATE -fwrapv -fno-strict-aliasing)
set_source_files_properties(${GEN} PROPERTIES COMPILE_OPTIONS "-w")

file(SHA1 ${CMAKE_SOURCE_DIR}/assets/splash.bmp SPLASH_SHA1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS assets/splash.bmp assets/logo.bmp assets/font.bmp)
set_source_files_properties(src/pc/launcher.c PROPERTIES COMPILE_DEFINITIONS
  "SRC_DIR=\"${CMAKE_SOURCE_DIR}\";PORT_AUTHOR=\"${PORT_AUTHOR}\";PORT_NAME=\"${PORT_NAME}\""
  OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/assets/splash.bmp;${CMAKE_SOURCE_DIR}/assets/logo.bmp;${CMAKE_SOURCE_DIR}/assets/font.bmp")

if(NOT GEN)
  # No game code: this is the launcher-only build, which is what the release zip is. Play then offers to start the
  # copy's own SILENT HILL 4.exe.
  message(STATUS "src/recomp/gen is empty: building the launcher only (run tools/regen.sh for the port itself)")
  target_compile_definitions(sh4-pc PRIVATE PORT_LAUNCHER_ONLY)
endif()

if(WIN32)
  # mingw-w64 (MSYS2) or clang, with SDL2's mingw development package.
  find_package(SDL2 REQUIRED)
  target_compile_definitions(sh4-pc PRIVATE SDL_MAIN_HANDLED _CRT_SECURE_NO_WARNINGS)
  target_compile_options(sh4-pc PRIVATE -Wno-macro-redefined)  # rt.h's WINAPI bridge vs the SDK's
  # A Windows program, not a console: the launcher has no terminal behind it, and the log is a file.
  target_link_options(sh4-pc PRIVATE -Wl,--subsystem,windows)
  target_link_libraries(sh4-pc PRIVATE SDL2::SDL2 opengl32 m user32 gdi32 winmm imm32 ole32 oleaut32 version uuid winhttp
                        advapi32 setupapi shell32 comdlg32 dinput8)
else()
  # GL is reached through SDL at run time, so nothing links libGL directly.
  find_package(SDL2 REQUIRED)
  target_link_libraries(sh4-pc PRIVATE SDL2::SDL2 m pthread)
endif()
