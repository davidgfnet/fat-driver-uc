
#include <stdio.h>
#include "fat.h"
#include "fat_priv.h"

#define EXTRA_CLUSTER_HINT  7  // Allocate more free blocks than just one

#define NUM_FILE_DESC 4
unsigned char tbuffer[512];
struct fat_file_descriptor {
	unsigned long start_block_num;
	unsigned long curr_block_num;
	unsigned long filesize;
	unsigned long curr_pos;
	unsigned long dir_entry;
	char dir_entry_num;
	char mode;
	char used;
} file_descriptors[NUM_FILE_DESC];

void file_system_init() {
	int i;
	for (i = 0; i < NUM_FILE_DESC; i++)
		file_descriptors[i].used = 0;
}

// Macro. Given a block, looks up for the next one
#define NEXT_BLOCK(cb) \
	{ \
	cb++; \
    if ((cb % part->blocks_per_cluster) == 0) { \
		long cluster = (cb/part->blocks_per_cluster)-1; \
		cb = fat_lookup[part->fat_version](part,cluster); \
		if (cb >= END_LIST) cb = ~0; \
		else cb *= part->blocks_per_cluster; \
	} \
	}

#define MIN2(X,Y) ((X) < (Y) ? : (X) : (Y))

void fat32_cache_add(struct fat_partition * part, unsigned long index, unsigned long value) {
	int cind = index & FAT32_CACHE_ENTRIES_MASK;

	part->fat_cache_data.fat32_cache_entries.fat_cache_lookup[cind] = index;
	part->fat_cache_data.fat32_cache_entries.fat_cache_values[cind] = value;

	part->fat_cache_valid |= (1<<cind);  // Mark as valid
}
void fat16_cache_add(struct fat_partition * part, unsigned short index, unsigned short value) {
	int cind = index & FAT16_CACHE_ENTRIES_MASK;

	part->fat_cache_data.fat16_cache_entries.fat_cache_lookup[cind] = index;
	part->fat_cache_data.fat16_cache_entries.fat_cache_values[cind] = value;

	part->fat_cache_valid |= (1<<cind);  // Mark as valid
}

unsigned long fat32_cache_lookup(struct fat_partition * part, unsigned long index, char * valid) {
	int cind = index & FAT32_CACHE_ENTRIES_MASK;

	if ((part->fat_cache_valid & (1<<cind)) != 0 && part->fat_cache_data.fat32_cache_entries.fat_cache_lookup[cind] == index) {
		*valid = 1;
		return part->fat_cache_data.fat32_cache_entries.fat_cache_values[cind];
	}else{
		*valid = 0;
		return 0;
	}
}
unsigned short fat16_cache_lookup(struct fat_partition * part, unsigned short index, char * valid) {
	int cind = index & FAT16_CACHE_ENTRIES_MASK;

	if ((part->fat_cache_valid & (1<<cind)) != 0 && part->fat_cache_data.fat16_cache_entries.fat_cache_lookup[cind] == index) {
		DEBUG_CODE(printf("Cache hit!\n");)
		*valid = 1;
		return part->fat_cache_data.fat16_cache_entries.fat_cache_values[cind];
	}else{
		DEBUG_CODE(printf("Cache miss!\n");)
		*valid = 0;
		return 0;
	}
}

// Compares a filename and a full path and tells us
// if they match
int dir_compare_83(const char * dir, char * fn83) {
	int i, last_slash = 1;
	if (dir[0] != '/') return -1;
	while (dir[last_slash] != '/' && dir[last_slash] != 0)
		last_slash++;

	for (i = 1; i < last_slash; i++)
		if (fn83[i-1] != dir[i])
			return -1;
		else if (fn83[i-1] == 0 || fn83[i-1] == ' ')
			return -1;

	while (dir[last_slash+1] == '/') last_slash++;

	return last_slash;
}

// A simple copy data function
void copy_data(void *dst, void *src, long size) {
	char * s = (char*)src;
	char * d = (char*)dst;
	while ((size--) > 0)
		*d++ = *s++;
}

