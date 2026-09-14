cmake_minimum_required(VERSION 3.24)
project(Orbital VERSION 0.1.0 LANGUAGES CXX)
include(CTest)
set(CMAKE_CXX_EXTENSIONS OFF)
option(ORBITAL_BUILD_DEMO "Build the native Vulkan demo" ON)
option(ORBITAL_RENDER_TESTS "Enable desktop GPU tests requiring local Vulkan validation layers" OFF)

if(MSVC)
  set(ORBITAL_WARNINGS /W4 /permissive- /EHsc)
else()
  set(ORBITAL_WARNINGS -Wall -Wextra -Wpedantic)
endif()

# CPU-only libraries (see docs/CODE_STYLE.md for the layering):
#   orbital_core   logging, panic, file I/O, math
#   orbital_scene  deterministic system generation, geometry
#   orbital_app_cpu app navigation and command-line parsing, independent of the renderer
#   orbital_render_cpu showcase contract, state binding and frame calculations
#   orbital_assets PNG I/O (vendored Wuffs decoder, stb writer), pixel kernels, materials
add_library(orbital_core STATIC src/core/log.cpp src/core/panic.cpp src/core/file.cpp)
target_include_directories(orbital_core PUBLIC src)
target_compile_features(orbital_core PUBLIC cxx_std_20)
target_compile_options(orbital_core PRIVATE ${ORBITAL_WARNINGS})

add_library(orbital_scene STATIC src/scene/system.cpp src/scene/geometry.cpp)
target_link_libraries(orbital_scene PUBLIC orbital_core)
target_compile_options(orbital_scene PRIVATE ${ORBITAL_WARNINGS})

add_library(orbital_render_cpu STATIC src/render/showcase.cpp src/render/frame_calculations.cpp)
target_link_libraries(orbital_render_cpu PUBLIC orbital_scene)
target_compile_options(orbital_render_cpu PRIVATE ${ORBITAL_WARNINGS})

add_library(orbital_app_cpu STATIC src/app/camera.cpp src/app/options.cpp)
target_link_libraries(orbital_app_cpu PUBLIC orbital_scene)
target_compile_options(orbital_app_cpu PRIVATE ${ORBITAL_WARNINGS})

add_library(orbital_assets STATIC src/assets/image.cpp src/assets/kernels.cpp src/assets/materials.cpp src/assets/texture.cpp)
set(smaa_source "${CMAKE_CURRENT_BINARY_DIR}/generated/smaa.cpp")
add_custom_command(
  OUTPUT "${smaa_source}"
  COMMAND "${CMAKE_COMMAND}"
    "-DINPUT_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/third_party/smaa"
    "-DOUTPUT=${smaa_source}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateSmaa.cmake"
  DEPENDS third_party/smaa/area.bin third_party/smaa/search.bin cmake/GenerateSmaa.cmake
  VERBATIM
  COMMENT "Embedding SMAA lookup binaries")
target_sources(orbital_assets PRIVATE "${smaa_source}")
target_include_directories(orbital_assets PRIVATE third_party/stb third_party/wuffs)
target_link_libraries(orbital_assets PUBLIC orbital_core)
target_compile_options(orbital_assets PRIVATE ${ORBITAL_WARNINGS})

# Optional offline encoders; the demo and its normal build do not depend on them.
option(ORBITAL_BUILD_TEXTURE_TOOLS "Build optional ASTC/BC7 encoders and material preparation tool" OFF)
if(ORBITAL_BUILD_TEXTURE_TOOLS)
  set(ASTCENC_CLI ON CACHE BOOL "Build astcenc CLI")
  set(ASTCENC_ISA_NATIVE ON)
  set(ASTCENC_WERROR OFF CACHE BOOL "Keep vendored warnings non-fatal")
  add_subdirectory(third_party/astcenc EXCLUDE_FROM_ALL)
  add_executable(texture_prepare EXCLUDE_FROM_ALL tools/texture_prepare.cpp)
  target_link_libraries(texture_prepare PRIVATE orbital_assets)
  target_compile_options(texture_prepare PRIVATE ${ORBITAL_WARNINGS})
  add_library(bc7enc STATIC EXCLUDE_FROM_ALL third_party/bc7enc/bc7enc.cpp)
  target_include_directories(bc7enc PUBLIC third_party/bc7enc)
  find_package(Threads REQUIRED)
  add_executable(bc7_compress EXCLUDE_FROM_ALL tools/bc7_compress.cpp)
  target_link_libraries(bc7_compress PRIVATE orbital_core bc7enc Threads::Threads)
  target_compile_options(bc7_compress PRIVATE ${ORBITAL_WARNINGS})
  add_custom_target(texture_tools DEPENDS texture_prepare astcenc-native bc7_compress)
