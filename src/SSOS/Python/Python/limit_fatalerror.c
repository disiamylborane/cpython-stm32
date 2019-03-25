
#include"limit.h"

void
Py_FatalError(const char *msg)
{
	printf_out("Fatal Py error: %s\n", msg);
    abort();
}