// Given a partition and a cluster number looks up for the next cluster (FAT chain)
long fat_lookup_f32(struct fat_partition * part, long cluster) {
	unsigned long block_offset, cluster_offset;
	unsigned long res;
	char valid;

	cluster = cluster - part->data_offset_cluster_m2;

	// First of all have a look at the cache
	res = fat32_cache_lookup(part,cluster,&valid);

	if (valid == 0) {
		// Lookup the first FAT
		block_offset = part->fat_offset_cluster*part->blocks_per_cluster+(cluster>>FAT32_N_ENTRIES_SHIFT);

		part->read_fn_ptr(part->desc,tbuffer,block_offset);

		cluster_offset = cluster&FAT32_N_ENTRIES_MASK;
		res = ((unsigned long*)tbuffer)[cluster_offset];
	}

	if (res < END_LIST) res += (part->data_offset_cluster_m2);
	else res = ~0;

	return res;
}

// Given a partition and a cluster number looks up for the next cluster (FAT chain)
long fat_lookup_f16(struct fat_partition * part, long cluster) {
	unsigned long block_offset, cluster_offset;
	unsigned long res;
	char valid;

	cluster = cluster - part->data_offset_cluster_m2;

	// First of all have a look at the cache
	res = fat16_cache_lookup(part,cluster,&valid);
	if (valid == 0) {
		// Lookup the first FAT
		block_offset = part->fat_offset_cluster*part->blocks_per_cluster+(cluster>>FAT16_N_ENTRIES_SHIFT);

		part->read_fn_ptr(part->desc,tbuffer,block_offset);

		cluster_offset = cluster&FAT16_N_ENTRIES_MASK;
		res = ((unsigned short*)tbuffer)[cluster_offset];
	}

	if (res >= F16_END_LIST) res = ~0;
	else res += (part->data_offset_cluster_m2);

	return res;
}

// Update the FAT entry in all the fats
void fat_update_f32(struct fat_partition * part, unsigned long cluster, unsigned long value) {
	unsigned long block_offset, cluster_offset;
	unsigned int count = part->clusters_per_fat*part->blocks_per_cluster;

	// Read from the first FAT, update and write back them all
	cluster = cluster - part->data_offset_cluster_m2;
	value = value - part->data_offset_cluster_m2;
	block_offset = part->fat_offset_cluster*part->blocks_per_cluster+(cluster>>FAT32_N_ENTRIES_SHIFT);

	part->read_fn_ptr(part->desc,tbuffer,block_offset);

	cluster_offset = cluster&FAT32_N_ENTRIES_MASK;
	((unsigned long*)tbuffer)[cluster_offset] = value;

	{ int nf;
		for (nf = 0; nf < part->num_fats; nf++)
			part->write_fn_ptr(part->desc,tbuffer,block_offset+count*nf);
	}

	fat32_cache_add(part,cluster,value);
}

// Update the FAT entry in all the fats
void fat_update_f16(struct fat_partition * part, unsigned long cluster, unsigned long value) {
	unsigned long block_offset, cluster_offset;
	unsigned int count = part->clusters_per_fat*part->blocks_per_cluster;

	// Read from the first FAT, update and write back them all
	cluster = cluster - part->data_offset_cluster_m2;
	value = value - part->data_offset_cluster_m2;
	block_offset = part->fat_offset_cluster*part->blocks_per_cluster+(cluster>>FAT16_N_ENTRIES_SHIFT);

	part->read_fn_ptr(part->desc,tbuffer,block_offset);

	cluster_offset = cluster&FAT16_N_ENTRIES_MASK;
	((unsigned short*)tbuffer)[cluster_offset] = value;

	{ int nf;
		for (nf = 0; nf < part->num_fats; nf++)
			part->write_fn_ptr(part->desc,tbuffer,block_offset+count*nf);
	}

	fat16_cache_add(part,cluster,value);
}

