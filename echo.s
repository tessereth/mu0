; Echo all input back to the screen
; until a q is entered
:main
LDA 0xfff
STO 0xfff
SUB :quit_c
JNE :main
STP

:quit_c
$q