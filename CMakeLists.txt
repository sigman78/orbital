cmake_minimum_required(VERSION 3.24)
project(Orbital VERSION 0.1.0 LANGUAGES CXX)
include(CTest)
set(CMAKE_CXX_EXTENSIONS OFF)
option(ORBITAL_BUILD_DEMO "Build the native Vulkan demo" ON)

if(MSVC)
  set(ORBITAL_WARNINGS /W4 /permissive- /EHsc)
else()
  set(ORBITAL_WARNINGS -Wall -Wextra -Wpedantic)
endif()

# CPU-only libraries (see docs/CODE_STYLE.md for the layering):
#   orbital_core   logging, panic, file I/O, math
#   orbital_scene  deterministic system generation, geometry, camera
#   orbital_assets PNG I/O (vendored Wuffs decoder, stb writer), pixel kernels, materials
add_library(orbital_core STATIC src/core/log.cpp src/core/panic.cpp src/core/file.cpp)
target_include_directories(orbital_core PUBLIC src)
target_compile_features(orbital_core PUBLIC cxx_std_20)
target_compile_options(orbital_core PRIVATE ${ORBITAL_WARNINGS})

add_library(orbital_scene STATIC src/scene/system.cpp src/scene/geometry.cpp src/app/camera.cpp)
target_link_libraries(orbital_scene PUBLIC orbital_core)
target_compile_options(orbital_scene PRIVATE ${ORBITAL_WARNINGS})

add_library(orbital_assets STATIC src/assets/image.cpp src/assets/kernels.cpp src/assets/materials.cpp)
target_include_directories(orbital_assets PRIVATE third_party/stb third_party/wuffs)
target_link_libraries(orbital_assets PUBLIC orbital_core)
target_compile_options(orbital_assets PRIVATE ${ORBITAL_WARNINGS})

if(BUILD_TESTING)
  foreach(test IN ITEMS core system geometry camera image kernels materials)
    add_executable(${test}_tests tests/${test}_tests.cpp)
    target_link_libraries(${test}_tests PRIVATE orbital_scene orbital_assets)
    # These tests use assertions as executable invariants, including Release.
    if(MSVC)
      target_compile_options(${test}_tests PRIVATE ${ORBITAL_WARNINGS} /UNDEBUG)
    else()
      target_compile_options(${test}_tests PRIVATE ${ORBITAL_WARNINGS} -UNDEBUG)
    endif()
    add_test(NAME ${test} COMMAND ${test}_tests)
  endforeach()
endif()

if(ORBITAL_BUILD_DEMO)
  if(NOT WIN32)
    message(FATAL_ERROR "The desktop demo currently requires Windows. CPU tools can use ORBITAL_BUILD_DEMO=OFF.")
  endif()
  set(NOGRAPHICSAPI_FORCE_CONVENTIONAL_BACKEND ON CACHE BOOL "Use the demo's conventional descriptor-set shader ABI")
  add_subdirectory(third_party/NoGraphicsAPI EXCLUDE_FROM_ALL)
  find_program(SPIRV_VAL_EXECUTABLE NAMES spirv-val HINTS "${CMAKE_SOURCE_DIR}/.tools/Vulkan-ValidationLayers/bin" "$ENV{VULKAN_SDK}/Bin")
  include(cmake/Slang.cmake)
  foreach(shader IN ITEMS fullscreen surface beltsplat ui discsplat)
    orbital_compile_slang("shaders/${shader}.vertex.spv" "shaders/${shader}.slang" vertexMain vertex)
    list(APPEND ORBITAL_SHADER_OUTPUTS "${CMAKE_BINARY_DIR}/shaders/${shader}.vertex.spv")
  endforeach()
  foreach(shader IN ITEMS surface background atmosphere post temporal beltsplat beltblur smaa ui dust disc discsplat)
    orbital_compile_slang("shaders/${shader}.fragment.spv" "shaders/${shader}.slang" fragmentMain fragment)
    list(APPEND ORBITAL_SHADER_OUTPUTS "${CMAKE_BINARY_DIR}/shaders/${shader}.fragment.spv")
  endforeach()
  orbital_compile_slang("shaders/cull.compute.spv" "shaders/cull.slang" computeMain compute)
  list(APPEND ORBITAL_SHADER_OUTPUTS "${CMAKE_BINARY_DIR}/shaders/cull.compute.spv")
  add_custom_target(orbital_shaders DEPENDS ${ORBITAL_SHADER_OUTPUTS})

  # Dear ImGui core (vendored, MIT); its Win32 backend is compiled into the platform layer.
  add_library(imgui STATIC third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp third_party/imgui/imgui_tables.cpp
                           third_party/imgui/imgui_widgets.cpp third_party/imgui/imgui_demo.cpp)
  target_include_directories(imgui PUBLIC third_party/imgui)
  target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS IMGUI_IMPL_WIN32_DISABLE_GAMEPAD)
  target_compile_features(imgui PUBLIC cxx_std_17)

  # Platform layer: the only code that talks to the OS windowing, input and font APIs.
  add_library(orbital_platform STATIC src/platform/win32/window.cpp src/platform/win32/process.cpp src/platform/win32/text.cpp
                                      src/platform/win32/overlay.cpp third_party/imgui/backends/imgui_impl_win32.cpp)
  target_link_libraries(orbital_platform PUBLIC orbital_core imgui PRIVATE user32 gdi32 dwmapi imm32)
  target_compile_definitions(orbital_platform PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
  target_compile_options(orbital_platform PRIVATE ${ORBITAL_WARNINGS})

  add_executable(orbital src/app/main.cpp src/app/hud.cpp src/app/ui.cpp src/render/renderer_resources.cpp src/render/renderer_frame.cpp)
  target_link_libraries(orbital PRIVATE orbital_scene orbital_assets orbital_platform NoGraphicsAPI::NoGraphicsAPI)
  target_include_directories(orbital PRIVATE shaders) # scene_shared.h, the C++/shader layout
  target_compile_definitions(orbital PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
  target_compile_options(orbital PRIVATE ${ORBITAL_WARNINGS})
  add_dependencies(orbital orbital_shaders)
  add_custom_command(TARGET orbital POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_BINARY_DIR}/shaders" "$<TARGET_FILE_DIR:orbital>/shaders")
  add_custom_target(orbital_sync_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "${CMAKE_BINARY_DIR}/assets"
    COMMENT "Synchronizing material assets")
  add_dependencies(orbital orbital_sync_assets)

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
