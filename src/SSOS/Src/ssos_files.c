
#include "ff.h"
#include "fcntl.h"
#include "SSOS_private.h"

FILE __stdin={0};
FILE __stdout={1};
FILE __stderr={2};

struct _SSOS_file_stream
{
	FIL* fip;
}SSOS_file_stream[_FS_LOCK];

static FIL* get_fil_by_fd(int fd)
{
	return SSOS_file_stream[fd-3].fip;
}

static int search_set_descriptor(FIL* f)
{
	for(int i=0; i<_FS_LOCK; i++)
	{
		if(SSOS_file_stream[i].fip == NULL)
		{
			SSOS_file_stream[i].fip = f;
			return i+3;
		}
	}
	return -1;
}

unsigned char SSOS_valid_fd(int fd)
{
	if(fd > 0 && fd <= 2)
		return 1;
	return !!(SSOS_file_stream[fd-3].fip);
}

int open(const char *name, int flags, int perms)
{
	register int ret;
	FIL* f=malloc(sizeof(FIL));
	if(!f)
	{
		//set errno
		return -1;
	}
	BYTE f_mode = 0;

	if(flags & O_RDONLY)
		f_mode |= FA_READ;
	
	if(flags & O_WRONLY)
		f_mode |= FA_WRITE;
	
	if(flags & O_CREAT)
	{
		if(flags & O_EXCL)
			f_mode |= FA_CREATE_NEW;
		else if(flags & O_TRUNC)
			f_mode |= FA_CREATE_ALWAYS;
		else
			f_mode |= FA_OPEN_ALWAYS;			
	}
	
	FRESULT fr=f_open(f, name, f_mode);
	if(fr!=FR_OK)
		return -1;
	
	if(  (flags & O_WRONLY) && (flags & O_TRUNC) && (!(flags & O_CREAT))  )
	{
		f_truncate(f);
	}
	
	ret=search_set_descriptor(f);
	
	if(ret==-1)
		f_close(f);
	
	return ret;
}

int close(int fd)
{
	if(!(SSOS_valid_fd(fd)))
		return -1;
	
	if(fd > 0 && fd <= 2)
		return 0;

	if(f_close(SSOS_file_stream[fd-3].fip) == FR_OK)
		return 0;
	else return -1;
}

int dummy_return_fileerror(void * buffer, size_t count)
{
	return -1;
}

int read(int fd, void * buffer, size_t count)
{
	//TODO: dummy stdin
	if (fd == 0)
		return 0;

	if (fd<=2)
		return -1;

	uint32_t br;
	if(FR_OK==f_read(get_fil_by_fd(fd),(void *)buffer,count,&br))
		return br;
	return -1;	
}

#define stdout_buffer_size 256

char stdout_buffer[stdout_buffer_size];

static int
write_fillbuffer(const void * buffer, size_t count)
{
	strncpy(stdout_buffer,buffer,stdout_buffer_size-1);
	return count;
}

int write(int fd, const void * buffer, size_t count)
{
	if (fd == 0)
		return -1;
	if (fd == 1)
		return write_stdout(buffer, count);
	if (fd == 2)
		return write_stderr(buffer, count);

	uint32_t bw;
	if(FR_OK==f_write(get_fil_by_fd(fd), (void *)buffer, count, &bw))
		return bw;
	return -1;	
}

int mkdir(const char* name)
{
	if(FR_OK==f_mkdir(name))
		return 0;
	return -1;	
}

int ssos_fflush(FILE *stream)
{
	int fd = stream->fd;
	if(fd>=0 && fd<=2)
		return 0;

	register FIL* f=get_fil_by_fd(fd);
	f_sync(f);

	return 0;
}

long lseek (int fd, long offset, int against)
{
	if(fd>=0 && fd<=2)
		return 0;
	register FIL* f=get_fil_by_fd(fd);
	register unsigned long realpos;
	switch(against){
		case SEEK_SET:
			realpos=offset;
			break;
		case SEEK_CUR:
			realpos=f->fptr + offset;
			break;
		case SEEK_END:
			realpos=f->fsize + offset;
			break;
		default:
			return -1;
	}
	if(f_lseek(f, realpos) != FR_OK)
		return -1;
	
	return (long) realpos;
}

long long SSOS_fd_size(int fd)
{
	if(SSOS_valid_fd(fd))
		return get_fil_by_fd(fd)->fsize;
	return -1;
}

long long SSOS_fp_size(FILE* fp)
{
	return SSOS_fd_size(fp->fd);
}

int isatty(int fd) //dummy function
{
	return ((fd>=0) && (fd<=2));
}

int fileno(FILE* f)
{
	return f->fd;
}
/*
const char *setlocale(int a, const char * b)
{
	(void)a;
	(void)b;
	return "C";
}
*/
FILE *fopen(const char *name, const char *mode)
 {
	int fd;
	FILE *fp;

	if (*mode != 'r' && *mode != 'w' && *mode != 'a')
		 return NULL;

	int flags=0;
	if (mode[0] == 'r')
	{
		 if(mode[1]==0)
			 flags = O_RDONLY;
		 else if(mode[1]=='+')
			 flags = O_RDWR;
	}
	else if (mode[0] == 'w')
	{
		 if(mode[1]==0)
			 flags = O_WRONLY|O_CREAT|O_TRUNC;
		 else if(mode[1]=='+')
			 flags = O_RDWR|O_CREAT|O_TRUNC;
	}
	else return NULL;

	fd = open(name, flags, 0);
	if(fd==-1)
		return NULL;

	fp = malloc(sizeof(fp));
	fp->fd=fd;
	return fp;
 }

int fclose(FILE *f)
{
	if(close(f->fd))
		return EOF;
	return 0;
}

void SSOS_files_init()
{
	memset(SSOS_file_stream, 0, sizeof(struct _SSOS_file_stream)*_FS_LOCK);
	//read_stdin = read_irrc;
	write_stdout = write_screen_out;
	write_stderr = write_screen_err;
}