// Searches a free cluster, marks it as used and last one, updates disk and returns the cluster number
// Using extra clusters hint advises to reserve more clusters and to create a valid cluster chain,
// so that the returned cluster is the first of a chain which finishes properly
unsigned long find_free_cluster_f16(struct fat_partition * part, unsigned char extra_clusters_hint) {
	unsigned long n;
	unsigned int count = part->clusters_per_fat*part->blocks_per_cluster;
	unsigned int init = part->fat_offset_cluster*part->blocks_per_cluster;
	int offset = -1;
	int nf;
	int last_cl;
	int revd = (part->data_offset_cluster-part->rootdir_offset_cluster)/(BLOCK_SIZE/sizeof(short)) + 1;
	// The FS reserves a few clusters for the data, we calculate the skip bad, so we loose some space, but we don't care

	for (n = revd; n < count; n++) {
		int i;
		unsigned short * list = (unsigned short*)tbuffer;
		part->read_fn_ptr(part->desc,tbuffer,n+init);
		for (i = 0; i < (BLOCK_SIZE/sizeof(short)); i++) {
			if (list[i] == 0) {
				if (offset == -1) {  // First free block found
					list[i] = ~0; // Mark as used and EOF!
					fat16_cache_add(part,(n<<FAT16_N_ENTRIES_SHIFT)+i,~0);
					offset = i;
					last_cl = i;
					if (extra_clusters_hint == 0) break;
				}else{
					// Found a free block, but we already reserved one
					list[last_cl] = i+(n<<FAT16_N_ENTRIES_SHIFT);  // Update the last block to form a chain
									//n*(BLOCK_SIZE/sizeof(short))+i;
					fat16_cache_add(part,(n<<FAT16_N_ENTRIES_SHIFT)+last_cl,list[last_cl]);
					list[i] = ~0; // Last
					fat16_cache_add(part,(n<<FAT16_N_ENTRIES_SHIFT)+i,~0);
					last_cl = i;
					if ((--extra_clusters_hint) == 0) break;
				}
			}
		}
		if (offset != -1) break;
	}

	// Now mark the clusters as used in the all the FATs
	for (nf = 0; nf < part->num_fats; nf++) {
		part->write_fn_ptr(part->desc,tbuffer,n+init+count*nf);
	}

	return n*(BLOCK_SIZE/sizeof(short)) + offset + part->data_offset_cluster_m2;
}

// Finds a free cluster of data
unsigned long find_free_cluster_f32(struct fat_partition * part, unsigned char extra_clusters_hint) {
	unsigned long n;
	unsigned int count = part->clusters_per_fat*part->blocks_per_cluster;
	unsigned int init = part->fat_offset_cluster*part->blocks_per_cluster;
	int offset = -1;
	int nf;
	int last_cl;
	int revd = (part->data_offset_cluster-part->rootdir_offset_cluster)/(BLOCK_SIZE/sizeof(short)) + 1;

	for (n = revd; n < count; n++) {
		int i;
		unsigned long * list = (unsigned long*)tbuffer;
		part->read_fn_ptr(part->desc,tbuffer,n+init);
		for (i = 0; i < (BLOCK_SIZE/sizeof(long)); i++) {
			if (list[i] == 0) {
				if (offset == -1) {  // First free block found
					list[i] = ~0; // Mark as used and EOF!
					fat32_cache_add(part,(n<<FAT32_N_ENTRIES_SHIFT)+i,~0);
					offset = i;
					last_cl = i;
					if (extra_clusters_hint == 0) break;
				}else{
					// Found a free block, but we already reserved one
					list[last_cl] = i+(n<<FAT32_N_ENTRIES_SHIFT);  // Update the last block to form a chain
					fat32_cache_add(part,(n<<FAT32_N_ENTRIES_SHIFT)+last_cl,list[last_cl]);
					list[i] = ~0; // Last
					fat32_cache_add(part,(n<<FAT32_N_ENTRIES_SHIFT)+i,~0);
					last_cl = i;
					if ((--extra_clusters_hint) == 0) break;
				}
			}
		}
		if (offset != -1) break;
	}

	// Now mark the clusters as used in the all the FATs
	for (nf = 0; nf < part->num_fats; nf++) {
		part->write_fn_ptr(part->desc,tbuffer,n+init+count*nf);
	}

	return n*(BLOCK_SIZE/sizeof(long)) + offset + part->data_offset_cluster_m2;
}

