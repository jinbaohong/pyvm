#include "pyvm.h"
/* OP CODE */
#include "opcode.h"
/* RUN:  execute bytecodes
 * BSTR: generate literal bytecodes
 */
#define RUN
// #define BSTR

static char *orig_ptr; // orig_ptr is an anchor of codeObject starting point
static char *_string_table[ARRSIZ];
static int _string_table_top = 0;


/* functionObject's API */

struct functionObject *func_init_by_codeObj(struct codeObject *cobj)
{
	struct functionObject *funcObj;

	funcObj = malloc(sizeof(struct functionObject));
	funcObj->_func_code = cobj;
	funcObj->_func_name = cobj->co_name;
	funcObj->_flags = cobj->flag;

	return funcObj;
}

/* frameObject's API */
struct frameObject {
	struct Map *_locals;
	struct Map *_globals;
	struct stack *_stack;
	struct stack *_loop_stack;
	struct const_ent **_consts;
	struct const_ent **_names;
	struct const_ent **_fast_locals; // Store args passed by caller.
	struct codeObject *_cobj;
	char *_pc;

	struct frameObject *_last;
};

struct frameObject *frame_init_by_codeObj(struct codeObject *cobj)
/* Init by codeObj is only for the first frame creation.
 * In other word, this appears only in interpret().
 */
{
	struct frameObject *fobj;

	fobj = malloc(sizeof(struct frameObject));
	fobj->_locals = map_init(MAP_SIZ);
	fobj->_globals = fobj->_locals; // In first frame, global doesn't make difference with local.
	fobj->_stack = stack_init(STACK_SIZ);
	fobj->_loop_stack = stack_init(STACK_SIZ);
	fobj->_consts = cobj->consts;
	fobj->_names = cobj->names;
	fobj->_cobj = cobj;
	fobj->_pc = cobj->bytecodes;

	return fobj;
}

struct frameObject *frame_init_by_funcObj(struct functionObject *funobj, struct const_ent **args)
/* Init by funcObj is for CALL_FUNCTION. */
{
	struct frameObject *frmobj;

	frmobj = malloc(sizeof(struct frameObject));
	frmobj->_locals = map_init(MAP_SIZ);
	frmobj->_globals = funobj->_globals;
	frmobj->_stack = stack_init(STACK_SIZ);
	frmobj->_loop_stack = stack_init(STACK_SIZ);
	frmobj->_consts = funobj->_func_code->consts;
	frmobj->_names = funobj->_func_code->names;
	frmobj->_cobj = funobj->_func_code;
	frmobj->_pc = funobj->_func_code->bytecodes;

	frmobj->_fast_locals = args;

	return frmobj;
}

unsigned char frame_get_opcode(struct frameObject *frObj)
/* Side effect: This will change actual _pc in frObj */
{
	unsigned char opcode;

	opcode = *(frObj->_pc)++;
	return opcode;
}

int frame_has_more_code(struct frameObject *frObj)
{
	return frObj->_pc < (frObj->_cobj->bytecodes + frObj->_cobj->byte_code_size) ? 1 : 0;
}

int frame_get_op_arg(struct frameObject *frObj)
/* Side effect: This will change actual _pc in frObj */
{
	int b1, b2;

	b1 = *(frObj->_pc)++ & 0xFF;
	b2 = (*(frObj->_pc)++ & 0xFF) << 8;

	return b1 | b2;
}

struct Map *builtin_init()
{
	struct Map *_builtins;
	struct const_ent *key, *val;

	_builtins = map_init(MAP_SIZ);
	map_put(new_String("True"), new_Int(1), _builtins);
	map_put(new_String("False"), new_Int(0), _builtins);
	map_put(new_String("None"), new_Int(0), _builtins);

	return _builtins;
}

struct const_ent *new_Int(int num)
{
	struct const_ent *res;

	res = malloc(sizeof(struct const_ent));
	res->_type = 'i';
	res->_ptr = malloc(sizeof(int));
	*(int*)res->_ptr = num;

	return res;
}

struct const_ent *new_String(char *string)
{
	struct const_ent *res;

	res = malloc(sizeof(struct const_ent));
	res->_type = 's';
	res->_ptr = (void*)string; // Doesn't copy string.

