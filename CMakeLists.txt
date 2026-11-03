cmake_minimum_required(VERSION 3.20)
project(HyperEZ VERSION 0.5.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(HYPEREZ_LTO "Optimización en tiempo de enlace" ON)
option(HYPEREZ_INSTALLER "Construir el instalador HyperEZ-Setup.exe" ON)

file(GLOB HZ_CORE CONFIGURE_DEPENDS src/core/*.cpp)
file(GLOB HZ_LANG CONFIGURE_DEPENDS src/lang/*.cpp)

# Núcleo del lenguaje como biblioteca (lo usan hyper y las pruebas)
add_library(hzlang STATIC ${HZ_CORE} ${HZ_LANG})
target_include_directories(hzlang PUBLIC src third_party)

set(HZ_TOOLS src/tools/main.cpp src/tools/process.cpp src/tools/templates.cpp src/tools/lsp.cpp src/tools/winshell.cpp
             src/tools/repl.cpp src/tools/watch.cpp)
if(WIN32)
  list(APPEND HZ_TOOLS packaging/windows/hyper.rc)
  # windres no informa de sus dependencias: recompilar los recursos si cambian el manifiesto, la versión o los iconos
  set_source_files_properties(packaging/windows/hyper.rc PROPERTIES OBJECT_DEPENDS
    "${CMAKE_SOURCE_DIR}/packaging/windows/hyper.manifest;${CMAKE_SOURCE_DIR}/src/core/version.h;${CMAKE_SOURCE_DIR}/assets/icons/hyperez.ico;${CMAKE_SOURCE_DIR}/assets/icons/hz-file.ico")
endif()
add_executable(hyper ${HZ_TOOLS})
target_link_libraries(hyper PRIVATE hzlang)

foreach(t hzlang hyper)
  if(MSVC)
    target_compile_options(${t} PRIVATE /utf-8 /W3)
  else()
    target_compile_options(${t} PRIVATE -Wall -Wno-unused-parameter -Wno-sign-compare -Wno-trigraphs)
  endif()
endforeach()

if(HYPEREZ_LTO AND CMAKE_BUILD_TYPE STREQUAL "Release")
  include(CheckIPOSupported)
  check_ipo_supported(RESULT HZ_IPO OUTPUT HZ_IPO_MSG LANGUAGES CXX)
  if(HZ_IPO)
    set_property(TARGET hzlang hyper PROPERTY INTERPROCEDURAL_OPTIMIZATION ON)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_link_options(hyper PRIVATE -flto=auto) # enlace LTO en paralelo
    endif()
  endif()
endif()

# Optimización guiada por perfil (GCC): build.ps1 compila con "generate", entrena con
# las pruebas y los benchmarks, y recompila con "use" en la misma carpeta.
set(HYPEREZ_PGO "" CACHE STRING "Optimización guiada por perfil: vacío, generate o use")
set_property(CACHE HYPEREZ_PGO PROPERTY STRINGS "" generate use)
set(HYPEREZ_PGO_DIR "${CMAKE_BINARY_DIR}/pgo-perfil" CACHE PATH "Carpeta de los perfiles de PGO")
if(HYPEREZ_PGO AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  message(WARNING "HYPEREZ_PGO solo está soportado con GCC; se ignora")
elseif(HYPEREZ_PGO STREQUAL "generate")
  foreach(t hzlang hyper)
    target_compile_options(${t} PRIVATE "-fprofile-generate=${HYPEREZ_PGO_DIR}" -fprofile-update=prefer-atomic)
  endforeach()
  target_link_options(hyper PRIVATE "-fprofile-generate=${HYPEREZ_PGO_DIR}")
elseif(HYPEREZ_PGO STREQUAL "use")
  foreach(t hzlang hyper)
    target_compile_options(${t} PRIVATE "-fprofile-use=${HYPEREZ_PGO_DIR}" -fprofile-partial-training
                           -Wno-missing-profile -Wno-coverage-mismatch)
  endforeach()
elseif(HYPEREZ_PGO)
  message(FATAL_ERROR "HYPEREZ_PGO debe ser vacío, generate o use (es '${HYPEREZ_PGO}')")
endif()

if(WIN32)
  target_link_libraries(hyper PRIVATE shell32 ole32 uuid)
  target_link_libraries(hzlang PUBLIC shell32 ole32)
endif()
if(MINGW)
  # Ejecutables autónomos: sin DLLs de MinGW
  target_link_options(hyper PRIVATE -static -static-libgcc -static-libstdc++ -s)
endif()

# Instalador (Windows): empaqueta hyper.exe, ejemplos, documentación y la extensión de VS Code.
if(WIN32 AND HYPEREZ_INSTALLER)
  add_subdirectory(packaging/windows)
endif()

# Pruebas: ctest ejecuta "hyper test" sobre la carpeta tests/
enable_testing()
add_test(NAME lenguaje COMMAND hyper test ${CMAKE_SOURCE_DIR}/tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME ejemplos COMMAND hyper test ${CMAKE_SOURCE_DIR}/examples WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME comprobar COMMAND hyper check ${CMAKE_SOURCE_DIR}/examples ${CMAKE_SOURCE_DIR}/tests/lang ${CMAKE_SOURCE_DIR}/tests/juegos WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME consola COMMAND ${CMAKE_COMMAND} -DHYPER=$<TARGET_FILE:hyper> -DRAIZ=${CMAKE_SOURCE_DIR}
         -DSESION=${CMAKE_SOURCE_DIR}/tests/repl/sesion.txt -DESPERADO=${CMAKE_SOURCE_DIR}/tests/repl/sesion.out
         -P ${CMAKE_SOURCE_DIR}/tests/repl/check_repl.cmake)
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
  add_test(NAME documentacion COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/check_docs.py $<TARGET_FILE:hyper>
           ${CMAKE_SOURCE_DIR}/docs/GUIA.md ${CMAKE_SOURCE_DIR}/README.md ${CMAKE_SOURCE_DIR}/editors/vscode/README.md)
endif()