unsigned long (*find_free_cluster[3])(struct fat_partition*,unsigned char) = {0,find_free_cluster_f16,find_free_cluster_f32};
long (*fat_lookup[3])(struct fat_partition * part, long cluster) = {0,fat_lookup_f16, fat_lookup_f32};
void (*fat_update[3])(struct fat_partition*, unsigned long, unsigned long) = {0,fat_update_f16, fat_update_f32};

// part is a valid polonger to a fat_partition structure hold by the user
// desc is the descriptor to pass the read/write functions
long open_partition(struct fat_partition * part, void * desc, read_data_func read_fn_ptr, write_data_func write_fn_ptr) {
	unsigned long num_tot_sect, wasted_sectors, num_clusters;

	part->read_fn_ptr = read_fn_ptr;
	part->write_fn_ptr = write_fn_ptr;
	part->desc = desc;

	// Read the header
	read_fn_ptr(desc,tbuffer,0);

	part->num_fats = HEAD_GET_NUM_FAT_COPIES(tbuffer);
	part->sectors_per_cluster = HEAD_GET_SECTORS_PER_CLUSTER(tbuffer);

	// Determine the partition version (FAT type)
	num_tot_sect = HEAD_GET_NUM_TOTAL_SECTORS(tbuffer);
	if (num_tot_sect == 0) num_tot_sect = HEAD_FAT32_GET_TOTAL_NUM_SECTORS(tbuffer);
	wasted_sectors = (HEAD_GET_NUM_ROOT_ENTRIES(tbuffer)*32/part->bytes_per_sector)
					 + HEAD_GET_NUM_RESERVED_SECTORS(tbuffer)
					 + HEAD_GET_NUM_SECTORS_PER_FAT(tbuffer)*part->num_fats;
	num_clusters = (num_tot_sect-wasted_sectors)/part->sectors_per_cluster;

	part->clusters_per_fat = HEAD_GET_NUM_SECTORS_PER_FAT(tbuffer)/part->sectors_per_cluster;
	part->total_number_clusters = num_tot_sect/part->sectors_per_cluster;
	part->bytes_per_sector = HEAD_GET_BYTES_PER_SECTOR(tbuffer);

	if (num_clusters < 4085) {
		part->fat_version = FAT12_V;
	}
	else if (num_clusters < 65525) {
		part->fat_version = FAT16_V;
		part->total_number_clusters = HEAD_FAT16_GET_TOTAL_NUM_SECTORS(tbuffer)/part->sectors_per_cluster;
	}
	else {
		part->fat_version = FAT32_V;
		part->total_number_clusters = HEAD_FAT32_GET_TOTAL_NUM_SECTORS(tbuffer)/part->sectors_per_cluster;
		part->clusters_per_fat = HEAD_FAT32_GET_SECTORS_PER_FAT(tbuffer)/part->sectors_per_cluster;
	}

	part->fat_offset_cluster = HEAD_GET_NUM_RESERVED_SECTORS(tbuffer)/part->sectors_per_cluster;
	part->rootdir_offset_cluster = part->fat_offset_cluster + part->num_fats*part->clusters_per_fat;
	part->data_offset_cluster = (HEAD_GET_NUM_ROOT_ENTRIES(tbuffer)*32/part->bytes_per_sector)/part->sectors_per_cluster + part->rootdir_offset_cluster;
	part->data_offset_cluster_m2 = part->data_offset_cluster-2;
	part->blocks_per_cluster = (HEAD_GET_BYTES_PER_SECTOR(tbuffer)*part->sectors_per_cluster)/BLOCK_SIZE;

	// Clean the cache
	part->fat_cache_valid = 0;

	DEBUG_CODE(
	printf("Loaded a fat %d\n",part->fat_version);
	printf("Number of fats: %d\n",part->num_fats);
	printf("Bytes per sector: %d\n",part->bytes_per_sector);
	printf("Sectors per cluster: %d\n",part->sectors_per_cluster);
	printf("Clusters per fat: %d\n",part->clusters_per_fat);
	printf("Total cluster count: %d\n",part->total_number_clusters);
	printf("Data start cluster offset: %d\n",part->data_offset_cluster);
	)

	return 1;
}


