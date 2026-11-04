# Prepara la carpeta con todo lo que instala HyperEZ-Setup.exe.
#   cmake -DSRC=<repo> -DSTAGE=<carpeta> -DHYPER=<hyper.exe> [-DEDITOR=<hyperez-editor.exe>] -P stage.cmake
file(REMOVE_RECURSE "${STAGE}")
file(MAKE_DIRECTORY "${STAGE}/bin" "${STAGE}/docs" "${STAGE}/examples" "${STAGE}/icons")
file(COPY "${HYPER}" DESTINATION "${STAGE}/bin")
if(EDITOR)
  file(COPY "${EDITOR}" DESTINATION "${STAGE}/bin") # HyperEZ Editor
endif()
execute_process(COMMAND "${HYPER}" docs "${STAGE}/docs/API.md" RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
if(rc)
  message(FATAL_ERROR "hyper docs falló (${rc})")
endif()
file(COPY "${SRC}/examples/" DESTINATION "${STAGE}/examples" PATTERN "*.out" EXCLUDE)
foreach(f README.md LICENSE CHANGELOG.md THIRD-PARTY-NOTICES.md assets/fonts/LICENSE-DejaVu.txt)
  if(EXISTS "${SRC}/${f}")
    file(COPY "${SRC}/${f}" DESTINATION "${STAGE}")
  endif()
endforeach()
file(COPY "${SRC}/docs/" DESTINATION "${STAGE}/docs" FILES_MATCHING PATTERN "*.md" PATTERN "*.png" PATTERN "*.svg")
# La documentación en HTML (para leerla sin conexión; la abre la Ayuda de HyperEZ Editor), hecha con el propio HyperEZ
execute_process(COMMAND "${HYPER}" "${SRC}/tools/docs_html.hz" "${STAGE}/docs" "${STAGE}/docs-html"
                RESULT_VARIABLE rc OUTPUT_QUIET ERROR_VARIABLE err)
if(rc)
  message(FATAL_ERROR "No se pudo generar la documentación HTML (${rc}):\n${err}")
endif()
file(COPY "${SRC}/editors/" DESTINATION "${STAGE}/editors" PATTERN "vscode" EXCLUDE) # colores para Notepad++, Sublime, Vim...
file(COPY "${SRC}/assets/icons/hz-file.ico" "${SRC}/assets/icons/hyperez.ico" DESTINATION "${STAGE}/icons")
file(GLOB vsix "${SRC}/editors/vscode/*.vsix")
if(vsix)
  list(SORT vsix)
  list(GET vsix -1 newest)
  file(MAKE_DIRECTORY "${STAGE}/vscode")
  file(COPY "${newest}" DESTINATION "${STAGE}/vscode")
else()
  message(WARNING "No hay .vsix en editors/vscode: el instalador no incluirá la extensión de VS Code")
endif()
file(COPY "${SRC}/assets/branding/banner.png" DESTINATION "${STAGE}/assets/branding") # la imagen del README instalado
