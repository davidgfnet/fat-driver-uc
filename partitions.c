
#include "partitions.h"

void read_partition_table(char part_num, void * buffer, unsigned long * start_block, unsigned long * num_blocks) {
	buffer += (0x01BE + part_num*16);
	*start_block = *(unsigned long*)&buffer[0x8];
	*num_blocks = *(unsigned long*)&buffer[0xC];
}


