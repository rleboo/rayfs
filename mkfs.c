
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdint.h>

#include "rayfs.h"

int main(int argc, char *argv[])
{
	// Initialize all variables to be used
	int fd;
	int ret;
	int inode_mkfs_count;
	int data_block_mkfs_count;
	off_t nbytes;
	
	// Superblock of rayfs
	struct rayfs_super_block rayfs_sb = 
	{
		.magic = RAYFS_MAGIC,
		.block_size = RAYFS_DEFAULT_BLOCK_SIZE,  // Will be used by sb_bread
	};

	
	// Root folder inode
	struct rayfs_inode root_folder_inode = 
	{
		.mode = S_IFDIR | 0777, // Gives all users r,w,x permissions 
		.inode_no = 0,
		.data_block_no = 0,
		.children_count = 1, //The number of children associated to this inode
	};
	
	// Root folder data block containing a dentry with a readme.txt
	struct rayfs_dentry root_data_block[] = {
		{
			.filename = "readme.txt",
			.inode_no = 1, //Might need to replace with const var
		},
	};
	
	// Readme.txt inode
	char readme[] = "This is a readme.txt!!\n";
	struct rayfs_inode read_me_inode = 
	{
		.mode = S_IFREG,
		.inode_no = 1,
		.data_block_no = 1,
		.file_size = sizeof(readme),
	};
	
	
	// Directly write above defined structs to partition
		
	fd = open(argv[1], O_RDWR);  // Open partition to write
	if(fd == -1)
	{
		printf("Error opening device");
		return -1;
	}
	
	ret = 0;
	inode_mkfs_count = 0;
	data_block_mkfs_count =0;
	
	do {
		// Seeking then writing to memory. Ensures that the structs
		// are being written in the correct positions
		
		//Writing Superblock to partition
		if (sizeof(rayfs_sb) != write(fd, &rayfs_sb, sizeof(rayfs_sb)))
		{
			printf("Writing Superblock error");
			ret = -1;
			break;
		}
		
		// Go to bytes of starting inode: 4096 
		nbytes = RAYFS_DEFAULT_BLOCK_SIZE;
		if(((off_t)-1) == lseek(fd, nbytes, SEEK_SET))
		{
			printf("Seek Failed");
			ret = -2;
			break;
		}
		
		//Writiting root folder --> contains read_me.txt
		if (sizeof(root_folder_inode)!= write(fd, &root_folder_inode, sizeof(root_folder_inode)))
		{
			printf("Writing root_folder error");
			ret = -3;
			break;
		}
	
		inode_mkfs_count++;
		// Go to bytes of second inode: 8192
		nbytes = RAYFS_DEFAULT_BLOCK_SIZE + (RAYFS_DEFAULT_BLOCK_SIZE * inode_mkfs_count);
		if(((off_t)-1) == lseek(fd, nbytes , SEEK_SET))
		{
			printf("Seek Failed");
			ret = -4;
			break;
		}
		
		//Writining readme.txt inode to partiition
		if (sizeof(read_me_inode) != write(fd, &read_me_inode, sizeof(read_me_inode)))
		{
			printf("Error writinging readme inode");
			ret = -5;
			break;
		}
		
		
		//Datablocs
		// Go to bytes of starting datablock: (4096) * 33
		nbytes = (DATA_BLOCK_START * RAYFS_DEFAULT_BLOCK_SIZE);
		if(((off_t)-1) == lseek(fd, nbytes, SEEK_SET))
		{
			printf("Seek Failed");
			ret = -6;
			break;
		}

		//Writing data_block containing dentry for read_me 
		if (sizeof(root_data_block) != write(fd, &root_data_block, sizeof(root_data_block)))
		{
			printf("Writing root data block error");
			ret = -7;
			break;
		}
		
		data_block_mkfs_count++;
		// Seeking bytes of second datablock: (4096) * 34
		nbytes = DATA_BLOCK_START * RAYFS_DEFAULT_BLOCK_SIZE + (data_block_mkfs_count* RAYFS_DEFAULT_BLOCK_SIZE); 
		if(((off_t)-1) == lseek(fd, nbytes, SEEK_SET))
		{
			printf("Seek Failed");
			ret = -8;
			break;
		}
		
		// Write readme txt to datablock
		if(sizeof(readme) != write(fd, readme, sizeof(readme)))
		{
			printf("Failed to write readme.txt text");
			ret = -9;
			break;
		}
	} while(0);
	
	close(fd);
	return ret;
	
}
