cmake_minimum_required(VERSION 3.25)
if(NOT CMAKE_HOST_APPLE)
  message(FATAL_ERROR "Run this smoke check in a macOS graphical session.")
endif()
get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT DEFINED ORBITAL_EXECUTABLE)
  set(ORBITAL_EXECUTABLE "${root}/build/macos-debug/orbital")
endif()
if(NOT EXISTS "${ORBITAL_EXECUTABLE}")
  message(FATAL_ERROR "Build macos-debug first, or set -DORBITAL_EXECUTABLE=/path/to/orbital.")
endif()
set(output "${root}/captures/macos-smoke")
file(MAKE_DIRECTORY "${output}")
set(ENV{VK_LAYER_VALIDATE_SYNC} 1)

function(run name)
  execute_process(
    COMMAND "${ORBITAL_EXECUTABLE}" --width 640 --height 360 --frames 40 --time 0
      --capture "${output}/${name}.png" ${ARGN}
    WORKING_DIRECTORY "${root}"
    OUTPUT_FILE "${output}/${name}.log" ERROR_FILE "${output}/${name}.log"
    RESULT_VARIABLE result TIMEOUT 180)
  file(READ "${output}/${name}.log" log)
  if(NOT result STREQUAL "0" OR log MATCHES "NoGraphicsAPI validation:|Validation Error|error:|VK_ERROR"
      OR NOT log MATCHES "Completed 40 frames\\." OR NOT EXISTS "${output}/${name}.png")
    message(FATAL_ERROR "${name} failed (${result}):\n${log}")
  endif()
  file(SIZE "${output}/${name}.png" capture_size)
  if(capture_size EQUAL 0)
    message(FATAL_ERROR "Empty capture: ${name}")
  endif()
  message(STATUS "Passed: ${name}")
endfunction()

run(earth --no-hud)
run(earth-repeat --no-hud)
execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
  "${output}/earth.png" "${output}/earth-repeat.png" RESULT_VARIABLE captures_differ)
if(captures_differ)
  message(STATUS "Fixed-time captures differ; retain both for pixel comparison.")
endif()
run(belt --bookmark 5 --high --no-hud --benchmark "${output}/belt.csv")
run(ui --ui)
run(maximize --maximize-at 8 --no-hud)
run(fullscreen --fullscreen-at 8 --no-hud)
message(STATUS "macOS rendering smoke passed; captures and logs: ${output}")
