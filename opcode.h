#define POP_TOP 1
#define ROT_TWO 2
#define ROT_THREE 3
#define DUP_TOP 4
#define UNARY_NEGATIVE 11
#define BINARY_MULTIPLY 20
#define BINARY_MODULO 22
#define BINARY_SUBSCR 25
#define BINARY_DIVIDE 21
#define BINARY_ADD 23
#define BINARY_SUBTRACT 24

#define INPLACE_ADD 55
#define STORE_MAP 54
#define INPLACE_SUBSTRACT 56
#define INPLACE_MULTIPLY 57
#define INPLACE_DIVIDE 58
#define INPLACE_MODULO 59
#define STORE_SUBSCR 60
#define DELETE_SUBSCR 61

#define GET_ITER 68

#define PRINT_ITEM 71
#define PRINT_NEWLINE 72

#define BREAK_LOOP 80
#define LOAD_LOCALS 82
#define RETURN_VALUE 83
#define YIELD_VALUE 86
#define POP_BLOCK 87
#define END_FINALLY 88
#define BUILD_CLASS 89

    // TODO: This is a separator
#define HAVE_ARGUMENT 90 /* Opcodes from here have an argument: */

#define STORE_NAME 90 /* Index in name list */
#define UNPACK_SEQUENCE 92
#define FOR_ITER 93
#define STORE_ATTR 95  /* Index in name list */
#define STORE_GLOBAL 97
#define DUP_TOPX 99   /* number of items to duplicate */
#define LOAD_CONST 100 /* Index in const list */
#define LOAD_NAME 101 /* Index in name list */
#define BUILD_TUPLE 102
#define BUILD_LIST 103
#define BUILD_MAP 105
#define LOAD_ATTR 106 /* Index in name list */
#define COMPARE_OP 107 /* Comparison operator */
#define IMPORT_NAME 108 /* Index in name list */
#define IMPORT_FROM 109 /* Index in name list */
#define JUMP_FORWARD 110 /* Number of bytes to skip */
#define JUMP_IF_FALSE_OR_POP 111 /* Target byte offset from beginning
                                    of code */

#define JUMP_ABSOLUTE 113
#define POP_JUMP_IF_FALSE 114
#define POP_JUMP_IF_TRUE 115
#define LOAD_GLOBAL 116 /* Index in name list */

#define CONTINUE_LOOP 119 /* Start of loop (absolute) */
#define SETUP_LOOP 120 /* Target address (relative) */
#define SETUP_EXCEPT 121  /* "" */
#define SETUP_FINALLY 122 /* "" */

#define LOAD_FAST  124 /* Local variable number */
#define STORE_FAST 125 /* Local variable number */

#define RAISE_VARARGS 130
#define CALL_FUNCTION 131
#define MAKE_FUNCTION 132

#define MAKE_CLOSURE 134 /* #free vars */
#define LOAD_CLOSURE 135 /* Load free variable from closure */
#define LOAD_DEREF 136 /* Load and dereference from closure cell */
#define STORE_DEREF 137 /* Store into cell */

#define CALL_FUNCTION_VAR 140