Motorola 68000 CPU emulator host tests.

Run on Linux:

    make test

The test covers reset vectors, MOVEQ flags, BSR/RTS, an illegal-instruction
exception, STOP/interrupt/RTE including USP/SSP exchange, and odd-address
detection. Level 7 is also checked as a non-maskable interrupt. MOVE/MOVEA,
addressing modes, immediate operations, MOVEM, DBRA, ADD/SUB/CMP/AND/OR/EOR,
ADDQ/SUBQ and control effective addresses have focused tests.
