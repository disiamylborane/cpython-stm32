

typedef struct __FILE {
	int fd;
} __FILE;

extern __FILE __stdin__;
extern __FILE __stdout__;
extern __FILE __stderr__;

extern __FILE* stdin;
extern __FILE* stdout;
extern __FILE* stderr;
