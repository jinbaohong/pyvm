#include <sys/mman.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

int main(int ac, char const *av[])
{
	int fd, size, magic;
	struct stat st;
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

	magic = *((int *)pyc_addr);
	printf("[magic num]:%x\n", magic);
	printf("[time     ]:%s", ctime((time_t*)(pyc_addr+4)));
	printf("[src size ]:%d\n", *(int*)(pyc_addr+8));
	// time_t i = 0x5e9e6d39;
	// time_t *k = &i;
	// printf("*k: %x\n", *k);
	// printf("*((int*)k): %d\n", *((int*)k));
	// printf("%s\n", ctime(k));

	// time_t *j = (time_t*)(pyc_addr+4);
	// printf("*j: %x\n", *j);
	// printf("*((int*)j): %d\n", *((int*)j));
	// printf("%s\n", ctime(j));

	// if (*(int*)k == *(int*)j)
	// 	printf("same\n");
	// if (*k < *j)
	// 	printf("k is smaller\n");
	// printf("%d\n", *j - *k + 1);


	return 0;
}