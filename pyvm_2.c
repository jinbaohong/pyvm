#include <sys/mman.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#define ARRSIZ 256
#define STACK_SIZ 256
#define MAP_SIZ 256

/* RUN:  execute bytecodes
 * BSTR: generate literal bytecodes
 */
#define RUN
// #define BSTR

/* OP CODE */
#include "opcode.h"

/* COMPARE_OP */
#define LESS 0
#define LESS_EQUAL 1
#define EQUAL 2
#define NOT_EQUAL 3
#define GREATER 4
#define GREATER_EQUAL 5


struct map_ent {
	struct const_ent *key;
	struct const_ent *val;
};

struct Map {
	int size;
	struct map_ent **array;
};


struct stack {
	int top;
	struct const_ent **array;
};

/* loop stack entry
 * POP_BLOCK will remove an entry from loop_stack
 */
struct Block {
	unsigned char _type;
	char *_target;
	int _level;
};

struct name_ent {
	char *key;
	struct const_ent *val;
};

struct varname_ent {
	char *key;
	int val;
};

struct const_ent {
	/* type:
	 * i = int
	 * t = string
	 * c = codeObject
	 */
	char _type;
	void *_ptr;
};

struct codeObject {
	int argcnt;
	int nlocals;
	int stacksize;
	int flag;
	int byte_code_size;
	char *bytecodes;
	int consts_sz;
	/* constant is not always integer(i).
	 * It may be string(t), codeObject(c)
	 * or something else type. So I use
	 * struct const_ent pointer as the
	 * entry of consts.
	 */
	struct const_ent *consts[ARRSIZ];
	int names_sz;
	struct const_ent *names[ARRSIZ];
	int varnames_sz;
	struct varname_ent varnames[ARRSIZ];
	char *freevars[ARRSIZ];
	char *cellvars[ARRSIZ];
	char *file_name;
	char *co_name;
	int lineno;
	char *notable;
};

static char *orig_ptr; // orig_ptr is an anchor of codeObject starting point
static char *_string_table[ARRSIZ];
static int _string_table_top = 0;

int getInt(char *src);
char *get_code_object(struct codeObject *cobj, char *memptr);
void interpret(struct codeObject *cobj);
int getArg(char *ptr);
struct stack *stack_init(int stksz);
void stack_push(struct const_ent *obg, struct stack *stk);
struct const_ent *stack_pop(struct stack *stk);
int stack_size(struct stack *stk);
struct Map *map_init(int mapsz);
int map_exist(struct const_ent *key, struct Map *map);
int map_put(struct const_ent *key, struct const_ent *val, struct Map *map);
struct const_ent *map_get(struct const_ent *key, struct Map *map);


int main(int ac, char const *av[])
{
	int fd, size;
	struct stat st;
	struct codeObject cobj;
	char *pyc_addr;

	/* Usage */
	if (ac < 2){
		printf("[usage]: %s file\n", av[0]);
		return 1;
	}

	/* map pyc to memory */
	if ((fd = open(av[1], O_RDONLY)) == -1)
		perror("fd");
	fstat(fd, &st);
	size = st.st_size;

	pyc_addr = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (pyc_addr == MAP_FAILED)
		perror("mmap");

	/* Get Code Object:
	 * Starting after 0x63('c')
	 */
	if (*(pyc_addr+8) != 'c'){
		printf("Get Code Object error: not starting with 'c'\n");
		exit(2);
	}
	orig_ptr = pyc_addr;
	get_code_object(&cobj, pyc_addr+9);
	#if defined BSTR
	printf("[magic num]:%x\n", getInt(pyc_addr));
	printf("[time     ]:%s", ctime((time_t*)(pyc_addr+4)));
	printf("[cobj->argcnt] %x\n", cobj.argcnt);
	printf("[cobj->nlocals] %x\n", cobj.nlocals);
	printf("[cobj->stacksize] %x\n", cobj.stacksize);
	printf("[cobj->flag] %x\n", cobj.flag);
	printf("[cobj->byte_code_size] %d\n", cobj.byte_code_size);
	printf("[cobj->bytecodes]\n");
	for (int i = 0; i < cobj.byte_code_size; i++)
		printf("%02x", *(cobj.bytecodes+i));
	printf("\n");
	printf("[_string_table]\n");
	for (int i = 0; i < _string_table_top; ++i)
		printf("    _string_table[%d]: %s\n", i, _string_table[i]);
	printf("[cobj->consts]\n");
	for (int i = 0; i < cobj.consts_sz; ++i){
		switch (cobj.consts[i]->_type){
			case 'i':
				printf("   cobj->consts[%d] %d\n", i, *((int *)(cobj.consts[i]->_ptr)));
				break;
			case 'N':
				printf("   cobj->consts[%d] None\n", i);
				break;
			case 'c':
				printf("   cobj->consts[%d] A code object\n", i);
				break;
			case 't':
				printf("   cobj->consts[%d] %s\n", i, (char *)(cobj.consts[i]->_ptr));
				break;
			default:
				break;
		}
	}
	printf("[cobj->names]\n");
	for (int i = 0; i < cobj.names_sz; ++i)
		printf("  cobj->names[%d] %s\n", i, (char*)cobj.names[i]->_ptr);
	printf("[cobj->filename] %s\n", cobj.file_name);
	printf("[cobj->co_name] %s\n", cobj.co_name);
	#endif
	/* Interpret byte codes */
	printf("[Byte Codes Intepreting:]\n");
	interpret(&cobj);
	return 0;
}

