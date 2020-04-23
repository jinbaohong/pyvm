#include <sys/mman.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#define ARRSIZ 256
#define STACK_SIZ 256

/* RUN:  execute bytecodes
 * BSTR: generate literal bytecodes
 */
#define RUN

/* OP CODE */
#include "opcode.h"

/* COMPARE_OP */
#define LESS 0
#define LESS_EQUAL 1
#define EQUAL 2
#define NOT_EQUAL 3
#define GREATER 4
#define GREATER_EQUAL 5

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
	int val;
};

struct codeObject {
	int argcnt;
	int nlocals;
	int stacksize;
	int flag;
	int byte_code_size;
	char *bytecodes;
	int consts_sz;
	int consts[ARRSIZ];
	int names_sz;
	struct name_ent names[ARRSIZ];
	char *varnames[ARRSIZ];
	char *freevars[ARRSIZ];
	char *cellvars[ARRSIZ];
	char *file_name;
	char *co_name;
	int lineno;
	char *notable;
};

static int top = -1;
static int loop_stack_top = -1;

int getInt(char *src);
void get_code_object(struct codeObject *cobj, char *memptr);
void interpret(struct codeObject *cobj);
void stack_push(int val, int *stk);
int* stack_init(int stksz);
int stack_pop(int *stk);
int stack_size(int *stk);
struct Block **loop_stack_init(int stksz);
void loop_stack_push(struct Block *val, struct Block **stk);
struct Block *loop_stack_pop(struct Block **stk);
int getArg(char *ptr);



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
	printf("[cobj->consts]\n");
	for (int i = 0; i < cobj.consts_sz; ++i)
		printf("   cobj->consts[%d] %d\n", i, cobj.consts[i]);
	printf("[cobj->names]\n");
	for (int i = 0; i < cobj.names_sz; ++i)
		printf("  c obj->names[%d] %s\n", i, cobj.names[i].key);
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
	int *_stack, *_consts, arg, opd_1, opd_2;
	char *pc;
	struct name_ent *_names;
	unsigned char opcode;
	struct Block **_loop_stack, *loop_block;

	_stack = stack_init(STACK_SIZ);
	_loop_stack = loop_stack_init(STACK_SIZ);
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
			case BINARY_MULTIPLY:
				#if defined BSTR
				printf("%3ld BINARY_MULTIPLY\n", pc-1-cobj->bytecodes);
				#endif
				#if defined RUN
				opd_1 = stack_pop(_stack);
				opd_2 = stack_pop(_stack);
				stack_push(opd_1 * opd_2, _stack);
				#endif
				break;
			case BINARY_ADD:
				#if defined BSTR
				printf("%3ld BINARY_ADD\n", pc-1-cobj->bytecodes);
				#endif
				#if defined RUN
				opd_1 = stack_pop(_stack);
				opd_2 = stack_pop(_stack);
				stack_push(opd_1 + opd_2, _stack);
				#endif
				break;
			case PRINT_ITEM:
				#if defined BSTR
				printf("%3ld PRINT_ITEM\n", pc-1-cobj->bytecodes);
				#endif
				#if defined RUN
				opd_1 = stack_pop(_stack);
				printf("%d", opd_1);
				#endif
				break;
			case PRINT_NEWLINE:
				#if defined BSTR
				printf("%3ld PRINT_NEWLINE\n", pc-1-cobj->bytecodes);
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
				loop_block = loop_stack_pop(_loop_stack);
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
				loop_block = loop_stack_pop(_loop_stack);
				while (stack_size(_stack) > loop_block->_level)
					stack_pop(_stack);
				#endif
				break;
			case STORE_NAME:
				#if defined BSTR
				printf("%3ld %-25s %d(%s)\n", pc-3-cobj->bytecodes, "STORE_NAME", arg, _names[arg].key);
				#endif
				#if defined RUN
				_names[arg].val = stack_pop(_stack);
				#endif
				break;
			case LOAD_CONST:
				#if defined BSTR
				printf("%3ld %-25s %d(%d)\n", pc-3-cobj->bytecodes, "LOAD_CONST", arg, _consts[arg]);
				#endif
				#if defined RUN
				stack_push(_consts[arg], _stack);
				#endif
				break;
			case LOAD_NAME:
				#if defined BSTR
				printf("%3ld %-25s %d(%s)\n", pc-3-cobj->bytecodes, "LOAD_NAME", arg, _names[arg].key);
				#endif
				#if defined RUN
				stack_push(_names[arg].val, _stack);
				#endif
				break;
			case COMPARE_OP:
				#if defined BSTR
				printf("%3ld %-25s %d(%d)\n", pc-3-cobj->bytecodes, "COMPARE_OP", arg, arg);
				#endif
				#if defined RUN
				opd_1 = stack_pop(_stack);
				opd_2 = stack_pop(_stack);
				switch (arg) {
					case LESS:
						stack_push(opd_2 < opd_1 ? 1 : 0, _stack);
						break;
					case LESS_EQUAL:
						stack_push(opd_2 <= opd_1 ? 1 : 0, _stack);
						break;
					case EQUAL:
						stack_push(opd_2 == opd_1 ? 1 : 0, _stack);
						break;
					case NOT_EQUAL:
						stack_push(opd_2 != opd_1 ? 1 : 0, _stack);
						break;
					case GREATER:
						stack_push(opd_2 > opd_1 ? 1 : 0, _stack);
						break;
					case GREATER_EQUAL:
						stack_push(opd_2 >= opd_1 ? 1 : 0, _stack);
						break;
				}
				#endif
				break;
			case JUMP_FORWARD:
				#if defined BSTR
				printf("%3ld %-25s %d(%ld)\n", pc-3-cobj->bytecodes, "JUMP_FORWARD", arg, pc-cobj->bytecodes+arg);
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
				printf("%3ld %-25s %d\n", pc-3-cobj->bytecodes, "POP_JUMP_IF_FALSE", arg);
				#endif
				#if defined RUN
				opd_1 = stack_pop(_stack);
				if (opd_1 == 0)
					pc = cobj->bytecodes + arg;
				#endif
				break;
			case SETUP_LOOP:
				#if defined BSTR
				printf("%3ld %-25s %d(%ld)\n", pc-3-cobj->bytecodes, "SETUP_LOOP", arg, pc-cobj->bytecodes+arg);
				#endif
				#if defined RUN
				// _target is the address which BREAK_LOOP can use without dereference.
				loop_block = malloc(sizeof(struct Block));
				loop_block->_type = opcode;
				loop_block->_target = pc + arg;
				loop_block->_level = stack_size(_stack);
				loop_stack_push(loop_block, _loop_stack);
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