// Given an open partition and a path returns the block number of the file or directory
// specified by path and in case dir_block and entry_number are not null their values
// will be the block of the directory which contains the file/directory and the entry
// number (offset) to the entry

unsigned long seek_path(struct fat_partition * part, const char * path, long * dir_block, char * entry_number) {
	unsigned long dir_entry = part->rootdir_offset_cluster*part->blocks_per_cluster;
	char root_dir = 1;
	if (path[0] == '/' && path[1] == 0) return dir_entry;

Restart_Search: {}

	while (1) {
		int j;
		for (j = 0; j < part->blocks_per_cluster; j++) {
			// Read a dir block!
			int i;
			struct directory_entry * entry = (struct directory_entry *)tbuffer;

			part->read_fn_ptr(part->desc,entry,dir_entry);

			for (i = 0; i < (BLOCK_SIZE/sizeof(struct directory_entry)); i++) {
				if (entry->filename[0] == 0) return ~0;
				if ((entry->attr&LONG_FILENAME) != LONG_FILENAME && ((unsigned char)entry->filename[0] != DELETED_FILE)) {
					int skip = dir_compare_83(path,entry->filename);
					if (skip > 0) {
						long old_dir_entry = dir_entry;
						long cluster = ((((unsigned long)entry->hcluster)<<16) | (entry->lcluster));
						cluster = (part->data_offset_cluster_m2 + cluster);
						dir_entry = cluster*part->blocks_per_cluster;
						root_dir = 0;

						path += skip;
						if ( ((entry->attr&IS_DIR) == IS_DIR && path[0] == '/' && path[1] == 0) || 
						     ((entry->attr&IS_DIR) != IS_DIR && path[0] == 0) ) {

							if (dir_block != 0) *dir_block = old_dir_entry;
							if (entry_number != 0) *entry_number = i;
							return dir_entry;
						}
						
						goto Restart_Search;
					}
				}
				entry++;
			}

			// Search the next directory entry, in the same cluster
			dir_entry++;
		}

		if (part->fat_version == FAT32_V || !root_dir) {
			// We pass trough a cluster, we'll need to lookup the FAT
			long cluster = (dir_entry/part->blocks_per_cluster)-1;
			dir_entry = fat_lookup[part->fat_version](part,cluster);
			if (dir_entry >= END_LIST) return ~0; // Reached the end of dir
			dir_entry *= part->blocks_per_cluster;
		}
		//NEXT_BLOCK(dir_entry)
	}

	return ~0;
}


long list_directory(struct fat_partition * part, const char * dir, char * filename, unsigned char max_entries, unsigned long skip) {
	long nfound = 0;
	char root_dir = (dir[0] == '/' && dir[1] == 0);
	unsigned long dir_entry = seek_path(part, dir, 0, 0);
	if (dir_entry == ~0) return 0;

	while (max_entries > 0) {
		int j;
		for (j = 0; j < part->blocks_per_cluster; j++) {
			int i;
			// Read a dir block!
			struct directory_entry * entry = (struct directory_entry *)tbuffer;

			part->read_fn_ptr(part->desc,entry,dir_entry);

			for (i = 0; i < (BLOCK_SIZE/sizeof(struct directory_entry)); i++) {
				if (entry->filename[0] == 0) return nfound; // Finished!
				if ((entry->attr&LONG_FILENAME) != LONG_FILENAME && ((unsigned char)entry->filename[0] != DELETED_FILE)) {
					DEBUG_CODE( printf("%11s\n",entry->filename); )
					copy_data(filename,entry->filename,11);
					filename += 11;
					nfound++; max_entries--;
					if (max_entries == 0) return nfound;
				}
				entry++;
			}
			dir_entry++;
		}

		if (part->fat_version == FAT32_V || !root_dir) {
			// We pass trough a cluster, we'll need to lookup the FAT
			long cluster = (dir_entry/part->blocks_per_cluster)-1;
			dir_entry = fat_lookup[part->fat_version](part,cluster);
			if (dir_entry >= END_LIST) return nfound; // Reached the end of dir
			dir_entry *= part->blocks_per_cluster;
		}
	}

	return nfound;
}

