
#ifndef _SSOSFUN_H_
#define _SSOSFUN_H_

#include "SSOS_defs.h"
#include "stdio.h"
#include "SSOS_assert.h"
#include <stdint.h>

int open(const char *name, int flags, int perms);
int close(int fd);
int read(int fd, void * buffer, size_t count);
int write(int fd, const void * buffer, size_t count);
long lseek (int fd, long offset, int against);
int fileno(FILE*);
int isatty(int fd); //dummy function
long long SSOS_fd_size(int fd);
long long SSOS_fp_size(FILE* fp);

void malloc_init(void* memory_start, size_t memory_size);
void fnctl_init(void);

int SSOS_Init(void);
int SSOS_RunPython(void);
int SSOS_StopPython(void);

uint32_t SSOS_GetClockFrequency(void);

size_t SSOS_malloc_get_free_mem();

#endif  /* _INC_ERRNO */
