# Ejecuta la consola interactiva con una sesión grabada y compara la salida con la esperada.
#   cmake -DHYPER=<hyper.exe> -DSESION=<sesion.txt> -DESPERADO=<sesion.out> -DRAIZ=<repositorio> -P check_repl.cmake
# Con -DACTUALIZAR=1 reescribe la salida esperada.
set(ENV{NO_COLOR} 1)
execute_process(COMMAND "${HYPER}" repl
                INPUT_FILE "${SESION}"
                OUTPUT_VARIABLE got ERROR_VARIABLE got
                ENCODING UTF8
                RESULT_VARIABLE rc
                WORKING_DIRECTORY "${RAIZ}"
                TIMEOUT 30)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "hyper repl terminó con código ${rc}:\n${got}")
endif()
string(REPLACE "\r" "" got "${got}")
if(ACTUALIZAR)
  file(WRITE "${ESPERADO}" "${got}")
  message(STATUS "Actualizado ${ESPERADO}")
  return()
endif()
file(READ "${ESPERADO}" want)
string(REPLACE "\r" "" want "${want}")
if(NOT got STREQUAL want)
  file(WRITE "${ESPERADO}.obtenido" "${got}")
  message(FATAL_ERROR "La salida de la consola no coincide con ${ESPERADO}.\n"
                      "Obtenida (guardada en ${ESPERADO}.obtenido):\n${got}")
endif()
