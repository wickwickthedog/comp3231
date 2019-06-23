/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>


/*
 * Put your function declarations and data types here ...
 */
struct file {
	struct vnode* vnode; 	// pointer to the file's vnode
	int flags;			 	// check permissions
	int refcount;		 	// for dup2
	off_t offset;		 
	struct lock *file_lock;	// protect access to the fd
};

int sys_open(userptr_t filename, int flags, mode_t mode);
ssize_t sys_read(int fd, void *buffer, size_t nBytes);
ssize_t sys_write(int fd, void *buffer, size_t nBytes);
off_t sys_lseek(int fd, off_t offset, int whence);
int sys_close(int fd);
int sys_dup2(int fd, int newfd);
int stdio_init(void);

#endif /* _FILE_H_ */