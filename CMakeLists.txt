cmake_minimum_required(VERSION 3.20)
# macOS: std::to_chars con decimales necesita macOS 13.3 o posterior
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "Versión mínima de macOS")
project(HyperEZ VERSION 0.9.0 LANGUAGES C CXX)

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

if(EMSCRIPTEN)
  # HyperEZ en el navegador (projects/web): el lenguaje, el servidor de lenguaje y una API para JavaScript.
  #   emcmake cmake -S . -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-web
  # Resultado: build-web/hyperez.js y hyperez.wasm (los usa projects/web/src/editor)
  target_compile_options(hzlang PRIVATE -fwasm-exceptions -Wall -Wno-unused-parameter -Wno-sign-compare)
  add_executable(hyperez-web src/wasm/web_api.cpp src/tools/lsp/lsp.cpp src/tools/cli/project_kinds.cpp src/tools/cli/templates.cpp)
  target_compile_options(hyperez-web PRIVATE -fwasm-exceptions)
  target_link_libraries(hyperez-web PRIVATE hzlang)
  target_link_options(hyperez-web PRIVATE -fwasm-exceptions --no-entry -sMODULARIZE=1 -sEXPORT_NAME=createHyperEZ
    -sENVIRONMENT=web,worker,node -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=8388608 -sEXIT_RUNTIME=0
    "-sEXPORTED_FUNCTIONS=['_hz_run','_hz_lsp','_hz_format','_hz_version','_hz_new_project','_hz_tests_passed','_hz_tests_failed','_malloc','_free']"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS','UTF8ToString','ENV']")
  set_target_properties(hyperez-web PROPERTIES OUTPUT_NAME hyperez SUFFIX ".js")
  return()
endif()

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
      if(WIN32 AND HYPEREZ_PGO STREQUAL "generate")
        # El ejecutable instrumentado de PGO solo sirve para entrenar: todo en un paso, sin particiones
        # (GCC 16 falla al enlazarlo con -flto-partition=one).
        set(HZ_LTO_LINK -flto=auto -flto-partition=none)
      elseif(WIN32)
        # En Windows GCC necesita make para repartir el enlace LTO en trabajos paralelos; sin él avisa de
        # que lo hace en serie. Con una sola partición no hay nada que repartir (y optimiza igual o mejor).
        set(HZ_LTO_LINK -flto=auto -flto-partition=one)
      else()
        set(HZ_LTO_LINK -flto=auto) # enlace LTO en paralelo
      endif()
      target_link_options(hyper PRIVATE ${HZ_LTO_LINK})
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
  # (y -Wstringop-overread: falsos positivos de GCC con LTO al copiar textos anchos en las cabeceras de MinGW)
  target_link_options(hyper PRIVATE -Wno-stringop-overflow -Wno-stringop-overread)
endif()
if(MINGW)
  # Ejecutables autónomos: sin DLLs de MinGW
  target_link_options(hyper PRIVATE -static -static-libgcc -static-libstdc++ -s)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT APPLE)
  # Linux: un solo ejecutable que no depende de la versión de libstdc++ del sistema
  target_link_options(hyper PRIVATE -static-libgcc -static-libstdc++)
endif()

# ---------------------------------------------------------------------------
# HyperEZ Editor (Windows): editor de código clásico con colores, errores en vivo y F5 para ejecutar
# ---------------------------------------------------------------------------
option(HYPEREZ_EDITOR "Construir HyperEZ Editor (Windows)" ON)
if(WIN32 AND HYPEREZ_EDITOR)
  add_executable(hyperez-editor WIN32 src/editor/editor.cpp src/tools/lsp/lsp.cpp packaging/windows/editor.rc)
  set_source_files_properties(packaging/windows/editor.rc PROPERTIES OBJECT_DEPENDS
    "${CMAKE_SOURCE_DIR}/packaging/windows/editor.manifest;${CMAKE_SOURCE_DIR}/src/core/version.h;${CMAKE_SOURCE_DIR}/assets/icons/hyperez.ico")
  target_link_libraries(hyperez-editor PRIVATE hzlang comctl32 comdlg32 shell32 ole32 oleaut32 uuid uxtheme)
  if(MSVC)
    target_compile_options(hyperez-editor PRIVATE /utf-8 /W3)
  else()
    target_compile_options(hyperez-editor PRIVATE -Wall -Wno-unused-parameter -Wno-sign-compare -municode)
    target_link_options(hyperez-editor PRIVATE -municode)
  endif()
  if(MINGW)
    target_link_options(hyperez-editor PRIVATE -static -static-libgcc -static-libstdc++ -s -Wno-stringop-overflow -Wno-stringop-overread)
  endif()
  if(HZ_IPO)
    set_property(TARGET hyperez-editor PROPERTY INTERPROCEDURAL_OPTIMIZATION ON)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND HZ_LTO_LINK)
      target_link_options(hyperez-editor PRIVATE ${HZ_LTO_LINK})
    endif()
  endif()
  if(HYPEREZ_PGO STREQUAL "generate" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(hyperez-editor PRIVATE "-fprofile-generate=${HYPEREZ_PGO_DIR}") # hzlang va instrumentada
  endif()
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
add_test(NAME formato COMMAND ${CMAKE_COMMAND} -DHYPER=$<TARGET_FILE:hyper> -DRAIZ=${CMAKE_SOURCE_DIR}
         -DTMP=${CMAKE_BINARY_DIR}/prueba-formato -P ${CMAKE_SOURCE_DIR}/tests/fmt/check_fmt.cmake)
add_test(NAME bytecode COMMAND ${CMAKE_COMMAND} -DHYPER=$<TARGET_FILE:hyper> -DRAIZ=${CMAKE_SOURCE_DIR}
         -P ${CMAKE_SOURCE_DIR}/tests/fmt/check_bytecode.cmake)
add_test(NAME proyectos COMMAND ${CMAKE_COMMAND} -DHYPER=$<TARGET_FILE:hyper> -DTMP=${CMAKE_BINARY_DIR}/prueba-proyectos
         -P ${CMAKE_SOURCE_DIR}/tests/fmt/check_projects.cmake)
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
  # Cada bloque ```hz de la documentación se ejecuta y debe funcionar
  file(GLOB_RECURSE HZ_DOCS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/docs/*.md)
  list(FILTER HZ_DOCS EXCLUDE REGEX "API\.md$") # referencia generada: sus ejemplos son fragmentos
  add_test(NAME documentacion COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/check_docs.py $<TARGET_FILE:hyper>
           ${HZ_DOCS} ${CMAKE_SOURCE_DIR}/README.md ${CMAKE_SOURCE_DIR}/editors/vscode/README.md)
  # La documentación generada (API, errores, bytecode) debe estar al día con el código
  add_test(NAME documentacion-generada COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/gen_docs.py $<TARGET_FILE:hyper> --check)
  # El kit para GitHub Linguist (editors/linguist) al día y con lo que miran las pruebas de Linguist
  # El compilador nativo (hyper build): una aplicación UEFI y un binario plano
  add_test(NAME nativo-uefi COMMAND hyper build ${CMAKE_SOURCE_DIR}/src/tools/native/pruebas/prueba.hez -o ${CMAKE_BINARY_DIR}/nativo-prueba.efi)
  add_test(NAME nativo-bin COMMAND hyper build ${CMAKE_SOURCE_DIR}/src/tools/native/pruebas/prueba.hez -o ${CMAKE_BINARY_DIR}/nativo-prueba.bin --formato bin)
  add_test(NAME linguist COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/linguist_kit.py --check $<TARGET_FILE:hyper>)
endif()
