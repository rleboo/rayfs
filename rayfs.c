
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include "rayfs.h"

/* Declare the license so that the compiler
 * doesn't throw a warning
 */

MODULE_LICENSE("GPL");

static struct kmem_cache *rayfs_inode_cachep;

struct dentry* rayfs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags);

ssize_t rayfs_read(struct file * filp, char __user * buf, size_t len, loff_t *ppos)
// Reads the file from data block and copies the block of data into 
// the user space
{
	struct super_block *sb;
	struct inode *inode;
	struct rayfs_inode *ray_inode;
	struct buffer_head *bh;
	char *buffer;
	int nbytes;
	
	// Gets the rayfs_inode and its similiar linux inode
	inode = filp->f_path.dentry->d_inode;
	sb = inode->i_sb;
	ray_inode = inode->i_private;
	
	if(*ppos >= ray_inode->file_size)
	{
		return 0;
	}
	
	bh = sb_bread(sb, DATA_BLOCK_START + ray_inode->data_block_no); 
	// get data block according got the correspoding block in filesystem
	
	if(!bh)
	{
		printk(KERN_ERR "Couldn't read file data block bh");
	}
	
	buffer = (char *) bh->b_data; // Gets file characters (data)
	nbytes = min((size_t) ray_inode->file_size, len);
	// Gets the size of the file to appropriately write to user-space
	
	if (copy_to_user(buf, buffer, nbytes)) // Coppes the (to, from, how many bytes) 
	{
		brelse(bh);
		printk(KERN_ERR "Error copying file content to userspace buffer\n");
		return -EFAULT;
	}
	brelse(bh);
	*ppos += nbytes;
	// Present pointer has shifted by nbytes
	return nbytes;
	
	
}

static const struct file_operations rayfs_file_ops = {
// reads the file (to user-spaces) 
	.read = rayfs_read,
};

static const struct inode_operations rayfs_file_inode_ops = {
// Gets the inode of a dentry
	.getattr = simple_getattr
};


static int rayfs_iterate_dir(struct file * filp, struct dir_context *ctx)
// Looks through the items in a folder
{
	loff_t pos;
	struct buffer_head *bh;
	struct super_block *sb;
	struct inode *inode;
	struct rayfs_inode *ray_inode;
	struct rayfs_dentry *de;
	int over;
	int i;
	
	pos = ctx->pos;
	inode = file_inode(filp);
	sb = inode->i_sb; 
	
	ray_inode = inode->i_private;
	
	
	if (unlikely(!S_ISDIR(ray_inode->mode)))
	{
		printk(KERN_ERR "ray is not a directory");
		return -ENOTDIR;
	}
	
	bh = sb_bread(sb, DATA_BLOCK_START); // Root Directory Data block

	if(bh == NULL) 
	{
		printk(KERN_ALERT "couldn't read block\n");
		return -ENOMEM;
	}
	

	if(ctx->pos != 0 && ctx->pos > ray_inode->children_count-1)
	// If the same user-space process calls iterate again, we can ignore it
	{
		goto done;
	}
	
	for(i = 0;i < ray_inode->children_count; i++)
	// Looks through all items in folder
	{
		de =  (struct rayfs_dentry *) bh->b_data + i;
		
		over = dir_emit(ctx, de->filename, MAX_FILENAME_LENGHT, de->inode_no, DT_UNKNOWN);
		// Directory is locked and all positive dentries in it are safe
		// If its sucessful we are done iterating through
		// We do this for each entry in the directory. When we run out, we are finished
		if(over)
		{
			printk(KERN_DEBUG "We finished: Read %s from folder %s, ctx->pos: %lld\n",
			de->filename, filp->f_path.dentry->d_name.name, ctx->pos);
			ctx->pos++;
			brelse(bh);
			return i+1; // How many entries we found. +1 because loop starts at 0
			//goto done;
		}
		
	}
done:
	brelse(bh);
	return 0;
	
}


