
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fat.h"

// Just some tests for local testing

unsigned long read_func (void* desc, void * ptr, unsigned long s) {
	fseek((FILE*)desc,s*BLOCK_SIZE,SEEK_SET);
	return fread(ptr,1,BLOCK_SIZE,(FILE*)desc);
}

unsigned long write_func (void* desc, void * ptr, unsigned long s) {
	fseek((FILE*)desc,s*BLOCK_SIZE,SEEK_SET);
	return fwrite(ptr,1,BLOCK_SIZE,(FILE*)desc);
}

int main(int argc, char ** argv) {
	if (argc != 2) {
		printf("Need file partition argument!\n");
		exit(1);
	}
	FILE* fd = fopen(argv[1],"rb+");

	struct fat_partition fat_part;
	open_partition(&fat_part, fd, read_func, write_func);

/*	char filenames[110][11];
	list_directory(&fat_part, "/DIR3/", &filenames[0][0], 110, 0);
	char ret = file_open(&fat_part, "/F1", 0);
	char mybuffer[1024];
	int ret2 = file_read(&fat_part, ret, mybuffer,512);
	ret2 = file_read(&fat_part, ret, mybuffer,512);
	ret2 = file_read(&fat_part, ret, mybuffer,512);
	mybuffer[ret2] = 0;
	printf("SIZE: %d\n%s\n",ret2,mybuffer);*/
if (1)
{
	char ret = file_open(&fat_part, "/F1", 0);
	char mybuf[512];
	int i;
	if (ret != -1) {
		for (i = 0; i < 512; i++) mybuf[i] = 'a'+(i*i)%24;
		for (i = 0; i < 256*16; i++)
			file_write(&fat_part, ret, mybuf, 512);
	}
}

if (1) {
	char ret = file_open(&fat_part, "/F1", 0);
	char mybuffer[1000*1024];
	int ret2 = file_read(&fat_part, ret, mybuffer,1000*1024);
	mybuffer[ret2] = 0;
//	printf("%s\nSIZE: %d\n",mybuffer,ret2);
}

	close(fd);
}


