
#ifndef _SSOSPRIVATE_H_
#define _SSOSPRIVATE_H_

#include "SSOS_defs.h"
#include "SSOS_assert.h"


//#define
#include "stdio.h"
#include <stddef.h>

//int (*read_stdin) (void * buffer, size_t count);
int (*write_stdout) (const void * buffer, size_t count);
int (*write_stderr) (const void * buffer, size_t count);

int read_irrc(void * buf, size_t count);
int write_screen_out(const void * buffer, size_t count);
int write_screen_err(const void * buffer, size_t count);

extern unsigned int process_current;

void SSOS_files_init();


#endif  /* _SSOSPRIVATE_H_ */
