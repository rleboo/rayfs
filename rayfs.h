#define RAYFS_DEFAULT_BLOCK_SIZE 4096
#define RAYFS_MAGIC 0x20191111  // Random Magic Num
#define MAX_FILENAME_LENGHT 255 // Max Char array
#define RAYFS_MAX_FILESYSTEM_BLOCKS 65
#define NUMBER_OF_INODES 32
#define NUMBER_OF_DATA_BLOCKS 32
#define INODE_BLOCK_START 1
#define DATA_BLOCK_START 33
#define SUPERBLOCK_START 0

/*
 *  FileSystem = |Superblock (4096 * 1) | 32 Inodes (4096 * 32) | 32 datablocks (4096 * 32) |
 * 
 */

struct rayfs_super_block {
	uint64_t magic;
	uint64_t block_size; 
	uint64_t inode_count;
};

struct rayfs_dentry {
	char filename[MAX_FILENAME_LENGHT];
	uint64_t inode_no; // Corresponding inode for particular file/directory
};

struct rayfs_inode {
	mode_t mode;  //Define what the inode can read, write, execute
	uint64_t inode_no;  // Which inode is this?
	uint64_t data_block_no; // Datablock that inode (file/dir) is stored
	union {
		uint64_t file_size;
		uint64_t children_count;
	};
};
