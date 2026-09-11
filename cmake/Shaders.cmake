function(orbital_shader name)
  set(source "${CMAKE_SOURCE_DIR}/shaders/${name}")
  set(output "${CMAKE_BINARY_DIR}/shaders/${name}.spv")
  set(validation)
  if(SPIRV_VAL_EXECUTABLE)
    set(validation COMMAND "${SPIRV_VAL_EXECUTABLE}" --target-env vulkan1.3 "${output}")
  endif()
  add_custom_command(OUTPUT "${output}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
    COMMAND "${GLSLANG_EXECUTABLE}" -V --target-env vulkan1.3 "-I${CMAKE_SOURCE_DIR}/shaders" "${source}" -o "${output}"
    ${validation}
    DEPENDS "${source}" "${CMAKE_SOURCE_DIR}/shaders/common.glsl"
    VERBATIM COMMENT "Compiling ${name}")
  set(ORBITAL_SHADER_OUTPUTS ${ORBITAL_SHADER_OUTPUTS} "${output}" PARENT_SCOPE)
endfunction()
