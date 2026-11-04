# Prueba del formateador (ctest "formato"):
#   1. desordenado.hz formateado debe quedar exactamente como desordenado.ordenado.hz
#   2. el código del repositorio (ejemplos, pruebas, bibliotecas) ya debe tener buen formato
#   cmake -DHYPER=<hyper> -DRAIZ=<repo> -DTMP=<carpeta> -P check_fmt.cmake
file(MAKE_DIRECTORY "${TMP}")
configure_file("${RAIZ}/tests/fmt/desordenado.hz" "${TMP}/desordenado.hz" COPYONLY)
execute_process(COMMAND "${HYPER}" fmt "${TMP}/desordenado.hz" RESULT_VARIABLE rc OUTPUT_QUIET ERROR_VARIABLE err)
if(rc)
  message(FATAL_ERROR "hyper fmt falló (${rc}): ${err}")
endif()
file(READ "${TMP}/desordenado.hz" got)
file(READ "${RAIZ}/tests/fmt/desordenado.ordenado.hz" want)
string(REPLACE "\r\n" "\n" got "${got}")
string(REPLACE "\r\n" "\n" want "${want}")
if(NOT got STREQUAL want)
  message(FATAL_ERROR "El resultado de hyper fmt no coincide con desordenado.ordenado.hz:\n${got}")
endif()
execute_process(COMMAND "${HYPER}" fmt --check "${RAIZ}/examples" "${RAIZ}/tests/lang" "${RAIZ}/tests/lib" "${RAIZ}/tests/juegos"
                        "${RAIZ}/src/lib" "${RAIZ}/tests/fmt/desordenado.ordenado.hz"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(rc)
  message(FATAL_ERROR "Hay archivos sin formato (ejecuta hyper fmt):\n${out}${err}")
endif()
message(STATUS "formato correcto")
