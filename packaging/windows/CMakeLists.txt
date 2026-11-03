# Instalador de Windows: HyperEZ-Setup-<versión>.exe
add_executable(hzpack pack.cpp)

add_executable(mkicon ../icons/mkicon.cpp)
target_include_directories(mkicon PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
add_custom_target(icons
  COMMAND mkicon ${CMAKE_SOURCE_DIR}/assets/branding/mark.svg ${CMAKE_SOURCE_DIR}/assets/icons
  DEPENDS mkicon
  COMMENT "Regenerando los iconos de HyperEZ")

set(HZ_STAGE ${CMAKE_BINARY_DIR}/stage)
set(HZ_PAYLOAD ${CMAKE_BINARY_DIR}/payload.bin)
set(HZ_SRC ${CMAKE_SOURCE_DIR})
file(GLOB_RECURSE HZ_STAGE_INPUTS CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/examples/* ${CMAKE_SOURCE_DIR}/docs/*.md ${CMAKE_SOURCE_DIR}/editors/vscode/*.vsix)

add_custom_command(OUTPUT ${HZ_PAYLOAD}
  COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -DSTAGE=${HZ_STAGE} -DHYPER=$<TARGET_FILE:hyper>
          -P ${CMAKE_CURRENT_SOURCE_DIR}/stage.cmake
  COMMAND hzpack ${HZ_PAYLOAD} ${HZ_STAGE}
  DEPENDS hyper hzpack ${CMAKE_CURRENT_SOURCE_DIR}/stage.cmake ${HZ_STAGE_INPUTS} ${CMAKE_SOURCE_DIR}/README.md ${CMAKE_SOURCE_DIR}/LICENSE ${CMAKE_SOURCE_DIR}/CHANGELOG.md
  COMMENT "Empaquetando los archivos del instalador"
  VERBATIM)
add_custom_target(payload DEPENDS ${HZ_PAYLOAD})

configure_file(setup.rc.in ${CMAKE_BINARY_DIR}/setup.rc @ONLY)
set_source_files_properties(${CMAKE_BINARY_DIR}/setup.rc PROPERTIES OBJECT_DEPENDS
  "${HZ_PAYLOAD};${CMAKE_CURRENT_SOURCE_DIR}/setup.manifest;${CMAKE_SOURCE_DIR}/src/core/version.h")

add_executable(setup WIN32 setup.cpp ${CMAKE_SOURCE_DIR}/src/tools/winshell.cpp ${CMAKE_BINARY_DIR}/setup.rc)
add_dependencies(setup payload)
target_include_directories(setup PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(setup PRIVATE UNICODE _UNICODE)
set_target_properties(setup PROPERTIES OUTPUT_NAME "HyperEZ-Setup-${PROJECT_VERSION}")
target_link_libraries(setup PRIVATE comctl32 ole32 shell32 uuid)
if(MINGW)
  target_link_options(setup PRIVATE -municode -static -static-libgcc -static-libstdc++ -s)
  target_compile_options(setup PRIVATE -Wall -Wno-unused-parameter -Wno-sign-compare)
endif()
