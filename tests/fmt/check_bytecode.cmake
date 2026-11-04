# Prueba del desensamblador (ctest "bytecode"): todo el código del repositorio se compila y cada
# instrucción se decodifica (el listado no puede contener "??").
#   cmake -DHYPER=<hyper> -DRAIZ=<repo> -P check_bytecode.cmake
file(GLOB_RECURSE archivos "${RAIZ}/examples/*.hz" "${RAIZ}/tests/lang/*.hz" "${RAIZ}/tests/lib/*.hz" "${RAIZ}/tests/juegos/*.hz"
     "${RAIZ}/src/lib/*.hz")
set(n 0)
foreach(f ${archivos})
  execute_process(COMMAND "${HYPER}" bytecode "${f}" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING UTF8)
  if(rc)
    message(FATAL_ERROR "hyper bytecode falló con ${f}:\n${err}")
  endif()
  string(FIND "${out}" "??" malo)
  if(NOT malo EQUAL -1)
    message(FATAL_ERROR "Instrucción no decodificable en ${f}:\n${out}")
  endif()
  math(EXPR n "${n} + 1")
endforeach()
message(STATUS "${n} archivos desensamblados sin errores")