struct rayfs_inode * rayfs_get_rayfs_inode(struct super_block *sb, uint64_t inode_no)
// returns the rayfs inode for a given inode number (block position)
{
	struct buffer_head *bh;
	struct rayfs_inode *inode;
	struct rayfs_inode *inode_buf;
	
	bh = sb_bread(sb, inode_no); // inode_no should be passed in 1 greater than the actual value
	// this is because the first block is actually the superblock
	if(!bh)
	{
		printk(KERN_ALERT "This fucked up");
	}	
	inode = (struct rayfs_inode *) bh->b_data;
	printk(KERN_INFO "The inode number obtained in disk is: [%lld]\n", inode->inode_no);
	printk(KERN_INFO "The datablock number obtained in disk is: [%lld]\n", inode->data_block_no);
	printk(KERN_INFO "The children count  in disk is: [%lld]\n", inode->children_count);
	
	inode_buf = kmem_cache_alloc(rayfs_inode_cachep, GFP_KERNEL); // GFP_KERNEL means allocation is occuring on behalf
	// of a process running in kernal space
	// kmem_cache-alloc is used here to allocate a space for the predefined struct
	// We know that we are going to use inode structs multiple times, it creates
	// multiple copies which we can use (saves time in comparison to kmalloc
	memcpy(inode_buf, inode, sizeof(*inode_buf));
	// Copies from source directory to the memory block
	
	brelse(bh);
	return inode_buf;
}


static const struct inode_operations rayfs_dir_inode_op = {
// Finds the given dentry file (confirms that it exists) 
	.lookup = rayfs_lookup, // find a given dentry entry/file
};

static const struct file_operations rayfs_dir_ops = {
// Tells a user-space process what files are in this directory
	.iterate = rayfs_iterate_dir,
};

static struct inode *rayfs_iget(struct super_block *sb, int ino)
// Returns the given inode at the ino number. Uses sb_bread which
// splits the device (given by sb) into block sizes (given by sb) and finds
// the n block at the n block location
{
	struct inode * inode;
	struct rayfs_inode *ray_inode;
	
	ray_inode = rayfs_get_rayfs_inode(sb, ino+1); //+1 to factor superblock block
	// returns the rayfs inode then creates and fills the inode to be given
	// to the linux filesystem
	inode = new_inode(sb);
	inode->i_ino = ino;
	inode->i_sb = sb;
	
	if(S_ISDIR(ray_inode->mode))
	// if the inode is directory we give it the directory read and lookup
	// functions
	{
		inode->i_op = &rayfs_dir_inode_op;
		inode->i_fop = &rayfs_dir_ops;
	}
	
	if(S_ISREG(ray_inode->mode))
	// If the inode is a file we assign it the file read and lookup functions
	{
		inode->i_fop = &rayfs_file_ops;
		inode->i_op = &rayfs_file_inode_ops;
	}
	
	
	inode->i_atime = inode->i_mtime
						= inode->i_ctime
						= current_time(inode);
						
	inode->i_private = ray_inode; // Saves it for late user ;)
	return inode;
}


struct dentry* rayfs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags)
// Must scan through dentries on the device and see if any match the 
// given dentry
{	
	struct rayfs_inode *parent = parent_inode->i_private; // rayfs_inode of parent_inode
	struct super_block *sb = parent_inode->i_sb; // Get the linux superblock
	struct buffer_head *bh;
	struct rayfs_dentry *dentry;
	struct inode * inode;
	struct rayfs_inode *temp;
	int i;
	
	bh = sb_bread(sb, DATA_BLOCK_START + parent->data_block_no );
	// Start/get buffer at the given inode data block
			
	dentry = (struct rayfs_dentry *) bh->b_data;
	
	for(i = 0; i < parent->children_count; i++)
	// Loop through all children of given inode to find the given dentry
	{
		if (!strcmp(dentry->filename, child_dentry->d_name.name))
		// If the filenames match, we found the given dentry! 
		{
			inode = rayfs_iget(sb, dentry->inode_no);
			// Get the rayfs inode at the given block
			temp = inode->i_private;
			// Pass it to be created in the linux file system
			inode_init_owner(inode, parent_inode, S_IFDIR | 0777);
			// Attaches given dentry to the given inode
			d_add(child_dentry, inode);
			return NULL;			
		}
		dentry++;
	}
	
	return NULL;
}