void interpret(struct codeObject *cobj)
{
	int arg;
	char *pc;
	unsigned char opcode;
	struct stack *_stack, *_loop_stack;
	struct Block *loop_block;
	struct const_ent **_consts, **_names, *opd_1, *opd_2, *opd_res;
	struct Map *_globals;

	_globals = map_init(MAP_SIZ);
	_stack = stack_init(STACK_SIZ);
	_loop_stack = stack_init(STACK_SIZ);
	_consts = cobj->consts;
	_names = cobj->names;
	pc = cobj->bytecodes;

	while ( pc < (cobj->bytecodes + cobj->byte_code_size) ) {
		opcode = *pc++;

		if ( opcode >= HAVE_ARGUMENT ) {
			arg = getArg(pc);
			pc += 2;
		}

		switch (opcode) {
			case POP_TOP:
				#if defined BSTR
				printf("%3ld POP_TOP\n", pc-1-cobj->bytecodes);
				#endif
				#if defined RUN
				stack_pop(_stack);
				#endif
				break;
			case BINARY_MULTIPLY:
				#if defined BSTR
				printf("%3ld BINARY_MULTIPLY\n", pc-1-cobj->bytecodes);
				#endif
				#if defined RUN
				opd_1 = stack_pop(_stack);
				opd_2 = stack_pop(_stack);
				opd_res = malloc(sizeof(struct const_ent));
				opd_res->_type = 'i';
				opd_res->_ptr = malloc(sizeof(int));
				*((int*)(opd_res->_ptr)) = *((int*)(opd_1->_ptr)) * *((int*)(opd_2->_ptr));
				stack_push(opd_res, _stack);
				#endif
				break;
			case BINARY_ADD:
				#if defined BSTR
				printf("%3ld BINARY_ADD\n", pc-1-cobj->bytecodes);
				#endif
				#if defined RUN
				opd_1 = stack_pop(_stack);
				opd_2 = stack_pop(_stack);
				opd_res = malloc(sizeof(struct const_ent));
				opd_res->_type = 'i';
				opd_res->_ptr = malloc(sizeof(int));
				*((int*)(opd_res->_ptr)) = *((int*)(opd_1->_ptr)) + *((int*)(opd_2->_ptr));
				stack_push(opd_res, _stack);
				#endif
				break;
			case PRINT_ITEM:
				#if defined BSTR
				printf("%3ld PRINT_ITEM\n", pc-1-cobj->bytecodes);
				#endif
				#if defined RUN
				opd_1 = stack_pop(_stack);
				printf("%d", *(int*)(opd_1->_ptr));
				#endif
				break;
			case PRINT_NEWLINE:
				#if defined BSTR
				printf("%3ld PRINT_NEWLINE\n\n", pc-1-cobj->bytecodes);
				#endif
				#if defined RUN
				printf("\n");
				#endif
				break;
			case BREAK_LOOP:
				#if defined BSTR
				printf("%3ld PRINT_NEWLINE\n", pc-1-cobj->bytecodes);
				#endif
				#if defined RUN
				loop_block = stack_pop(_loop_stack)->_ptr;
				while (stack_size(_stack) > loop_block->_level)
					stack_pop(_stack);
				pc = loop_block->_target;
				#endif
				break;
			case RETURN_VALUE:
				#if defined BSTR
				printf("%3ld RETURN_VALUE\n", pc-1-cobj->bytecodes);
				#endif
				#if defined RUN
				stack_pop(_stack);
				#endif
				break;
			case POP_BLOCK:
				#if defined BSTR
				printf("%3ld POP_BLOCK\n", pc-1-cobj->bytecodes);
				#endif
				#if defined RUN
				loop_block = stack_pop(_loop_stack)->_ptr;
				while (stack_size(_stack) > loop_block->_level)
					stack_pop(_stack);
				#endif
				break;
			case STORE_NAME:
				#if defined BSTR
				printf("%3ld %-25s %d (%s)\n\n", pc-3-cobj->bytecodes, "STORE_NAME", arg, (char*)_names[arg]->_ptr);
				#endif
				#if defined RUN
				opd_res = malloc(sizeof(struct const_ent));
				opd_res->_type = 's';
				opd_res->_ptr = calloc(strlen((char*)_names[arg]->_ptr)+1, 1);
				strcpy(opd_res->_ptr, _names[arg]->_ptr);
				map_put(opd_res, stack_pop(_stack), _globals);
				#endif
				break;
			case LOAD_CONST:
				#if defined BSTR
				switch (_consts[arg]->_type) {
					case 'i':
						printf("%3ld %-25s %d (%d)\n", pc-3-cobj->bytecodes, "LOAD_CONST", arg, *((int*)(_consts[arg]->_ptr)));
						break;
					case 't':
						printf("%3ld %-25s %d (%s)\n", pc-3-cobj->bytecodes, "LOAD_CONST", arg, (char*)(_consts[arg]->_ptr));
						break;
					case 'N':
						printf("%3ld %-25s %d (None)\n", pc-3-cobj->bytecodes, "LOAD_CONST", arg);
						break;
					case 'c':
						printf("%3ld %-25s %d (<code object %s, line %d>)\n", pc-3-cobj->bytecodes, "LOAD_CONST", arg, ((struct codeObject*)(_consts[arg]->_ptr))->co_name, ((struct codeObject*)(_consts[arg]->_ptr))->lineno);
						break;
					default:
						printf("Error: doesn't recognize consts type: %c\n", _consts[arg]->_type);
						exit(2);
						break;
				}
				#endif
				#if defined RUN
				stack_push(_consts[arg], _stack);
				#endif
				break;
			case LOAD_NAME:
				#if defined BSTR
				printf("%3ld %-25s %d (%s)\n", pc-3-cobj->bytecodes, "LOAD_NAME", arg, (char*)_names[arg]->_ptr);
				#endif
				#if defined RUN
				stack_push(map_get(_names[arg], _globals), _stack);
				#endif
				break;
			case COMPARE_OP:
				#if defined BSTR
				printf("%3ld %-25s %d (.)\n", pc-3-cobj->bytecodes, "COMPARE_OP", arg);
				#endif
				#if defined RUN
				opd_1 = stack_pop(_stack);
				opd_2 = stack_pop(_stack);

				opd_res = malloc(sizeof(struct const_ent));
				opd_res->_type = 'i';
				opd_res->_ptr = (void*)malloc(sizeof(int));
				switch (arg) {
					case LESS:
						*((int*)(opd_res->_ptr)) = *(int*)(opd_2->_ptr) < *(int*)(opd_1->_ptr) ? 1 : 0;
						break;
					case LESS_EQUAL:
						*((int*)(opd_res->_ptr)) = *(int*)(opd_2->_ptr) <= *(int*)(opd_1->_ptr) ? 1 : 0;
						break;
					case EQUAL:
						*((int*)(opd_res->_ptr)) = *(int*)(opd_2->_ptr) == *(int*)(opd_1->_ptr) ? 1 : 0;
						break;
					case NOT_EQUAL:
						*((int*)(opd_res->_ptr)) = *(int*)(opd_2->_ptr) != *(int*)(opd_1->_ptr) ? 1 : 0;
						break;
					case GREATER:
						*((int*)(opd_res->_ptr)) = *(int*)(opd_2->_ptr) > *(int*)(opd_1->_ptr) ? 1 : 0;
						break;
					case GREATER_EQUAL:
						*((int*)(opd_res->_ptr)) = *(int*)(opd_2->_ptr) >= *(int*)(opd_1->_ptr) ? 1 : 0;
						break;
				}
				stack_push(opd_res, _stack);
				#endif
				break;
			case JUMP_FORWARD:
				#if defined BSTR
				printf("%3ld %-25s %d (%ld)\n", pc-3-cobj->bytecodes, "JUMP_FORWARD", arg, pc-cobj->bytecodes+arg);
				#endif
				#if defined RUN
				pc += arg;
				#endif
				break;
			case JUMP_ABSOLUTE:
				#if defined BSTR
				printf("%3ld %-25s %d\n", pc-3-cobj->bytecodes, "JUMP_FORWARD", arg);
				#endif
				#if defined RUN
				pc = cobj->bytecodes + arg;
				#endif
				break;
			case POP_JUMP_IF_FALSE:
				#if defined BSTR
				printf("%3ld %-25s %d\n\n", pc-3-cobj->bytecodes, "POP_JUMP_IF_FALSE", arg);
				#endif
				#if defined RUN
				opd_1 = stack_pop(_stack);
				if (*(int*)(opd_1->_ptr) == 0)
					pc = cobj->bytecodes + arg;
				#endif
				break;
			case SETUP_LOOP:
				#if defined BSTR
				printf("%3ld %-25s %d (to %ld)\n", pc-3-cobj->bytecodes, "SETUP_LOOP", arg, pc-cobj->bytecodes+arg);
				#endif
				#if defined RUN
				// _target is the address which BREAK_LOOP can use without dereference.
				loop_block = malloc(sizeof(struct Block));
				loop_block->_type = opcode;
				loop_block->_target = pc + arg;
				loop_block->_level = stack_size(_stack);
				opd_res = malloc(sizeof(struct const_ent));
				opd_res->_type = 'B';
				opd_res->_ptr = loop_block;
				stack_push(opd_res, _loop_stack);
				#endif
				break;
			case CALL_FUNCTION:
				#if defined BSTR
				printf("%3ld %-25s %d\n", pc-3-cobj->bytecodes, "CALL_FUNCTION", arg);
				#endif
				#if defined RUN
				#endif
				break;
			case MAKE_FUNCTION:
				#if defined BSTR
				printf("%3ld %-25s %d\n", pc-3-cobj->bytecodes, "MAKE_FUNCTION", arg);
				#endif
				#if defined RUN
				#endif
				break;
			default:
				printf("Error: doesn't know opcode [%d]\n", opcode);
				exit(2);
		}

	}
}

