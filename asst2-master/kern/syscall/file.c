#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <lib.h>
#include <uio.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <file.h>
#include <syscall.h>
#include <copyinout.h>
#include <proc.h>

/*
 * Add your file-related functions here ...
 */
int sys_open(userptr_t filename, int flags, mode_t mode)
{
    char fname[NAME_MAX];
    int result;
    size_t actual;

    /* check args */
    if (filename == NULL) return EFAULT; // invalid filename pointer

    // return 0 on success
	result = copyinstr(filename, fname, NAME_MAX, &actual);
	if (result) return EFAULT;      

    /*
	 * stdin stdout stderr thus fd set to 3
	 */
	int fd; 

    for (fd = 3;fd < OPEN_MAX; fd++) {
        if (curproc -> t_ft[fd] != NULL) continue;
        /* 
         * can't initialise
         * struct file *f = curproc -> t_ft[fd];
		 * f = kmalloc(sizeof(struct file));
         */
        curproc -> t_ft[fd] = kmalloc(sizeof(struct file));
        // fails to malloc
        if (curproc -> t_ft[fd] == NULL) return ENOMEM; // no memory

        curproc -> t_ft[fd] -> file_lock = lock_create("file_lock");
        // fails to create lock
        if (curproc -> t_ft[fd] -> file_lock == NULL) {
            kfree(curproc -> t_ft[fd]);
            return ENOMEM;
        }

        curproc -> t_ft[fd] -> flags = flags & O_ACCMODE; 	// Check flags contain Read Or Write
		KASSERT(curproc -> t_ft[fd] -> flags == O_RDONLY || curproc -> t_ft[fd] -> flags == O_WRONLY || curproc -> t_ft[fd] -> flags == O_RDWR);

        curproc -> t_ft[fd] -> refcount = 1;
        curproc -> t_ft[fd] -> offset = 0;

        result = vfs_open(fname, flags, mode, &(curproc -> t_ft[fd] -> vnode));
        if (result) {
            lock_destroy(curproc -> t_ft[fd] -> file_lock);
            kfree(curproc -> t_ft[fd]);
            return result;
        }

        break;	// exit loop once successfully vfs_open
    }

    if (fd < OPEN_MAX + 1) return fd; // success
  
    return EMFILE;
}

ssize_t sys_read(int fd, void *buffer, size_t nBytes) 
{
    int result;

    /* check args */
    if (fd < 0 || fd > OPEN_MAX + 1) return EBADF; // invalid fd

    if (curproc -> t_ft[fd] == NULL) return EBADF; // invalid fd

    if (buffer == NULL) return EFAULT; // invalid address space

    if (curproc -> t_ft[fd] -> flags == O_WRONLY) return EACCES; // permission denied

    if (nBytes <= 0) return EINVAL; // invalid arg (nBytes should be positive)
    
    // enter critical section
    lock_acquire(curproc -> t_ft[fd] -> file_lock);

    struct uio u;
    struct iovec iov;

    // helper function to initialize an iovec and uio for user I/O.
    uio_uinit(&iov, &u, buffer, nBytes, curproc -> t_ft[fd] -> offset, UIO_READ);

    size_t remaining = u.uio_resid;

    result = VOP_READ(curproc -> t_ft[fd] -> vnode, &u);
    if (result) {
		lock_release(curproc -> t_ft[fd] -> file_lock);
		return result;
	}

    // after VOP_READ, u.uio)resid is updated
    /* uio_resid will have been decremented by the amount transferred */
    remaining = nBytes - u.uio_resid;

    // updates the file offset from UIO
    curproc -> t_ft[fd] -> offset = u.uio_offset;
    // exit critical region
    lock_release(curproc -> t_ft[fd] -> file_lock);

    return remaining;
}

