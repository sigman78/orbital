find_program(ORBITAL_SLANGC NAMES slangc HINTS "${CMAKE_SOURCE_DIR}/.tools/slang/bin" "$ENV{VULKAN_SDK}/Bin" REQUIRED)

function(orbital_compile_slang output source entry stage)
    get_filename_component(source_absolute "${source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    get_filename_component(output_absolute "${output}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    get_filename_component(output_directory "${output_absolute}" DIRECTORY)
    set(validation)
    if(SPIRV_VAL_EXECUTABLE)
        set(validation COMMAND "${SPIRV_VAL_EXECUTABLE}" --target-env vulkan1.3 "${output_absolute}")
    endif()
    add_custom_command(
        OUTPUT "${output_absolute}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_directory}"
        COMMAND "${ORBITAL_SLANGC}" "${source_absolute}"
                -target spirv -profile spirv_1_6 -matrix-layout-column-major -fvk-use-entrypoint-name
                -entry "${entry}" -stage "${stage}" -o "${output_absolute}"
        ${validation}
        DEPENDS "${source_absolute}"
                "${CMAKE_SOURCE_DIR}/shaders/common.slang"
                "${CMAKE_SOURCE_DIR}/shaders/belt.slang"
                "${CMAKE_SOURCE_DIR}/shaders/rockclass.slang"
                "${CMAKE_SOURCE_DIR}/shaders/beltfar.slang"
                "${CMAKE_SOURCE_DIR}/shaders/clouds.slang"
                "${CMAKE_SOURCE_DIR}/shaders/surface_common.slang"
                "${CMAKE_SOURCE_DIR}/shaders/scene_shared.h"
        VERBATIM
        COMMENT "Compiling Slang ${entry} (${stage})"
    )
    set(${output} "${output_absolute}" PARENT_SCOPE)
endfunction()