// Opens a file and fills its file descriptor
char file_open(struct fat_partition * part, const char *file, char mode) {
	int i;
	for (i = 0; i < NUM_FILE_DESC; i++) {
		if(file_descriptors[i].used == 0)
			break;
	}
	if (i == NUM_FILE_DESC) return -1;

	file_descriptors[i].used = 1;
	file_descriptors[i].mode = mode;
	file_descriptors[i].curr_pos = 0;
	file_descriptors[i].start_block_num = file_descriptors[i].curr_block_num = 
			seek_path(part, file, &file_descriptors[i].dir_entry, &file_descriptors[i].dir_entry_num);

	if (file_descriptors[i].start_block_num != ~0) {
		struct directory_entry * entry = (struct directory_entry *)tbuffer;
		entry += file_descriptors[i].dir_entry_num;

		part->read_fn_ptr(part->desc,tbuffer,file_descriptors[i].dir_entry);
		file_descriptors[i].filesize = entry->filesize;
	}else{
		file_descriptors[i].used = 0;
		DEBUG_CODE(printf(" [!] File not found! %s\n",file);)
	}

	DEBUG_CODE(
	printf(" * File size: %d\n",file_descriptors[i].filesize);
	printf(" * First cluster: %d\n",file_descriptors[i].start_block_num/part->blocks_per_cluster-part->data_offset_cluster_m2);
	printf(" * Block dir. entry: %d\n",file_descriptors[i].dir_entry);
	printf(" * Block dir. entry offset: %d\n",file_descriptors[i].dir_entry_num);
	)

	return i;
}

// Closes a file descriptor to free it
void file_close(struct fat_partition * part, char fd) {
	file_descriptors[(int)fd].used = 0;
}

// Travels from a cluster to another by a number n
unsigned long fat_move (unsigned long start,unsigned long n,struct fat_partition * part) {
	while (n--) {
		start = fat_lookup[part->fat_version](part,start);
		if (start >= END_LIST) return ~0; // Reached the end!
	}
	return start;
}

// Goes to the next block (through the FAT chain) and, if neecessary, allocates blocks
unsigned long next_block_walloc(struct fat_partition * part, unsigned long cblock) {
	unsigned long previous = cblock;
	NEXT_BLOCK(cblock)
	if (cblock == ~0) {
		// Allocate a new block! and complete the chain
		cblock = find_free_cluster[part->fat_version](part,EXTRA_CLUSTER_HINT);
		fat_update[part->fat_version](part,((previous+1)/part->blocks_per_cluster)-1,cblock);
		cblock *= part->blocks_per_cluster;
	}
	return cblock;
}

// Reads data from a file
int file_read(struct fat_partition * part, char fd, void * buffer, int size) {
	int tsize,pos_offset;
	unsigned long cblock;

	// Basic checks
	{
		int remaining_size = file_descriptors[(int)fd].filesize - file_descriptors[(int)fd].curr_pos;
		if (size > remaining_size) size = remaining_size;

		if (size <= 0) return 0; // EOF or requested a zero length read
	}
	tsize = size;
	pos_offset = file_descriptors[(int)fd].curr_pos % BLOCK_SIZE;
	cblock = file_descriptors[(int)fd].curr_block_num;
	file_descriptors[(int)fd].curr_pos += size;

	while (tsize > 0) {
		int adv_block;
		int can_copy = BLOCK_SIZE-pos_offset;
		if (can_copy > size) can_copy = size;
		adv_block = (can_copy+pos_offset == BLOCK_SIZE);

		part->read_fn_ptr(part->desc,tbuffer,cblock);

		copy_data(buffer, &tbuffer[pos_offset], can_copy);
		buffer += can_copy;

		pos_offset = 0;
		tsize -= can_copy;

		if (adv_block)
			cblock = next_block_walloc(part,cblock);
	}
	file_descriptors[(int)fd].curr_block_num = cblock;
	return size;
}

