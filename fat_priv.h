
#ifndef __FAT_PRIV___H___
#define __FAT_PRIV___H___

#define FAT12_V 0
#define FAT16_V 1
#define FAT32_V 2

#define F16_END_LIST 0xFFF8
#define END_LIST 0x0FFFFFF8

#define IS_DIR 0x10
#define LONG_FILENAME (0xF)
#define DELETED_FILE (0xE5)

struct directory_entry {
	char filename[11];
	unsigned char attr;
	unsigned char wcase;
	unsigned char creat_ms;	
	unsigned short creat_time;
	unsigned short creat_date;
	unsigned short last_access;
	unsigned short hcluster;
	unsigned short timestamp;
	unsigned short datestamp;
	unsigned short lcluster;
	unsigned long filesize;
};

/*
struct fat_32_header {
	unsigned long  total_number_sectors;   //  0
	unsigned long sectors_per_fat;         //  4
	unsigned short mirror_flags;           //  8
	unsigned short fs_version;             // 10
	unsigned long first_cluster_root;      // 12
	unsigned short fs_info;                // 16
	unsigned short backup_boot_location;   // 18
	unsigned char reserved[12];            // 20
	unsigned char logical_drive_num;       // 32
	unsigned char reserved2;               // 33
	unsigned char ext_signature; // 0x29   // 34
	unsigned long serial_no;               // 35
	unsigned char volume_label[10];        // 39
	unsigned char fs_type[8];              // 49
} __attribute__ ((packed));

struct fat_16_header {
	unsigned long  total_number_sectors;
	unsigned char logical_drive_num;
	unsigned char reserved;
	unsigned char ext_signature; // 0x29 or 0x28
	unsigned long serial_no;
	unsigned char volume_label[10];
	unsigned char fs_type[8];
} __attribute__ ((packed));

struct fat_header {
	unsigned char bootstrap[3];              //  0
	unsigned char OEMname[8];                //  3
	unsigned short bytes_per_sector;         // 11
	unsigned char  sectors_per_cluster;      // 13
	unsigned short num_reserved_sectors;     // 14
	unsigned char  num_fat_copies;           // 16
	unsigned short num_root_dir_entries;     // 17
	unsigned short num_total_sectors;        // 19
	unsigned char  media_desc;               // 21
	unsigned short num_sectors_per_fat;      // 22
	unsigned short num_sectors_per_track;    // 24
	unsigned short num_heads;                // 26
	unsigned long  num_hidden_sectors;       // 28
	union fat_version_specific {             // 32
		unsigned char padding[478];
		struct fat_16_header fat16;
		struct fat_32_header fat32;
	} specific;
	unsigned char signature[2];              // 510
} __attribute__ ((packed));

struct directory_entry {
	char filename[11];
	unsigned char attr;
	unsigned char wcase;
	unsigned char creat_ms;	
	unsigned short creat_time;
	unsigned short creat_date;
	unsigned short last_access;
	unsigned short hcluster;
	unsigned short timestamp;
	unsigned short datestamp;
	unsigned short lcluster;
	unsigned long filesize;
} __attribute__ ((packed));
*/

#define HEAD_GET_BYTES_PER_SECTOR(head) (*(unsigned short*)(&head[11]))
#define HEAD_GET_SECTORS_PER_CLUSTER(head) (*(unsigned char*)(&head[13]))
#define HEAD_GET_NUM_TOTAL_SECTORS(head) (*(unsigned short*)(&head[19]))
#define HEAD_GET_NUM_RESERVED_SECTORS(head) (*(unsigned short*)(&head[14]))

#define HEAD_GET_NUM_SECTORS_PER_FAT(head) (*(unsigned short*)(&head[22]))
#define HEAD_GET_NUM_FAT_COPIES(head) (*(unsigned char*)(&head[16]))
#define HEAD_GET_NUM_ROOT_ENTRIES(head) (*(unsigned short*)(&head[17]))

#define HEAD_FAT32_GET_TOTAL_NUM_SECTORS(head) (*(unsigned long*)(&head[32]))
#define HEAD_FAT32_GET_SECTORS_PER_FAT(head) (*(unsigned long*)(&head[36]))

#define HEAD_FAT16_GET_TOTAL_NUM_SECTORS(head) (*(unsigned long*)(&head[32]))

#endif


