
#include <stdint.h>
#include "Python.h"

int _mculib__checkaddress(Py_ssize_t address)
{
	if (address == NULL) {
		PyErr_SetString(PyExc_ValueError, "NULL is currently unavailable :)");
		return NULL;
	}
	return 1;
}
int _mculib__parseargs(unsigned int *val, Py_ssize_t *address, PyObject* args)
{
	if (!PyArg_ParseTuple(args, "In", val, address)) {
		return NULL;
	}
	if (!_mculib__checkaddress(*address))
		return NULL;
	return 1;
}

PyObject *_mculib_write8(PyObject* self, PyObject* args) {
	unsigned int val;
	Py_ssize_t address;
	if (!_mculib__parseargs(&val,&address,args))
		return NULL;

	*(uint8_t*)address = (uint8_t)val;
	Py_RETURN_NONE;
}
PyObject *_mculib_write16(PyObject* self, PyObject* args) {
	unsigned int val;
	Py_ssize_t address;
	if (!_mculib__parseargs(&val, &address, args))
		return NULL;
	if (address & 1) {
		PyErr_SetString(PyExc_ValueError, "Address is not 16-aligned");
		return NULL;
	}
	*(uint16_t*)address = (uint16_t)val;
	Py_RETURN_NONE;
}
PyObject *_mculib_write32(PyObject* self, PyObject* args) {
	unsigned int val;
	Py_ssize_t address;
	if (!_mculib__parseargs(&val, &address, args))
		return NULL;
	if (address & 3) {
		PyErr_SetString(PyExc_ValueError, "Address is not 32-aligned");
		return NULL;
	}
	*(uint32_t*)address = (uint32_t)val;
	Py_RETURN_NONE;
}

PyObject *_mculib_read32(PyObject* self, PyObject* arg) {
	Py_ssize_t address = PyLong_AsLong(arg);
	if (!_mculib__checkaddress(address))
		return NULL;
	
	PyObject *i = PyLong_FromUnsignedLong(*(unsigned long*)address);
	return i;
}
PyObject *_mculib_read16(PyObject* self, PyObject* arg) {
	Py_ssize_t address = PyLong_AsLong(arg);
	if (!_mculib__checkaddress(address))
		return NULL;

	PyObject *i = PyLong_FromUnsignedLong(*(unsigned long*)address);
	return i;
}
PyObject *_mculib_read8(PyObject* self, PyObject* arg) {
	Py_ssize_t address = PyLong_AsLong(arg);
	if (!_mculib__checkaddress(address))
		return NULL;

	PyObject *i = PyLong_FromUnsignedLong(*(unsigned long*)address);
	return i;
}

PyObject *_mculib_malloc(PyObject* self, PyObject* arg) {
	Py_ssize_t length = PyLong_AsLong(arg);

	void *i = malloc(length);
	return PyLong_FromLong((long)i);
}
PyObject *_mculib_free(PyObject* self, PyObject* arg) {
	void *address = (void *)PyLong_AsLong(arg);

	free(address);
	Py_RETURN_NONE;
}

static PyMethodDef _mculib_methods[] = {
	{ "write8",         _mculib_write8,   METH_VARARGS,         NULL },
	{ "write16",        _mculib_write16,  METH_VARARGS,         NULL },
	{ "write32",        _mculib_write32,  METH_VARARGS,         NULL },
	{ "read32",         _mculib_read32,   METH_O,         NULL },
	{ "read16",         _mculib_read16,   METH_O,         NULL },
	{ "read8",          _mculib_read8,    METH_O,         NULL },
	{ "malloc",         _mculib_malloc,   METH_O,         NULL },
	{ "free",           _mculib_free,     METH_O,         NULL },
	{ NULL,              NULL }           /* sentinel */
};

static struct PyModuleDef _mculibmodule = {
	PyModuleDef_HEAD_INIT,
	"_mculib",
	NULL,
	-1,
	_mculib_methods,
	NULL,
	NULL,
	NULL,
	NULL
};


PyMODINIT_FUNC
PyInit__mculib(void)
{
	PyObject *m;

	m = PyModule_Create(&_mculibmodule);
	if (m == NULL)
		return NULL;

	return m;
}