void update_dir_entry(struct fat_partition * part, char fd) {
	struct directory_entry * entry = (struct directory_entry *)tbuffer;
	part->read_fn_ptr(part->desc,tbuffer,file_descriptors[(int)fd].dir_entry);

	entry[file_descriptors[(int)fd].dir_entry_num].filesize = file_descriptors[(int)fd].filesize;
	{
		unsigned long realc = file_descriptors[(int)fd].start_block_num/part->blocks_per_cluster-part->data_offset_cluster_m2;
		entry[file_descriptors[(int)fd].dir_entry_num].lcluster = (unsigned short)(realc&0xFFFF);
		entry[file_descriptors[(int)fd].dir_entry_num].hcluster = (unsigned short)(realc>>16);
	}

	part->write_fn_ptr(part->desc,tbuffer,file_descriptors[(int)fd].dir_entry);
}

// Writes data to a file
int file_write(struct fat_partition * part, char fd, void * buffer, int size) {
	int tsize = size;
	int pos_offset = file_descriptors[(int)fd].curr_pos % BLOCK_SIZE;
	unsigned long cblock = file_descriptors[(int)fd].curr_block_num;
	file_descriptors[(int)fd].curr_pos += size;
	if (file_descriptors[(int)fd].curr_pos > file_descriptors[(int)fd].filesize)
		file_descriptors[(int)fd].filesize = file_descriptors[(int)fd].curr_pos;

	if (cblock == (part->data_offset_cluster_m2)*part->blocks_per_cluster) {
		// Important!!! Sometimes zero length files point to cluster 0!!!
		// This does NOT mean they are really pointing to data at sect. 0, it's just
		// a way to keep out 0 length files from actually using disk data space
		cblock = part->blocks_per_cluster*find_free_cluster[part->fat_version](part,EXTRA_CLUSTER_HINT);
		file_descriptors[(int)fd].curr_block_num  = cblock;
		file_descriptors[(int)fd].start_block_num = cblock;
		// update_dir_entry(part, fd); // No need!
	}

	while (tsize > 0) {
		int adv_block;
		int can_copy = BLOCK_SIZE-pos_offset;
		if (can_copy > size) can_copy = size;
		adv_block = (can_copy+pos_offset == BLOCK_SIZE);

		// No need to read data in case we replace it all
		if (can_copy != BLOCK_SIZE)
			part->read_fn_ptr(part->desc,tbuffer,cblock);
		copy_data(&tbuffer[pos_offset], buffer, can_copy);
		part->write_fn_ptr(part->desc,tbuffer,cblock);

		buffer += can_copy;

		pos_offset = 0;
		tsize -= can_copy;

		if (adv_block)
			cblock = next_block_walloc(part,cblock);
	}
	file_descriptors[(int)fd].curr_block_num = cblock;

	update_dir_entry(part,fd);

	return size;
}

int  file_seek (struct fat_partition * part, char fd, int rel, int offset) {
	int fsize = file_descriptors[(int)fd].filesize;
	int fpos  = file_descriptors[(int)fd].curr_pos;
	int newpos;

	switch(rel) {
	case SEEK_REL:
		newpos = fpos + offset;
		break;
	case SEEK_START:
		newpos = offset;
		break;
	case SEEK_END:
		newpos = fsize + offset;
		break;
	default:
		return 0;
	}

	if (newpos < 0) newpos = 0;
	else if (newpos > fsize) {
		// TODO
	}
}


