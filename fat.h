
#ifndef __FAT___H___
#define __FAT___H___

#define BLOCK_SIZE        512
#define BLOCK_SIZE_SHIFT  9
#define FAT32_N_ENTRIES_SHIFT  7
#define FAT16_N_ENTRIES_SHIFT  8
#define FAT32_N_ENTRIES_MASK   0x7F
#define FAT16_N_ENTRIES_MASK   0xFF

#define FAT32_CACHE_ENTRIES  8
#define FAT32_CACHE_ENTRIES_MASK (FAT32_CACHE_ENTRIES-1)
#define FAT16_CACHE_ENTRIES  16
#define FAT16_CACHE_ENTRIES_MASK (FAT16_CACHE_ENTRIES-1)

#define DEBUG_INFO

#ifdef DEBUG_INFO
	#define DEBUG_CODE(a) a
#else
	#define DEBUG_CODE(a)
#endif

#define SEEK_REL    0
#define SEEK_START  1
#define SEEK_END    2

typedef unsigned long (*read_data_func)(void*, void *, unsigned long);
typedef unsigned long (*write_data_func)(void*, void *, unsigned long);


struct fat_partition {
	read_data_func read_fn_ptr;
	write_data_func write_fn_ptr;
	void *desc;

	union {
		struct {
			unsigned short fat_cache_lookup[FAT16_CACHE_ENTRIES];
			unsigned short fat_cache_values[FAT16_CACHE_ENTRIES];
		} fat16_cache_entries;
		struct {
			unsigned long fat_cache_lookup [FAT32_CACHE_ENTRIES];
			unsigned long fat_cache_values [FAT32_CACHE_ENTRIES];
		} fat32_cache_entries;
	} fat_cache_data;
	unsigned long fat_cache_valid;

	unsigned long clusters_per_fat;
	unsigned long fat_offset_cluster;
	unsigned long data_offset_cluster,data_offset_cluster_m2;
	unsigned long rootdir_offset_cluster;
	unsigned long blocks_per_cluster;
	unsigned long total_number_clusters;
	unsigned short bytes_per_sector;
	unsigned char fat_version, num_fats,sectors_per_cluster;
};

void file_system_init();
long open_partition(struct fat_partition * part, void * desc, read_data_func read_fn_ptr, write_data_func write_fn_ptr);
long list_directory(struct fat_partition * part, const char * dir, char * filename, unsigned char max_entries, unsigned long skip);
char file_open(struct fat_partition * part, const char *file, char mode);
int  file_read (struct fat_partition * part, char fd, void * buffer, int size);
int  file_write(struct fat_partition * part, char fd, void * buffer, int size);
int  file_seek (struct fat_partition * part, char fd, int rel, int offset);

#endif


