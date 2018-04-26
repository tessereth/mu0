mu0
===

An assembler and simulator for the mu0 architecture.

Usage
-----

```
$ make
$ ./mu0 assemble echo.s echo.mu0
$ ./mu0 emulate echo.mu0
$ ./mu0

Usage:

1. mu0 assemble <assembly file> <machine code file> [-v]
2. mu0 emulate <machine code file> [-v] [-l n]

    -v  : verbose
    -l n: limit on the number of clock cycles to emulate

The assembler chooses what to do with each line based on the first character(s)
of the line. If the first character is:
    ';' or whitespace the line is ignored.
    ':' the next word is assumed to be a label.
    '#' the next number is stored at the next memory location.
    '$' the next character is stored as its ASCII representation.
If the line starts with one of the three letter commands
    LDA, STO, ADD, SUB, JMP, JGE, JNE
The opcode is stored and the next token is assumed to be the memory address.
If the memory address starts with a ':' it is assumed to be a label.
If the line starts with STP, 0 is stored at the next memory location.

The emulator expects a sequence of 4 digit hex numbers, one per line.
Memory location 0xfff is interpreted as memory-mapped IO. A LDA from 0xfff
reads from stdin and a STO to 0xfff prints to stdout.

Warnings: Lines must not exceed 90 characters
    The code is not very robust. If the files don't match the requirements,
    behaviour is undefined.
```