/* int stack operation */
int* stack_init(int stksz)
{
	int *stk = calloc(sizeof(int), stksz);
}

void stack_push(int val, int *stk)
{
	if (top == STACK_SIZ-1){
		printf("Stack is full\n");
		exit(2);
	}
	stk[++top] = val;
}

int stack_pop(int *stk)
{
	if (top == -1){
		printf("Stack is empty\n");
		exit(2);
	}
	return stk[top--];
}

int stack_size(int *stk)
{
	return top + 1;
}

/* loop_block stack operation */
struct Block **loop_stack_init(int stksz)
{
	struct Block **stk = malloc(sizeof(struct Block *) * stksz);
}

void loop_stack_push(struct Block *val, struct Block **stk)
{
	if (loop_stack_top == STACK_SIZ-1){
		printf("Loop_Stack is full\n");
		exit(2);
	}
	stk[++loop_stack_top] = val;
}

struct Block *loop_stack_pop(struct Block **stk)
{
	if (loop_stack_top == -1){
		printf("Loop_Stack is empty\n");
		exit(2);
	}
	return stk[loop_stack_top--];
}



void get_code_object(struct codeObject *cobj, char *memptr)
{
	int byte_code_sz, cst_table_sz,
		names_sz, varnames_sz, cellvar_sz,
		free_var_sz, file_name_sz,
		code_name_sz, name_len;
	char *byte_code_ptr, *file_name_ptr,
		 *code_name_ptr;

	cobj->argcnt = getInt(memptr); memptr += 4;
	cobj->nlocals = getInt(memptr); memptr += 4;
	cobj->stacksize = getInt(memptr); memptr += 4;
	cobj->flag = getInt(memptr); memptr += 4;

	/* Get byte code */
	if (*memptr != 's') { /* byte codes is start with 's' */
		printf("byte codes error\n");
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
	if (*memptr != '(') { /* constant table is start with '(' */
		printf("constants table error\n");
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
				cobj->consts[i] = getInt(memptr);
				memptr += 4;
				break;
			case 'N':
				#if defined BSTR
				printf("got N\n");
				#endif
				cobj->consts[i] = 0;
				break;
			default:
				printf("constant type error:\n");
				printf("Can't recognize char [%c]\n", *(memptr-1));
				exit(2);
		}
	}

	/* Get names table (variable table) */
	if (*memptr != '(') { /* names table is start with '(' */
		printf("names table error\n");
		exit(2);
	}
	memptr++;
	names_sz = getInt(memptr); memptr += 4;
	cobj->names_sz = names_sz;
	for (int i = 0; i < names_sz; i++) {
		if (*memptr++ != 't'){
			printf("Error: name entry doesn't start with 't'\n");
			exit(2);
		}
		name_len = getInt(memptr);
		cobj->names[i].key = calloc(name_len+1, 1);
		memcpy(cobj->names[i].key, memptr+4, name_len);
		memptr += (4 + name_len);
	}

	/* Get varnames table */
	if (*memptr != '(') { /* varnames table is start with '(' */
		printf("varnames table error\n");
		exit(2);
	}
	memptr++;
	varnames_sz = getInt(memptr); memptr += 4;

	/* Get cellvar table */
	if (*memptr != '(') { /* cellvar table is start with '(' */
		printf("cellvar table error\n");
		exit(2);
	}
	memptr++;
	cellvar_sz = getInt(memptr); memptr += 4;

	/* Get free_var table */
	if (*memptr != '(') { /* free_var table is start with '(' */
		printf("varnames table error\n");
		exit(2);
	}
	memptr++;
	free_var_sz = getInt(memptr); memptr += 4;

	/* Get file name */
	if (*memptr != 's') { /* filename is start with 's' */
		printf("filename error\n");
		exit(2);
	}
	memptr++;
	file_name_sz = getInt(memptr); memptr += 4;
	file_name_ptr = calloc(file_name_sz+1, 1);
	memcpy(file_name_ptr, memptr, file_name_sz);
	cobj->file_name = file_name_ptr;
	memptr += file_name_sz;

	/* Get code name */
	if (*memptr != 't') { /* codename is start with 't' */
		printf("codename error\n");
		exit(2);
	}
	memptr++;
	code_name_sz = getInt(memptr); memptr += 4;
	code_name_ptr = calloc(code_name_sz+1, 1);
	memcpy(code_name_ptr, memptr, code_name_sz);
	cobj->co_name = code_name_ptr;
	memptr += code_name_sz;

}

int getInt(char *src)
{
	int b1, b2, b3, b4;

	b1 = *src++ & 0xFF;
	b2 = (*src++ & 0xFF) << 8;
	b3 = (*src++ & 0xFF) << 16;
	b4 = (*src & 0xFF) << 24;

	return b1 | b2 | b3 | b4;
}