int getArg(char *ptr)
{
	int b1, b2;

	b1 = *ptr++ & 0xFF;
	b2 = (*ptr & 0xFF) << 8;

	return b1 | b2;
}

/* Generic stack */

struct stack *stack_init(int stksz)
{
	struct stack *stk = malloc(sizeof(struct stack));
	stk->top = -1;
	stk->array = malloc(sizeof(struct const_ent*) * stksz);
	return stk;
}

void stack_push( struct const_ent *obg, struct stack *stk)
{
	if (stk->top == STACK_SIZ-1){
		printf("Error: Stack is full\n");
		exit(2);
	}
	stk->array[++(stk->top)] = obg;
}

struct const_ent *stack_pop(struct stack *stk)
{
	if (stk->top == -1){
		printf("Error: Stack is empty\n");
		exit(2);
	}
	return stk->array[(stk->top)--];
}

int stack_size(struct stack *stk)
{
	return stk->top + 1;
}

/* Generic Map */
struct Map *map_init(int mapsz)
{
	struct Map *map = malloc(sizeof(struct Map));
	map->size = 0;
	map->array = malloc(sizeof(struct map_ent*) * mapsz);
	return map;
}

int map_exist(struct const_ent *key, struct Map *map)
{
	for (int i = 0; i < map->size; i++)
		if (strcmp(map->array[i]->key->_ptr, key->_ptr) == 0)
			return i;
	/* Doesn't find key */
	return -1;
}