	return res;
}


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
	struct frameObject *frame_now, *frame_tmp;
	struct Block *loop_block;
	struct const_ent *opd_1, *opd_2, *opd_res, **args;
	struct Map *_builtins;
	unsigned char opcode;
	int arg;

	_builtins = builtin_init();

	frame_now = frame_init_by_codeObj(cobj);
	frame_now->_last = NULL;

	while ( frame_has_more_code(frame_now) ) {
		opcode = frame_get_opcode(frame_now);

		if ( opcode >= HAVE_ARGUMENT )
			arg = frame_get_op_arg(frame_now);

		switch (opcode) {
			case POP_TOP:
				#if defined BSTR
				printf("%3ld POP_TOP\n", frame_now->_pc - 1 - frame_now->_cobj->bytecodes);
				#endif
				#if defined RUN
				stack_pop(frame_now->_stack);
				#endif
				break;
			case BINARY_MULTIPLY:
				#if defined BSTR
				printf("%3ld BINARY_MULTIPLY\n", frame_now->_pc - 1 - frame_now->_cobj->bytecodes);
				#endif
				#if defined RUN
				opd_1 = stack_pop(frame_now->_stack);
				opd_2 = stack_pop(frame_now->_stack);
				opd_res = malloc(sizeof(struct const_ent));
				opd_res->_type = 'i';
				opd_res->_ptr = malloc(sizeof(int));
				*((int*)(opd_res->_ptr)) = *((int*)(opd_1->_ptr)) * *((int*)(opd_2->_ptr));
				stack_push(opd_res, frame_now->_stack);
				#endif
				break;
			case BINARY_ADD:
				#if defined BSTR
				printf("%3ld BINARY_ADD\n", frame_now->_pc - 1 - frame_now->_cobj->bytecodes);
				#endif
				#if defined RUN
				opd_1 = stack_pop(frame_now->_stack);
				opd_2 = stack_pop(frame_now->_stack);
				opd_res = malloc(sizeof(struct const_ent));
				opd_res->_type = 'i';
				opd_res->_ptr = malloc(sizeof(int));
				*((int*)(opd_res->_ptr)) = *((int*)(opd_1->_ptr)) + *((int*)(opd_2->_ptr));
				stack_push(opd_res, frame_now->_stack);
				#endif
				break;
			case PRINT_ITEM:
				#if defined BSTR
				printf("%3ld PRINT_ITEM\n", frame_now->_pc - 1 - frame_now->_cobj->bytecodes);
				#endif
				#if defined RUN
				opd_1 = stack_pop(frame_now->_stack);
				// printf("%c\n", opd_1->_type);
				switch (opd_1->_type) {
					case 'i':
						printf("%d", *(int*)(opd_1->_ptr));
						break;
					case 't':
						printf("%s", (char*)opd_1->_ptr);\
						break;
					case 's':
						printf("%s", (char*)opd_1->_ptr);
				}
				#endif
				break;
			case PRINT_NEWLINE:
				#if defined BSTR
				printf("%3ld PRINT_NEWLINE\n\n", frame_now->_pc - 1 - frame_now->_cobj->bytecodes);
				#endif
				#if defined RUN
				printf("\n");
				#endif
				break;
			case BREAK_LOOP:
				#if defined BSTR
				printf("%3ld PRINT_NEWLINE\n", frame_now->_pc - 1 - frame_now->_cobj->bytecodes);
				#endif
				#if defined RUN
				loop_block = stack_pop(frame_now->_loop_stack)->_ptr;
				while (stack_size(frame_now->_stack) > loop_block->_level)
					stack_pop(frame_now->_stack);
				frame_now->_pc = loop_block->_target;
				#endif
				break;
			case RETURN_VALUE:
				#if defined BSTR
				printf("%3ld RETURN_VALUE\n", frame_now->_pc - 1 - frame_now->_cobj->bytecodes);
				#endif
				#if defined RUN
				opd_res = stack_pop(frame_now->_stack);
				if (frame_now->_last == NULL)
					return;
				frame_tmp = frame_now;

				/* Please free() frame_tmp here */

				frame_now = frame_now->_last;
				stack_push(opd_res, frame_now->_stack);
				#endif
				break;
			case POP_BLOCK:
				#if defined BSTR
				printf("%3ld POP_BLOCK\n", frame_now->_pc - 1 - frame_now->_cobj->bytecodes);
				#endif
				#if defined RUN
				loop_block = stack_pop(frame_now->_loop_stack)->_ptr;
				while (stack_size(frame_now->_stack) > loop_block->_level)
					stack_pop(frame_now->_stack);
				#endif
				break;
			case STORE_NAME:
				#if defined BSTR
				printf("%3ld %-25s %d (%s)\n\n", frame_now->_pc - 3 - frame_now->_cobj->bytecodes, "STORE_NAME", arg, (char*)frame_now->_names[arg]->_ptr);
				#endif
				#if defined RUN
				opd_res = malloc(sizeof(struct const_ent));
				opd_res->_type = 's';
				opd_res->_ptr = calloc(strlen((char*)frame_now->_names[arg]->_ptr)+1, 1);
				strcpy(opd_res->_ptr, frame_now->_names[arg]->_ptr);
				map_put(opd_res, stack_pop(frame_now->_stack), frame_now->_locals);
				#endif
				break;
			case LOAD_CONST:
				#if defined BSTR
				switch (frame_now->_consts[arg]->_type) {
					case 'i':
						printf("%3ld %-25s %d (%d)\n", frame_now->_pc - 3 - frame_now->_cobj->bytecodes, "LOAD_CONST", arg, *(int*)frame_now->_consts[arg]->_ptr);
						break;
					case 's':
					case 't':
						printf("%3ld %-25s %d (%s)\n", frame_now->_pc - 3 - frame_now->_cobj->bytecodes, "LOAD_CONST", arg, (char*)frame_now->_consts[arg]->_ptr);
						break;
					case 'N':
						printf("%3ld %-25s %d (None)\n", frame_now->_pc - 3 - frame_now->_cobj->bytecodes, "LOAD_CONST", arg);
						break;
					case 'c':
						printf("%3ld %-25s %d (<code object %s, line %d>)\n", frame_now->_pc - 3 - frame_now->_cobj->bytecodes, "LOAD_CONST", arg, ((struct codeObject*)(frame_now->_consts[arg]->_ptr))->co_name, ((struct codeObject*)(frame_now->_consts[arg]->_ptr))->lineno);
						break;
					default:
						printf("Error: doesn't recognize consts type: %c\n", frame_now->_consts[arg]->_type);
						exit(2);
						break;
				}
				#endif
				#if defined RUN
				stack_push(frame_now->_consts[arg], frame_now->_stack);
				#endif
				break;
			case LOAD_NAME:
				#if defined BSTR
				printf("%3ld %-25s %d (%s)\n", frame_now->_pc - 3 - frame_now->_cobj->bytecodes, "LOAD_NAME", arg, (char*)frame_now->_names[arg]->_ptr);
				#endif
				#if defined RUN
				if (map_exist(frame_now->_names[arg], frame_now->_locals) != -1) {
					stack_push(map_get(frame_now->_names[arg], frame_now->_locals), frame_now->_stack);
					break;
				} else if (map_exist(frame_now->_names[arg], frame_now->_globals) != -1) {
					stack_push(map_get(frame_now->_names[arg], frame_now->_globals), frame_now->_stack);
					break;
				} else if (map_exist(frame_now->_names[arg], _builtins) != -1) {
					stack_push(map_get(frame_now->_names[arg], _builtins), frame_now->_stack);
					break;
				}
				printf("Error: variable %s doesn't exist.\n", (char*)frame_now->_names[arg]->_ptr);
				#endif
				break;
			case COMPARE_OP:
				#if defined BSTR
				printf("%3ld %-25s %d (.)\n", frame_now->_pc - 3 - frame_now->_cobj->bytecodes, "COMPARE_OP", arg);
				#endif
				#if defined RUN
				opd_1 = stack_pop(frame_now->_stack);
				opd_2 = stack_pop(frame_now->_stack);

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
				stack_push(opd_res, frame_now->_stack);
				#endif
				break;
			case JUMP_FORWARD:
				#if defined BSTR
				printf("%3ld %-25s %d (%ld)\n", frame_now->_pc - 3 - frame_now->_cobj->bytecodes, "JUMP_FORWARD", arg, frame_now->_pc - frame_now->_cobj->bytecodes + arg);
				#endif
				#if defined RUN
				frame_now->_pc += arg;
				#endif
				break;
			case JUMP_ABSOLUTE:
				#if defined BSTR
				printf("%3ld %-25s %d\n", frame_now->_pc - 3 - frame_now->_cobj->bytecodes, "JUMP_FORWARD", arg);
				#endif
				#if defined RUN
				frame_now->_pc = frame_now->_cobj->bytecodes + arg;
				#endif
				break;
			case POP_JUMP_IF_FALSE:
				#if defined BSTR
				printf("%3ld %-25s %d\n\n", frame_now->_pc - 3 - frame_now->_cobj->bytecodes, "POP_JUMP_IF_FALSE", arg);
				#endif
				#if defined RUN
				opd_1 = stack_pop(frame_now->_stack);
				if (*(int*)(opd_1->_ptr) == 0)
					frame_now->_pc = frame_now->_cobj->bytecodes + arg;
				#endif
				break;
			case LOAD_GLOBAL:
				#if defined BSTR
				printf("%3ld %-25s %d (%s)\n", frame_now->_pc - 3 - frame_now->_cobj->bytecodes, "LOAD_GLOBAL", arg, (char*)frame_now->_names[arg]->_ptr);
				#endif
				#if defined RUN
				if (map_exist(frame_now->_names[arg], frame_now->_globals) != -1) {
					stack_push(map_get(frame_now->_names[arg], frame_now->_globals), frame_now->_stack);
					break;
				}
				printf("Error: variable %s doesn't exist.\n", (char*)frame_now->_names[arg]->_ptr);
				#endif
				break;
			case SETUP_LOOP:
				#if defined BSTR
				printf("%3ld %-25s %d (to %ld)\n", frame_now->_pc - 3 - frame_now->_cobj->bytecodes, "SETUP_LOOP", arg, frame_now->_pc - frame_now->_cobj->bytecodes + arg);
				#endif
				#if defined RUN
				// _target is the address which BREAK_LOOP can use without dereference.
				loop_block = malloc(sizeof(struct Block));
				loop_block->_type = opcode;
				loop_block->_target = frame_now->_pc + arg;
				loop_block->_level = stack_size(frame_now->_stack);
				opd_res = malloc(sizeof(struct const_ent));
				opd_res->_type = 'B';
				opd_res->_ptr = loop_block;
				stack_push(opd_res, frame_now->_loop_stack);
				#endif
				break;
			case LOAD_FAST:
				#if defined BSTR
				printf("%3ld %-25s %d\n", frame_now->_pc - 3 - frame_now->_cobj->bytecodes, "LOAD_FAST", arg);
				#endif
				#if defined RUN
				stack_push(frame_now->_fast_locals[arg], frame_now->_stack);
				#endif
				break;
			case CALL_FUNCTION:
				#if defined BSTR
				printf("%3ld %-25s %d\n", frame_now->_pc - 3 - frame_now->_cobj->bytecodes, "CALL_FUNCTION", arg);
				#endif
				#if defined RUN
				// args is the pointer of arguments
				args = malloc(sizeof(struct const_ent*) * arg);
				while (arg-- > 0)
					args[arg] = stack_pop(frame_now->_stack);
				frame_tmp = frame_init_by_funcObj(stack_pop(frame_now->_stack)->_ptr, args);
				frame_tmp->_last = frame_now;
				frame_now = frame_tmp;
				#endif
				break;
			case MAKE_FUNCTION:
				#if defined BSTR
				printf("%3ld %-25s %d\n", frame_now->_pc - 3 - frame_now->_cobj->bytecodes, "MAKE_FUNCTION", arg);
				#endif
				#if defined RUN
				opd_res = malloc(sizeof(struct const_ent));
				opd_res->_type = 'f'; // functionObject
				opd_res->_ptr = (void*)func_init_by_codeObj(stack_pop(frame_now->_stack)->_ptr);
				((struct functionObject*)(opd_res->_ptr))->_globals = frame_now->_globals; // Store globals into funcObj
				stack_push(opd_res, frame_now->_stack);
				#endif
				break;
			default:
				printf("Error: doesn't know opcode [%d]\n", opcode);
				exit(2);
		}
	}
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
			case 's':
				#if defined BSTR
				printf("got s\n");
				#endif
				name_len = getInt(memptr);
				const_ent_ptr = malloc(sizeof(struct const_ent));
				const_ent_ptr->_type = 's';
				const_ent_ptr->_ptr = calloc(name_len+1, 1);
				memcpy(const_ent_ptr->_ptr, memptr+4, name_len);
				cobj->consts[i] = const_ent_ptr;
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