ssize_t sys_write(int fd, void *buffer, size_t nBytes) 
{
    int result;

    /* check args */
    if (fd < 0 || fd > OPEN_MAX + 1) return EBADF; // invalid fd

    if (curproc -> t_ft[fd] == NULL) return EBADF; // invalid fd

    if (buffer == NULL) return EFAULT; // invalid address space

    if (curproc -> t_ft[fd] -> flags == O_RDONLY) return EACCES; // permission denied

    if (nBytes <= 0) return EINVAL; // invalid arg (nBytes should be positive)
    
    //  enter critical section
    lock_acquire(curproc -> t_ft[fd] -> file_lock);

    struct uio u;
    struct iovec iov;

    // helper function to initialize an iovec and uio for user I/O.
    uio_uinit(&iov, &u, buffer, nBytes, curproc -> t_ft[fd] -> offset, UIO_WRITE);

    size_t remaining = u.uio_resid;

    result = VOP_WRITE(curproc -> t_ft[fd] -> vnode, &u);
    if (result) {
		lock_release(curproc -> t_ft[fd] -> file_lock);
		return result;
	}

    /* uio_resid will have been decremented by the amount transferred */
    remaining = nBytes - u.uio_resid;

    // updates the file offset from UIO
    curproc -> t_ft[fd] -> offset = u.uio_offset;

    // exit critial section
    lock_release(curproc -> t_ft[fd] -> file_lock);

    return remaining;
}

off_t sys_lseek(int fd, off_t offset, int whence)
{
	int result;
	/* check args */
    if (fd < 0 || fd > OPEN_MAX + 1) return EBADF; // invalid fd

    if (curproc -> t_ft[fd] == NULL) return EBADF; // invalid fd

    struct stat fStat;

    // enter critical section
    lock_acquire(curproc -> t_ft[fd] -> file_lock);
    result = -1;

    switch(whence) {
        /* Seek relative to beginning of the file */
        case SEEK_SET: // 0
            if (offset < 0) return EINVAL;  // invalid argument
            result = curproc -> t_ft[fd] -> offset = offset;
            break;

        /* Seek relative to current position in file */
        case SEEK_CUR: // 1
            result = curproc -> t_ft[fd] -> offset += offset;
            if (result < 0) return EINVAL; // seek position less than zero are invalid
            break;

        /* Seek relative to EOF of the file */
        case SEEK_END:
            result = VOP_STAT(curproc -> t_ft[fd] -> vnode, &fStat);
            if (result) {
                lock_release(curproc -> t_ft[fd] -> file_lock);
                return result;
            }
            result = curproc -> t_ft[fd] -> offset = offset + fStat.st_size; // seek positions beyond EOF are legal, at least on regular files.
            break;

        default:
            lock_release(curproc -> t_ft[fd] -> file_lock);
            return EINVAL;  		// invalid arg
    }
    if (!VOP_ISSEEKABLE(curproc -> t_ft[fd] -> vnode)) {
        lock_release(curproc -> t_ft[fd] -> file_lock);
        return ESPIPE; 				// illegal seek
    }
    // exit critical section
    lock_release(curproc -> t_ft[fd] -> file_lock);

    return result;
}

int sys_close(int fd)
{
	/* check args */
    if (fd < 0 || fd > OPEN_MAX + 1) return EBADF; // invalid fd

    if (curproc -> t_ft[fd] == NULL) return EBADF; // invalid fd
    // enter critical section
    lock_acquire(curproc -> t_ft[fd] -> file_lock);

    KASSERT( curproc -> t_ft[fd] -> refcount > 0); // each file has refcount of at least one after sys_open()
    curproc -> t_ft[fd] -> refcount--;

    /* Release file descriptor */
    if (curproc -> t_ft[fd] -> refcount == 0) {
        vfs_close(curproc -> t_ft[fd] -> vnode);

        lock_release(curproc -> t_ft[fd] -> file_lock);
        lock_destroy(curproc -> t_ft[fd] -> file_lock);
        kfree(curproc -> t_ft[fd]);
        curproc -> t_ft[fd] = NULL;	// updates the fd !
        return 0;
    }
    // exit critical section
    lock_release(curproc -> t_ft[fd] -> file_lock);

    return 0;   // on success
}

