/* Return the copyright string.  This is updated manually. */

#include "Python.h"

static const char cprt[] =
"\
(c)";

const char *
Py_GetCopyright(void)
{
    return cprt;
}