int map_put(struct const_ent *key, struct const_ent *val, struct Map *map)
{
	int i;

	if (map->size == MAP_SIZ){
		printf("Error: Map is full\n");
		exit(2);
	}
	if ((i = map_exist(key, map)) == -1) {
		struct map_ent *map_ent_tmp = malloc(sizeof(struct map_ent));
		map_ent_tmp->key = key;
		map_ent_tmp->val = val;
		map->array[(map->size)++] = map_ent_tmp;
	} else 
		map->array[i]->val = val;
	return 0;
}

struct const_ent *map_get(struct const_ent *key, struct Map *map)
{
	int i;
	if ((i = map_exist(key, map)) != -1)
		return map->array[i]->val;
	printf("Error: doesn't find key %s\n", (char*)key->_ptr);
	exit(2);
}


char *get_code_object(struct codeObject *cobj, char *memptr)
{
	int byte_code_sz, cst_table_sz,
		names_sz, varnames_sz, cellvar_sz,
		free_var_sz, file_name_sz,
		code_name_sz, name_len, varname_len,
		begin_line_no, lnotab_sz;
	char *byte_code_ptr, *file_name_ptr,
		 *code_name_ptr, *lnotab_ptr;
	struct const_ent *const_ent_ptr;



	cobj->argcnt = getInt(memptr); memptr += 4; printf("argcnt = %d\n", cobj->argcnt);
	cobj->nlocals = getInt(memptr); memptr += 4; printf("nlocals = %d\n", cobj->nlocals);
	cobj->stacksize = getInt(memptr); memptr += 4; printf("stacksize = %d\n", cobj->stacksize);
	cobj->flag = getInt(memptr); memptr += 4;

	/* Get byte code */
	if (*memptr != 's') { /* byte codes is start with 's' */
		printf("Error: byte codes error\n");
		printf("memptr is at %lx\n", memptr - orig_ptr);
		exit(2);
	}
	memptr++;
	byte_code_sz = getInt(memptr); memptr += 4;
	byte_code_ptr = calloc(byte_code_sz+1, 1);
	memcpy(byte_code_ptr, memptr, byte_code_sz);
	cobj->bytecodes = byte_code_ptr;
	memptr += byte_code_sz;
	cobj->byte_code_size = byte_code_sz;

	/* Get constants table */
	printf("Getting constants table...\n");
	if (*memptr != '(') { /* constant table is start with '(' */
		printf("Error: constants table error\n");
		exit(2);
	}
	memptr++;
	cst_table_sz = getInt(memptr); memptr += 4;
	cobj->consts_sz = cst_table_sz;
	for (int i = 0; i < cst_table_sz; i++)	{
		switch ( *memptr++ ) {
			case 'i':
				#if defined BSTR
				printf("got i\n");
				#endif
				const_ent_ptr = malloc(sizeof(struct const_ent));
				const_ent_ptr->_type = 'i';
				const_ent_ptr->_ptr = malloc(sizeof(int));
				*((int *)(const_ent_ptr->_ptr)) = getInt(memptr);
				cobj->consts[i] = const_ent_ptr;
				memptr += 4;
				break;
			case 't':
				#if defined BSTR
				printf("got t\n");
				#endif
				name_len = getInt(memptr);
				const_ent_ptr = malloc(sizeof(struct const_ent));
				const_ent_ptr->_type = 't';
				const_ent_ptr->_ptr = calloc(name_len+1, 1);
				memcpy(const_ent_ptr->_ptr, memptr+4, name_len);
				cobj->consts[i] = const_ent_ptr;
				_string_table[_string_table_top++] = (char *)(const_ent_ptr->_ptr);
				memptr += (4 + name_len);
				break;
			case 'N':
				#if defined BSTR
				printf("got N\n");
				#endif
				const_ent_ptr = malloc(sizeof(struct const_ent));
				const_ent_ptr->_type = 'N';
				const_ent_ptr->_ptr = malloc(sizeof(int));
				*((int *)(const_ent_ptr->_ptr)) = 0;
				cobj->consts[i] = const_ent_ptr;
				break;
			case 'c':
				#if defined BSTR
				printf("got c\n");
				#endif
				const_ent_ptr = malloc(sizeof(struct const_ent));
				const_ent_ptr->_type = 'c';
				const_ent_ptr->_ptr = malloc(sizeof(struct codeObject));
				cobj->consts[i] = const_ent_ptr;
				memptr = get_code_object(const_ent_ptr->_ptr, memptr);
				break;
			default:
				printf("Error: constant type error:\n");
				printf("Can't recognize char [%c]\n", *(memptr-1));
				exit(2);
		}
	}
	/* Get names table (variable table) */
	printf("Getting names table...\n");
	if (*memptr++ != '(') { /* names table is start with '(' */
		printf("Error: names table error\n");
		exit(2);
	}
	names_sz = getInt(memptr); memptr += 4;
	cobj->names_sz = names_sz;
	for (int i = 0; i < names_sz; i++) {
		switch (*memptr++) {
			case 't':
				printf("got a 't'\n");
				name_len = getInt(memptr);
				cobj->names[i] = malloc(sizeof(struct const_ent));
				cobj->names[i]->_type = 's';
				cobj->names[i]->_ptr = calloc(name_len+1, 1);
				memcpy(cobj->names[i]->_ptr, memptr+4, name_len);
				_string_table[_string_table_top++] = cobj->names[i]->_ptr;
				memptr += (4 + name_len);
				break;
			case 'R':
				printf("got a 'R', names[%d] is %s\n", i, _string_table[getInt(memptr)]);
				name_len = getInt(memptr);
				cobj->names[i] = malloc(sizeof(struct const_ent));
				cobj->names[i]->_type = 's';
				cobj->names[i]->_ptr = _string_table[getInt(memptr)];
				memptr += 4;
				break;
			default:
				printf("Error: names type error:\n");
				printf("Can't recognize char [%c]\n", *(memptr-1));
				exit(2);
		}
	}

	/* Get varnames table */
	printf("Getting varnames table...\n");
	if (*memptr++ != '(') { /* varnames table is start with '(' */
		printf("Error: varnames table error\n");
		exit(2);
	}
	varnames_sz = getInt(memptr); memptr += 4;
	cobj->varnames_sz = varnames_sz;
	for (int i = 0; i < varnames_sz; i++) {
		if (*memptr++ != 't'){
			printf("Error: varname entry doesn't start with 't'\n");
			exit(2);
		}
		varname_len = getInt(memptr);
		cobj->varnames[i].key = calloc(varname_len+1, 1);
		memcpy(cobj->varnames[i].key, memptr+4, varname_len);
		_string_table[_string_table_top++] = cobj->varnames[i].key;
		memptr += (4 + varname_len);
	}

	/* Get cellvar table */
	printf("Getting cellvar table...\n");
	if (*memptr++ != '(') { /* cellvar table is start with '(' */
		printf("Error: cellvar table error\n");
		printf("memptr is at %lx\n", memptr - orig_ptr);
		exit(2);
	}
	cellvar_sz = getInt(memptr); memptr += 4;

	/* Get free_var table */
	printf("Getting free_var table...\n");
	if (*memptr++ != '(') { /* free_var table is start with '(' */
		printf("Error: varnames table error\n");
		exit(2);
	}
	free_var_sz = getInt(memptr); memptr += 4;

	/* Get file name */
	printf("Getting file name...\n");
	if (*memptr++ != 's') { /* filename is start with 's' */
		printf("Error: filename error\n");
		exit(2);
	}
	file_name_sz = getInt(memptr); memptr += 4;
	file_name_ptr = calloc(file_name_sz+1, 1);
	memcpy(file_name_ptr, memptr, file_name_sz);
	cobj->file_name = file_name_ptr;
	memptr += file_name_sz;

	/* Get code name */
	printf("Getting code name...\n");
	if (*memptr++ != 't') { /* codename is start with 't' */
		printf("Error: codename error\n");
		exit(2);
	}
	code_name_sz = getInt(memptr); memptr += 4;
	code_name_ptr = calloc(code_name_sz+1, 1);
	memcpy(code_name_ptr, memptr, code_name_sz);
	cobj->co_name = code_name_ptr;
	_string_table[_string_table_top++] = code_name_ptr;
	memptr += code_name_sz;


	/* Get line number table */
	begin_line_no = getInt(memptr); memptr += 4;
	cobj->lineno = begin_line_no;

	printf("Get line number table...\n");
	if (*memptr++ != 's') { /* line number table is start with 's' */
		printf("Error: lineno table error\n");
		exit(2);
	}
	lnotab_sz = getInt(memptr); memptr += 4;
	lnotab_ptr = calloc(lnotab_sz+1, 1);
	memcpy(lnotab_ptr, memptr, lnotab_sz);
	cobj->notable = lnotab_ptr;
	memptr += lnotab_sz;
	
	return memptr;
}