int sys_dup2(int fd, int newfd)
{
	int result;

	/* check args */
	if (fd < 0 || fd > OPEN_MAX + 1) return EBADF; // invalid fd

	if (newfd < 0 || newfd > OPEN_MAX + 1) return EBADF; // invalid fd

    if (curproc -> t_ft[fd] == NULL) return EBADF; // invalid fd

    if (newfd == fd) return newfd;
    // enter critical section
    lock_acquire(curproc -> t_ft[fd] -> file_lock);

    if (curproc -> t_ft[newfd] != NULL) {
        result = sys_close(newfd);
        if (result) {
            lock_release(curproc -> t_ft[fd] -> file_lock);
            return result;
        }
    }

    curproc -> t_ft[newfd] = curproc -> t_ft[fd];
    curproc -> t_ft[fd] -> refcount++;
    // exit critical section
    lock_release(curproc -> t_ft[fd] -> file_lock);

    return newfd;
}

int stdio_init() 
{
    int result;

    char c0[] = "con:";

    curproc -> t_ft[0] = kmalloc(sizeof(struct file));
    /*
	 * fails to malloc
	 */
    if (curproc -> t_ft[0] == NULL) return ENOMEM;

    curproc -> t_ft[0] -> flags = O_RDONLY;
    curproc -> t_ft[0] -> refcount = 1;
    curproc -> t_ft[0] -> offset = 0;
    /*
	 * STDIN_FILENO 0, Standard input 
	 */
	result = vfs_open(c0, curproc -> t_ft[0] -> flags, 0664, &curproc -> t_ft[0] -> vnode);
	if(result) return result;

	curproc -> t_ft[0] -> file_lock = lock_create("std_input");
    /*
	 * fails to create lock
	 */
    if (curproc -> t_ft[0] -> file_lock == NULL) {
        kfree(curproc -> t_ft[0]);
        return ENOMEM;
    }
    sys_close(0); // file descriptor 0 at the start of the program can start closed

    char c1[] = "con:";

    curproc -> t_ft[1] = kmalloc(sizeof(struct file));
    /*
	 * fails to malloc
	 */
    if (curproc -> t_ft[1] == NULL) return ENOMEM;  // no enough memory

    curproc -> t_ft[1] -> flags = O_WRONLY;
    curproc -> t_ft[1] -> refcount = 1;
    curproc -> t_ft[1] -> offset = 0;
    /*
	 * STDOUT_FILENO 1, Standard output 
	 */
	result = vfs_open(c1, curproc -> t_ft[1] -> flags, 0664, &curproc -> t_ft[1] -> vnode);
	if(result) return result;

	curproc -> t_ft[1] -> file_lock = lock_create("std_output");
    /*
	 * fails to create lock
	 */
    if (curproc -> t_ft[1] -> file_lock == NULL) {
        kfree(curproc -> t_ft[1]);
        return ENOMEM;      // no enough memory
    }

    char c2[] = "con:";

    curproc -> t_ft[2] = kmalloc(sizeof(struct file));
    /*
	 * fails to malloc
	 */
    if (curproc -> t_ft[2] == NULL) return ENOMEM;

    curproc -> t_ft[2] -> flags = O_WRONLY;
    curproc -> t_ft[2] -> refcount = 1;
    curproc -> t_ft[2] -> offset = 0;
    /*
	 * STDERR_FILENO 2, Standard error  
	 */
	result = vfs_open(c2, curproc -> t_ft[2] -> flags, 0664, &curproc -> t_ft[2] -> vnode);
	if(result) return result;

	curproc -> t_ft[2] -> file_lock = lock_create("std_error");
    /*
	 * fails to create lock
	 */
    if (curproc -> t_ft[2] -> file_lock == NULL) {
        kfree(curproc -> t_ft[2]);
        return ENOMEM;      // no enough memory
    }
    return 0;
}