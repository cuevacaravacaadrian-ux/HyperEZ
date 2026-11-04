" Vim: colores de HyperEZ (.hez y .hz). Copia syntax/ y ftdetect/ en ~/.vim (o ~/.config/nvim). Generado por tools/gen_editors.py
if exists("b:current_syntax")
  finish
endif

syn keyword hzKeyword var let const function return if else for foreach in while repeat break continue class struct constructor this super extends implements interface enum import from as public private protected static new try catch finally throw async await match event on once emit step get set and or not is
syn keyword hzConstant true false null
syn keyword hzType number int text bool list map set any void function Task Range Event
syn keyword hzBuiltin print println write input len typeof str toNumber toInt assert assertEqual assertThrows test exit range time wait spawn abs sqrt pow floor ceil round min max clamp random randomInt randomChoice randomSeed Console File System JSON Path Time Process Http Regex Bytes Hash Matrix Image Vec2 Vec3 Color Error
syn match hzNumber "\<\(0[xX][0-9a-fA-F_]\+\|0[bB][01_]\+\|[0-9][0-9_]*\(\.[0-9_]\+\)\=\([eE][+-]\=[0-9]\+\)\=\)\>"
syn match hzClassName "\<[A-Z][A-Za-z0-9_]*\>"
syn region hzInterp matchgroup=hzInterpDelim start="${" end="}" contained contains=TOP
syn region hzString start=+"+ skip=+\\.+ end=+"+ contains=hzInterp,hzEscape
syn region hzString start=+'+ skip=+\\.+ end=+'+ contains=hzInterp,hzEscape
syn region hzString start=+"""+ end=+"""+ contains=hzInterp
syn region hzRawString start=+r"+ end=+"+
syn region hzRawString start=+r'+ end=+'+
syn match hzEscape "\\." contained
syn match hzDocComment "///.*$"
syn match hzComment "//.*$" contains=hzTodo
syn region hzComment start="/\*" end="\*/" contains=hzTodo
syn keyword hzTodo TODO FIXME PENDIENTE contained
syn match hzOperator "|>\|=>\|->\|\.\.=\|\.\.\|??\|?\."

hi def link hzKeyword Keyword
hi def link hzConstant Constant
hi def link hzType Type
hi def link hzClassName Type
hi def link hzBuiltin Function
hi def link hzNumber Number
hi def link hzString String
hi def link hzRawString String
hi def link hzEscape SpecialChar
hi def link hzInterpDelim Special
hi def link hzComment Comment
hi def link hzDocComment SpecialComment
hi def link hzTodo Todo
hi def link hzOperator Operator

let b:current_syntax = "hyperez"