endif()

# Opt-in measurement executable; its allocation instrumentation never enters the demo.
add_executable(storage_profile EXCLUDE_FROM_ALL tools/storage_profile.cpp)
target_link_libraries(storage_profile PRIVATE orbital_scene orbital_assets)
target_compile_options(storage_profile PRIVATE ${ORBITAL_WARNINGS})

if(BUILD_TESTING)
  if(NOT MINGW) # NoGraphicsAPI headers explicitly exclude MinGW.
    add_executable(gpu_scopes_tests tests/gpu_scopes_tests.cpp)
    target_compile_features(gpu_scopes_tests PRIVATE cxx_std_20)
    target_include_directories(gpu_scopes_tests PRIVATE third_party/NoGraphicsAPI/include third_party/NoGraphicsAPI/utility/include)
    if(MSVC)
      target_compile_options(gpu_scopes_tests PRIVATE ${ORBITAL_WARNINGS} /UNDEBUG)
    else()
      target_compile_options(gpu_scopes_tests PRIVATE ${ORBITAL_WARNINGS} -UNDEBUG)
    endif()
    add_test(NAME gpu_scopes COMMAND gpu_scopes_tests)
  endif()
  foreach(test IN ITEMS core system geometry camera image kernels materials smaa texture app showcase frame_calculations options)
    add_executable(${test}_tests tests/${test}_tests.cpp)
    target_link_libraries(${test}_tests PRIVATE orbital_scene orbital_assets)
    if(test STREQUAL "showcase" OR test STREQUAL "frame_calculations")
      target_link_libraries(${test}_tests PRIVATE orbital_render_cpu)
    endif()
    if(test STREQUAL "camera" OR test STREQUAL "app" OR test STREQUAL "options")
      target_link_libraries(${test}_tests PRIVATE orbital_app_cpu)
    endif()
    # These tests use assertions as executable invariants, including Release.
    if(MSVC)
      target_compile_options(${test}_tests PRIVATE ${ORBITAL_WARNINGS} /UNDEBUG)
    else()
      target_compile_options(${test}_tests PRIVATE ${ORBITAL_WARNINGS} -UNDEBUG)
    endif()
    add_test(NAME ${test} COMMAND ${test}_tests)
  endforeach()
  set_tests_properties(smaa PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/third_party/smaa")
endif()

if(ORBITAL_BUILD_DEMO)
  if(NOT WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "The desktop demo supports Windows and Linux. CPU tools can use ORBITAL_BUILD_DEMO=OFF.")
  endif()
  set(NOGRAPHICSAPI_FORCE_CONVENTIONAL_BACKEND ON CACHE BOOL "Use the demo's conventional descriptor-set shader ABI")
  add_subdirectory(third_party/NoGraphicsAPI EXCLUDE_FROM_ALL)
  find_program(SPIRV_VAL_EXECUTABLE NAMES spirv-val HINTS "${CMAKE_SOURCE_DIR}/.tools/Vulkan-ValidationLayers/bin" "$ENV{VULKAN_SDK}/Bin")
  include(cmake/Slang.cmake)
  foreach(source IN ITEMS scene/fullscreen surface/surface belt/beltsplat overlay/ui belt/discsplat scene/motes sky/stars)
    get_filename_component(shader "${source}" NAME)
    orbital_compile_slang("shaders/${shader}.vertex.spv" "shaders/${source}.slang" vertexMain vertex)
    list(APPEND ORBITAL_SHADER_OUTPUTS "${CMAKE_BINARY_DIR}/shaders/${shader}.vertex.spv")
  endforeach()
  foreach(source IN ITEMS
      planets/surface_earth planets/surface_giant planets/surface_airless planets/atmosphere
      surface/surface_rock sky/background sky/galaxy sky/stars
      belt/beltsplat belt/beltblur belt/dust belt/disc belt/discsplat
      aa/temporal aa/smaa aa/fxaa_pass
      post/bloom post/composite post/meter post/present post/sun_visibility overlay/ui scene/motes)
    get_filename_component(shader "${source}" NAME)
    orbital_compile_slang("shaders/${shader}.fragment.spv" "shaders/${source}.slang" fragmentMain fragment)
    list(APPEND ORBITAL_SHADER_OUTPUTS "${CMAKE_BINARY_DIR}/shaders/${shader}.fragment.spv")
  endforeach()
  orbital_compile_slang("shaders/cull.compute.spv" "shaders/belt/cull.slang" computeMain compute)
  list(APPEND ORBITAL_SHADER_OUTPUTS "${CMAKE_BINARY_DIR}/shaders/cull.compute.spv")
  add_custom_target(orbital_shaders DEPENDS ${ORBITAL_SHADER_OUTPUTS})

  # Dear ImGui core (vendored, MIT); its OS backend is compiled into the platform layer.
  add_library(imgui STATIC third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp third_party/imgui/imgui_tables.cpp
                           third_party/imgui/imgui_widgets.cpp third_party/imgui/imgui_demo.cpp)
  target_include_directories(imgui PUBLIC third_party/imgui)
  target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS IMGUI_IMPL_WIN32_DISABLE_GAMEPAD)
  target_compile_features(imgui PUBLIC cxx_std_17)

  # Platform layer: the only code that talks to the OS windowing, input and font APIs.
  if(WIN32)
    add_library(orbital_platform STATIC src/platform/win32/window.cpp src/platform/win32/process.cpp src/platform/win32/text.cpp
                                        src/platform/win32/overlay.cpp third_party/imgui/backends/imgui_impl_win32.cpp)
    target_link_libraries(orbital_platform PUBLIC orbital_core imgui PRIVATE user32 gdi32 dwmapi imm32)
    target_compile_definitions(orbital_platform PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
  else()
    find_package(SDL2 REQUIRED CONFIG)
    find_package(Freetype REQUIRED)
    find_package(Fontconfig REQUIRED)
    add_library(orbital_platform STATIC src/platform/linux/window.cpp src/platform/linux/process.cpp
      src/platform/linux/text.cpp src/platform/linux/overlay.cpp third_party/imgui/backends/imgui_impl_sdl2.cpp)
    target_link_libraries(orbital_platform PUBLIC orbital_core imgui PRIVATE SDL2::SDL2 Freetype::Freetype Fontconfig::Fontconfig)
  endif()
  target_compile_options(orbital_platform PRIVATE ${ORBITAL_WARNINGS})

  add_executable(orbital src/app/main.cpp src/app/hud.cpp src/app/ui.cpp src/app/ui_panel.cpp src/render/renderer_resources.cpp src/render/gpu_image.cpp src/render/renderer_frame.cpp
    src/render/renderer_frame_data.cpp src/render/renderer_pipelines.cpp src/render/renderer_assets.cpp
    src/render/renderer_belt.cpp src/render/renderer_scene.cpp src/render/renderer_post.cpp src/render/renderer_overlay.cpp)
  target_link_libraries(orbital PRIVATE orbital_scene orbital_app_cpu orbital_render_cpu orbital_assets orbital_platform NoGraphicsAPIUtility::commands)
  target_include_directories(orbital PRIVATE shaders) # scene_shared.h, the C++/shader layout
  target_compile_definitions(orbital PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
  target_compile_options(orbital PRIVATE ${ORBITAL_WARNINGS})
  add_dependencies(orbital orbital_shaders)
  add_custom_command(TARGET orbital POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_BINARY_DIR}/shaders" "$<TARGET_FILE_DIR:orbital>/shaders")
  add_custom_target(orbital_sync_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "${CMAKE_BINARY_DIR}/assets"
    COMMENT "Synchronizing material assets")
  add_dependencies(orbital orbital_sync_assets)

  if(BUILD_TESTING AND ORBITAL_RENDER_TESTS)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_executable(render_validation_probe tests/render_validation_probe.cpp src/render/gpu_image.cpp)
    target_link_libraries(render_validation_probe PRIVATE orbital_core NoGraphicsAPIUtility::commands)
    target_compile_options(render_validation_probe PRIVATE ${ORBITAL_WARNINGS})
    add_test(NAME renderer_validation COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/check-renderer.py"
      --build "$<TARGET_FILE_DIR:orbital>")
    set_tests_properties(renderer_validation PROPERTIES LABELS "gpu;validation" RUN_SERIAL TRUE TIMEOUT 1800)
  endif()

  install(TARGETS orbital RUNTIME DESTINATION .)
  install(DIRECTORY "${CMAKE_BINARY_DIR}/shaders" DESTINATION .)
  install(DIRECTORY assets DESTINATION .)
  install(FILES README.md DESTINATION .)
  install(DIRECTORY docs DESTINATION .)
  install(FILES third_party/NoGraphicsAPI/LICENSE DESTINATION licenses RENAME NoGraphicsAPI-LICENSE.txt)
  install(FILES third_party/stb/LICENSE DESTINATION licenses RENAME stb-LICENSE.txt)
  install(FILES third_party/wuffs/LICENSE DESTINATION licenses RENAME wuffs-LICENSE.txt)
  install(FILES third_party/smaa/LICENSE.txt DESTINATION licenses RENAME SMAA-LICENSE.txt)
  install(FILES third_party/imgui/LICENSE.txt DESTINATION licenses RENAME imgui-LICENSE.txt)
endif()
