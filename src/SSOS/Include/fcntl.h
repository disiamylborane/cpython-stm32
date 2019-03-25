#ifndef _UAPI_SSOS_FCNTL_H
#define _UAPI_SSOS_FCNTL_H


#define O_RDONLY        0x0001
#define O_WRONLY        0x0002
#define O_RDWR          O_RDONLY|O_WRONLY
#define O_APPEND        0x0008

#define O_CREAT         0x0100
#define O_TRUNC         0x0200
#define O_EXCL          0x0400

//#define O_TEXT          0x4000
//#define O_BINARY        0x8000
//#define O_RAW           0x0000
//#define O_TEMPORARY     0x0000
//#define O_NOINHERIT     0x0000
//#define O_SEQUENTIAL    0x0000
//#define O_RANDOM        0x0000		0x1000	/* Allow empty relative pathname */
//int open(const char *name, int flags, int perms);
//int close(int fd);
//int read(int fd, void * buffer, size_t count);
//int write(int fd, const void * buffer, size_t count);
//long lseek (int fd, long offset, int against);
//int fileno(FILE*);
//int isatty(int fd);
//long long SSOS_fd_size(int fd);


#endif /* _UAPI_LINUX_FCNTL_H */

