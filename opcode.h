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
 /* Opcodes from here have an argument: */
#define HAVE_ARGUMENT 90
#define STORE_NAME 90
#define UNPACK_SEQUENCE 92
#define FOR_ITER 93
#define STORE_ATTR 95
#define STORE_GLOBAL 97
#define DUP_TOPX 99
#define LOAD_CONST 100
#define LOAD_NAME 101
#define BUILD_TUPLE 102
#define BUILD_LIST 103
#define BUILD_MAP 105
#define LOAD_ATTR 106
#define COMPARE_OP 107
#define IMPORT_NAME 108
#define IMPORT_FROM 109
#define JUMP_FORWARD 110
#define JUMP_IF_FALSE_OR_POP 111
#define JUMP_ABSOLUTE 113
#define POP_JUMP_IF_FALSE 114
#define POP_JUMP_IF_TRUE 115
#define LOAD_GLOBAL 116
#define CONTINUE_LOOP 119
#define SETUP_LOOP 120
#define SETUP_EXCEPT 121
#define SETUP_FINALLY 122
#define LOAD_FAST  124
#define STORE_FAST 125
#define RAISE_VARARGS 130
#define CALL_FUNCTION 131
#define MAKE_FUNCTION 132
#define MAKE_CLOSURE 134
#define LOAD_CLOSURE 135
#define LOAD_DEREF 136
#define STORE_DEREF 137
#define CALL_FUNCTION_VAR 140