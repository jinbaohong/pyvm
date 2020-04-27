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

struct varname_ent {
	char *key;
	int val;
};

struct const_ent {
	/* type:
	 * i = int
	 * t = string
	 * c = codeObject
	 * f = functionObject
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

struct functionObject {
	struct codeObject *_func_code;
	char *_func_name;
	unsigned int _flags;
	struct Map *_globals; // Every function has its own globals, which is built when it was created.
};

int getInt(char *src);
char *get_code_object(struct codeObject *cobj, char *memptr);
void interpret(struct codeObject *cobj);

struct stack *stack_init(int stksz);
void stack_push(struct const_ent *obg, struct stack *stk);
struct const_ent *stack_pop(struct stack *stk);
int stack_size(struct stack *stk);

struct Map *map_init(int mapsz);
int map_exist(struct const_ent *key, struct Map *map);
int map_put(struct const_ent *key, struct const_ent *val, struct Map *map);
struct const_ent *map_get(struct const_ent *key, struct Map *map);

struct frameObject *frame_init_by_funcObj(struct functionObject *funobj, struct const_ent **args);
struct frameObject *frame_init_by_codeObj(struct codeObject *cobj);
unsigned char frame_get_opcode(struct frameObject *frObj);
int frame_has_more_code(struct frameObject *frObj);
int frame_get_op_arg(struct frameObject *frObj);

struct functionObject *func_init_by_codeObj(struct codeObject *cobj);
