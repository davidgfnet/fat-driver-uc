
#ifndef __FAT___H___
#define __FAT___H___

void read_partition_table(char part_num, void * buffer, unsigned long * start_block, unsigned long * num_blocks);

#endif