static int rayfs_fill_super(struct super_block *sb, void *data, int silent)
// This function takes the devices superblock, and if valid, fills the kernel's
// superblock struct. 
{
	struct inode *root_inode;
	struct buffer_head *bh;
	struct rayfs_super_block *rayfs_sb;
	struct rayfs_inode * ray_inode;
	int ret = 0;

	bh = sb_bread(sb, SUPERBLOCK_START);  // Gets superblock from device
	printk(KERN_ALERT "Super fill start");

	if (!bh)
	// If no buffer
	{
		printk(KERN_ALERT "This is an error on fill super_block");
		return -1;
	}
	
	rayfs_sb = (struct rayfs_super_block *) bh->b_data;
			
	if (unlikely(rayfs_sb->magic != RAYFS_MAGIC))
	// If magic number of superblock doesn't match the defined one
	// Wasn't properly written/retrieved 
	{
		printk(KERN_ALERT "superblock's magic numbers don't match");
		goto release;
	}
	
	if(unlikely(rayfs_sb->block_size != RAYFS_DEFAULT_BLOCK_SIZE))
	// If the sb doesn't have the correct blocksize. Wasn't properly
	// written/retrieved
	{
		printk(KERN_ALERT "superblocks' block size dont match");
		goto release;
	}	
	
	// Fill system's sb with file system superblock contents
	sb->s_magic = RAYFS_MAGIC;
	sb->s_fs_info = rayfs_sb;
	sb->s_maxbytes = RAYFS_DEFAULT_BLOCK_SIZE;
	
	root_inode = new_inode(sb);
	// Allocates a new inode (root inode) for the given superblock
	root_inode->i_ino = INODE_BLOCK_START;
	// Code goes on to fill and register inode
	inode_init_owner(root_inode, NULL, S_IFDIR | 0777);
	ray_inode = rayfs_get_rayfs_inode(sb, INODE_BLOCK_START);
	// Returns the inode at the given block number
	
	if(!root_inode || !ray_inode)
	{
		// Error --> If any of the inodes are invalid (NULL) 
		ret = -ENOMEM;
		goto release;
	}
	
	// Further fill root inode 
	root_inode->i_sb = sb;  // Inode superblock (current sb) 
	root_inode->i_op = &rayfs_dir_inode_op;  // Reading file operations 
	root_inode->i_fop = &rayfs_dir_ops;      // Reading inode operations
	root_inode->i_atime = root_inode->i_mtime
						= root_inode->i_ctime
						= current_time(root_inode);
	root_inode->i_private = ray_inode;
	// Store important data for later use. In this case, store the 
	// orignal file system root inode
	sb->s_root = d_make_root(root_inode);
	// Make the root inode the root position (inode) of the filesytem

	if(!sb->s_root)
	{
		ret = -ENOMEM;
		goto release;
	}
	
	ret = 0;

release:
	brelse(bh); // Releases the buffer 
	printk(KERN_ALERT "RELEASE");
	return ret;

}

struct dentry *rayfs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void * data)
{	
	// Mounts rayfs - Making partition accessible and attaching it to the 
	// existing directory 
	return mount_bdev(fs_type, flags, dev_name, data, rayfs_fill_super);
}

struct file_system_type rayfs_type = 
// Filesystem to be registered. 
{	
	.owner = THIS_MODULE,  // Current module holds ownership of rayfs
	.name = "rayfs", // My name :D
	.mount = rayfs_mount, // calls mount function 
	.kill_sb = kill_block_super,
	.fs_flags = FS_REQUIRES_DEV
};

static int __init start_hello(void)
// Used to register file system
{
	int err;
	rayfs_inode_cachep = kmem_cache_create("rayfs_inode_cache", sizeof(struct rayfs_inode),
	0, (SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD), NULL); // Creates a cache the size of a inode
	// Will be used later to store a inode in cache

	printk(KERN_ALERT "Hello, world fromm the Linux kernel!\n");

	err = register_filesystem(&rayfs_type);
	if (err)
	  {
		  printk(KERN_ALERT "Unable to register file system\n");
		  return err;
	  }
	 
	/* Non-zero returned value means the module_init failed and the
	 * LKM can't be loaded
	 */
	return 0;
}

static void __exit end_hello(void)
// USed to unregister file system
{
	// Unregisters a given filesystem (rayfs0)
	printk(KERN_ALERT "Short is the life of an LKM\n");
	unregister_filesystem(&rayfs_type);
}

module_init(start_hello);
module_exit(end_hello);
