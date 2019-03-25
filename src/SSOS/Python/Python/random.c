#include "Python.h"
void
_PyRandom_Init(void)
{
}
void
_PyRandom_Fini(void)
{
}
static unsigned char RandomNumbers[] =
{
	7, 167, 203, 54,
	32, 78, 164, 112,
	237, 182, 75, 96,
	135, 13, 42, 27
};
#define SSOS_URANDOM_SIZE 16
static unsigned char CurrUrandPos = 0;
int
_PyOS_URandom(void *buffer, Py_ssize_t size)
{
    if (size < 0) {
        PyErr_Format(PyExc_ValueError,
                     "negative argument not allowed");
        return -1;
    }
    if (size == 0)
        return 0;

	for (size_t i = 0; i < size; i++)
	{
		((char*)buffer)[i] = RandomNumbers[CurrUrandPos];
		CurrUrandPos = (CurrUrandPos+1)%SSOS_URANDOM_SIZE;
	}
	return 0;
}