// getString()
// {
// 	name_len = getInt(memptr);
// 	cobj->names[i].key = calloc(name_len+1, 1);
// 	memcpy(cobj->names[i].key, memptr+4, name_len);
// 	memptr += (4 + name_len);
// }

int getInt(char *src)
{
	int b1, b2, b3, b4;

	b1 = *src++ & 0xFF;
	b2 = (*src++ & 0xFF) << 8;
	b3 = (*src++ & 0xFF) << 16;
	b4 = (*src & 0xFF) << 24;

	return b1 | b2 | b3 | b4;
}




// 	printf("Getting constants table...\n");
// 	if (*memptr != '(') { /* constant table is start with '(' */
// 		printf("Error: constants table error\n");
// 		exit(2);
// 	}
// 	memptr++;
// 	cst_table_sz = getInt(memptr); memptr += 4;
// 	cobj->consts_sz = cst_table_sz;
// 	for (int i = 0; i < cst_table_sz; i++) {
// 		getTuple( *memptr++ , );
// 		cobj->consts[i] = obg_ent_ptr;	
// 	}

// getTuple(char tuple_type, ) {
// 	/* Get constants table */
// 	struct obg_ent *obg_ent_ptr = malloc(sizeof(struct obg_ent));
// 	switch ( tuple_type ) {
// 		case 'i':
// 			#if defined BSTR
// 			printf("got i\n");
// 			#endif
// 			obg_ent_ptr->_type = 'i';
// 			obg_ent_ptr->_ptr = malloc(sizeof(int));
// 			*((int *)(obg_ent_ptr->_ptr)) = getInt(memptr);
// 			memptr += 4;
// 			break;
// 		case 't':
// 			#if defined BSTR
// 			printf("got t\n");
// 			#endif
// 			name_len = getInt(memptr);
// 			obg_ent_ptr->_type = 't';
// 			obg_ent_ptr->_ptr = calloc(name_len+1, 1);
// 			memcpy(obg_ent_ptr->_ptr, memptr+4, name_len);
// 			_string_table[_string_table_top++] = (char *)(obg_ent_ptr->_ptr);
// 			memptr += (4 + name_len);
// 			break;
// 		case 'N':
// 			#if defined BSTR
// 			printf("got N\n");
// 			#endif
// 			obg_ent_ptr->_type = 'N';
// 			obg_ent_ptr->_ptr = malloc(sizeof(int));
// 			*((int *)(obg_ent_ptr->_ptr)) = 0;
// 			break;
// 		case 'c':
// 			#if defined BSTR
// 			printf("got c\n");
// 			#endif
// 			obg_ent_ptr->_type = 'c';
// 			obg_ent_ptr->_ptr = malloc(sizeof(struct codeObject));
// 			memptr = get_code_object(obg_ent_ptr->_ptr, memptr);
// 			break;
// 		default:
// 			printf("Error: constant type error:\n");
// 			printf("Can't recognize char [%c]\n", *(memptr-1));
// 			exit(2);
// 	}
// 	return obg_ent_ptr;
// }