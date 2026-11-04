# Incrusta un archivo binario en el ejecutable como un array de C++.
#   cmake -DIN=<archivo> -DOUT=<archivo.cpp> -DNAME=<identificador> -P embed_bin.cmake
# Genera:  const unsigned char NAME[] = {...};  const unsigned long NAME_size = ...;
file(READ "${IN}" hex HEX)
string(LENGTH "${hex}" n)
math(EXPR size "${n} / 2")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
string(REGEX REPLACE "(0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,)" "\\1\n" bytes "${bytes}")
set(code "// Generado por cmake/embed_bin.cmake a partir de ${IN}: no editar.\nnamespace hz {\nextern const unsigned char ${NAME}[];\nextern const unsigned long ${NAME}_size;\nconst unsigned char ${NAME}[] = {\n${bytes}\n};\nconst unsigned long ${NAME}_size = ${size};\n}\n")
if(EXISTS "${OUT}")
  file(READ "${OUT}" old)
  if(old STREQUAL code)
    return()
  endif()
endif()
file(WRITE "${OUT}" "${code}")
