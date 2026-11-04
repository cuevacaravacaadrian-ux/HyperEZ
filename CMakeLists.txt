cmake_minimum_required(VERSION 3.20)
# macOS: std::to_chars con decimales necesita macOS 13.3 o posterior
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "Versión mínima de macOS")
project(HyperEZ VERSION 0.6.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(HYPEREZ_LTO "Optimización en tiempo de enlace" ON)
option(HYPEREZ_INSTALLER "Construir el instalador HyperEZ-Setup.exe (Windows)" ON)

# ---------------------------------------------------------------------------
# Núcleo del lenguaje (biblioteca hzlang)
#   src/core            utilidades comunes
#   src/lang/syntax     lexer, parser y AST
#   src/lang/sema       analizador (tipos, nombres) y diagnósticos
#   src/lang/compiler   generador de bytecode
#   src/lang/vm         máquina virtual, GC, fibras y eventos
#   src/lang/stdlib     biblioteca estándar nativa (un archivo por área)
#   src/lang/prelude    preludio escrito en HyperEZ
#   src/lib             bibliotecas escritas en HyperEZ (import ai, import stats...)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE HZ_CORE CONFIGURE_DEPENDS src/core/*.cpp)
file(GLOB_RECURSE HZ_LANG CONFIGURE_DEPENDS src/lang/*.cpp)
file(GLOB_RECURSE HZ_EMBED_SOURCES CONFIGURE_DEPENDS src/lang/prelude/*.hz src/lib/*.hz)

# El preludio y las bibliotecas .hz se incrustan en el ejecutable
set(HZ_EMBED_CPP ${CMAKE_BINARY_DIR}/generated/embedded_hz.cpp)
add_custom_command(OUTPUT ${HZ_EMBED_CPP}
  COMMAND ${CMAKE_COMMAND} -DOUT=${HZ_EMBED_CPP} -DPRELUDE=${CMAKE_SOURCE_DIR}/src/lang/prelude/prelude.hz
          -DLIBDIR=${CMAKE_SOURCE_DIR}/src/lib -P ${CMAKE_SOURCE_DIR}/cmake/embed_hz.cmake
  DEPENDS ${HZ_EMBED_SOURCES} ${CMAKE_SOURCE_DIR}/cmake/embed_hz.cmake
  COMMENT "Incrustando el preludio y las bibliotecas de HyperEZ"
  VERBATIM)

# Fuente de texto por defecto de Image.text (DejaVu Sans, licencia libre: assets/fonts/LICENSE-DejaVu.txt)
set(HZ_FONT_CPP ${CMAKE_BINARY_DIR}/generated/default_font.cpp)
add_custom_command(OUTPUT ${HZ_FONT_CPP}
  COMMAND ${CMAKE_COMMAND} -DIN=${CMAKE_SOURCE_DIR}/assets/fonts/DejaVuSans.ttf -DOUT=${HZ_FONT_CPP} -DNAME=kDefaultFont
          -P ${CMAKE_SOURCE_DIR}/cmake/embed_bin.cmake
  DEPENDS ${CMAKE_SOURCE_DIR}/assets/fonts/DejaVuSans.ttf ${CMAKE_SOURCE_DIR}/cmake/embed_bin.cmake
  COMMENT "Incrustando la fuente DejaVu Sans"
  VERBATIM)

add_library(hzlang STATIC ${HZ_CORE} ${HZ_LANG} ${HZ_EMBED_CPP} ${HZ_FONT_CPP})
target_include_directories(hzlang PUBLIC src third_party)

# ---------------------------------------------------------------------------
# Herramienta hyper (CLI, consola interactiva, watch, servidor de lenguaje)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE HZ_TOOLS CONFIGURE_DEPENDS src/tools/*.cpp)
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
  target_link_libraries(hzlang PUBLIC shell32 ole32 winhttp ws2_32)
else()
  find_package(Threads REQUIRED)
  target_link_libraries(hzlang PUBLIC Threads::Threads)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  # Falso positivo conocido de GCC con LTO dentro de stb_image_write (third_party)
  set_source_files_properties(src/lang/stdlib/image.cpp PROPERTIES COMPILE_OPTIONS "-Wno-stringop-overflow")
  target_link_options(hyper PRIVATE -Wno-stringop-overflow)
endif()
if(MINGW)
  # Ejecutables autónomos: sin DLLs de MinGW
  target_link_options(hyper PRIVATE -static -static-libgcc -static-libstdc++ -s)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT APPLE)
  # Linux: un solo ejecutable que no depende de la versión de libstdc++ del sistema
  target_link_options(hyper PRIVATE -static-libgcc -static-libstdc++)
endif()

# Instalador (Windows): empaqueta hyper.exe, ejemplos, documentación y la extensión de VS Code.
if(WIN32 AND HYPEREZ_INSTALLER)
  add_subdirectory(packaging/windows)
endif()

# ---------------------------------------------------------------------------
# Pruebas (ctest)
# ---------------------------------------------------------------------------
enable_testing()
add_test(NAME lenguaje COMMAND hyper test ${CMAKE_SOURCE_DIR}/tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME ejemplos COMMAND hyper test ${CMAKE_SOURCE_DIR}/examples WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME comprobar COMMAND hyper check ${CMAKE_SOURCE_DIR}/examples ${CMAKE_SOURCE_DIR}/tests/lang
         ${CMAKE_SOURCE_DIR}/tests/juegos ${CMAKE_SOURCE_DIR}/tests/lib ${CMAKE_SOURCE_DIR}/src/lib
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME consola COMMAND ${CMAKE_COMMAND} -DHYPER=$<TARGET_FILE:hyper> -DRAIZ=${CMAKE_SOURCE_DIR}
         -DSESION=${CMAKE_SOURCE_DIR}/tests/repl/sesion.txt -DESPERADO=${CMAKE_SOURCE_DIR}/tests/repl/sesion.out
         -P ${CMAKE_SOURCE_DIR}/tests/repl/check_repl.cmake)
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
  # Cada bloque ```hz de la documentación se ejecuta y debe funcionar
  file(GLOB_RECURSE HZ_DOCS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/docs/*.md)
  list(FILTER HZ_DOCS EXCLUDE REGEX "API\.md$") # referencia generada: sus ejemplos son fragmentos
  add_test(NAME documentacion COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/check_docs.py $<TARGET_FILE:hyper>
           ${HZ_DOCS} ${CMAKE_SOURCE_DIR}/README.md ${CMAKE_SOURCE_DIR}/editors/vscode/README.md)
  # La documentación generada (API, errores, bytecode) debe estar al día con el código
  add_test(NAME documentacion-generada COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/gen_docs.py $<TARGET_FILE:hyper> --check)
endif()
