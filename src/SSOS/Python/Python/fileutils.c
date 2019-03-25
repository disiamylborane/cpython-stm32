
#include "Python.h"
#include "ff.h"
#include "SSOS_errno.h"
#include "SSOS_fun.h"
#include "wchar.h"

PyObject *
_Py_device_encoding(int fd)
{
    Py_RETURN_NONE;
}

static char*
encode_ascii_surrogateescape(const wchar_t *text, size_t *error_pos)
{
	char *result = NULL, *out;
	size_t len, i;
	wchar_t ch;

	if (error_pos != NULL)
		*error_pos = (size_t)-1;

	len = wcslen(text);

	result = PyMem_Malloc(len + 1);  /* +1 for NUL byte */
	if (result == NULL)
		return NULL;

	out = result;
	for (i = 0; i<len; i++) {
		ch = text[i];

		if (ch <= 0x7f) {
			/* ASCII character */
			*out++ = (char)ch;
		}
		else if (0xdc80 <= ch && ch <= 0xdcff) {
			/* UTF-8b surrogate */
			*out++ = (char)(ch - 0xdc00);
		}
		else {
			if (error_pos != NULL)
				*error_pos = i;
			PyMem_Free(result);
			return NULL;
		}
	}
	*out = '\0';
	return result;
}

static wchar_t*
decode_ascii_surrogateescape(const char *arg, size_t *size)
{
    wchar_t *res;
    unsigned char *in;
    wchar_t *out;
    size_t argsize = strlen(arg) + 1;

    if (argsize > PY_SSIZE_T_MAX/sizeof(wchar_t))
        return NULL;
    res = PyMem_RawMalloc(argsize*sizeof(wchar_t));
    if (!res)
        return NULL;

    in = (unsigned char*)arg;
    out = res;
    while(*in)
        if(*in < 128)
            *out++ = *in++;
        else
            *out++ = 0xdc00 + *in++;
    *out = 0;
    if (size != NULL)
        *size = out - res;
    return res;
}


/* Decode a byte string from the locale encoding with the
   surrogateescape error handler: undecodable bytes are decoded as characters
   in range U+DC80..U+DCFF. If a byte sequence can be decoded as a surrogate
   character, escape the bytes using the surrogateescape error handler instead
   of decoding them.

   Return a pointer to a newly allocated wide character string, use
   PyMem_RawFree() to free the memory. If size is not NULL, write the number of
   wide characters excluding the null character into *size

   Return NULL on decoding error or memory allocation error. If *size* is not
   NULL, *size is set to (size_t)-1 on memory error or set to (size_t)-2 on
   decoding error.

   Decoding errors should never happen, unless there is a bug in the C
   library.

   Use the Py_EncodeLocale() function to encode the character string back to a
   byte string. */
wchar_t*
Py_DecodeLocale(const char* arg, size_t *size)
{
    wchar_t *res;
	
	res = decode_ascii_surrogateescape(arg, size);
    if (res != NULL)
		return res;

    if (size != NULL)
        *size = (size_t)-1;
    return NULL;
}

/* Encode a wide character string to the locale encoding with the
   surrogateescape error handler: surrogate characters in the range
   U+DC80..U+DCFF are converted to bytes 0x80..0xFF.

   Return a pointer to a newly allocated byte string, use PyMem_Free() to free
   the memory. Return NULL on encoding or memory allocation error.

   If error_pos is not NULL, *error_pos is set to the index of the invalid
   character on encoding error, or set to (size_t)-1 otherwise.

   Use the Py_DecodeLocale() function to decode the bytes string back to a wide
   character string. */
char*
Py_EncodeLocale(const wchar_t *text, size_t *error_pos)
{
	return encode_ascii_surrogateescape(text, error_pos);
}

//@SVD
//_Py_fstat_noraise is removed
//For fileio: use FIL->size instead
FILE*
_Py_fopen(const char *pathname, const char *mode)
{
    return fopen(pathname, mode);
}

FILE*
_Py_fopen_obj(PyObject *path, const char *mode)
{
    FILE *f;
    int async_err = 0;
    PyObject *bytes;
    char *path_bytes;

    if (!PyUnicode_FSConverter(path, &bytes))
        return NULL;
    path_bytes = PyBytes_AS_STRING(bytes);

    do {
        Py_BEGIN_ALLOW_THREADS
        f = fopen(path_bytes, mode);
        Py_END_ALLOW_THREADS
    } while (f == NULL
             && errno == EINTR && !(async_err = PyErr_CheckSignals()));

    Py_DECREF(bytes);
    if (async_err)
        return NULL;

    if (f == NULL) {
        PyErr_SetFromErrnoWithFilenameObject(PyExc_OSError, path);
        return NULL;
    }
    return f;
}

/* Read count bytes from fd into buf.

   On success, return the number of read bytes, it can be lower than count.
   If the current file offset is at or past the end of file, no bytes are read,
   and read() returns zero.

   On error, raise an exception, set errno and return -1.

   When interrupted by a signal (read() fails with EINTR), retry the syscall.
   If the Python signal handler raises an exception, the function returns -1
   (the syscall is not retried).

   Release the GIL to call read(). The caller must hold the GIL. */
Py_ssize_t
_Py_read(int fd, void *buf, size_t count)
{
    Py_ssize_t n;
    int err;
    int async_err = 0;

    /* _Py_read() must not be called with an exception set, otherwise the
     * caller may think that read() was interrupted by a signal and the signal
     * handler raised an exception. */
    assert(!PyErr_Occurred());

    if (!_PyVerify_fd(fd)) {
        /* save/restore errno because PyErr_SetFromErrno() can modify it */
        err = errno;
        PyErr_SetFromErrno(PyExc_OSError);
        errno = err;
        return -1;
    }

    if (count > INT_MAX) {
        /* On SSOS, rot_ne_razevaj, INT_MAX only, not SSIZE_T_MAX*/
        count = INT_MAX;
    }

    _Py_BEGIN_SUPPRESS_IPH
    do {
        Py_BEGIN_ALLOW_THREADS
        errno = 0;
//#ifdef MS_WINDOWS
//        n = read(fd, buf, (int)count);
//#else
        n = read(fd, buf, count);//SSOS: type(count) = size_t
//#endif
        /* save/restore errno because PyErr_CheckSignals()
         * and PyErr_SetFromErrno() can modify it */
        err = errno;
        Py_END_ALLOW_THREADS
    } while (n < 0 && err == EINTR &&
            !(async_err = PyErr_CheckSignals()));
    _Py_END_SUPPRESS_IPH

    if (async_err) {
        /* read() was interrupted by a signal (failed with EINTR)
         * and the Python signal handler raised an exception */
        errno = err;
        assert(errno == EINTR && PyErr_Occurred());
        return -1;
    }
    if (n < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        errno = err;
        return -1;
    }

    return n;
}


static Py_ssize_t
_Py_write_impl(int fd, const void *buf, size_t count, int gil_held)
{
    Py_ssize_t n;
    int err;
    int async_err = 0;

    if (!_PyVerify_fd(fd)) {
        if (gil_held) {
            /* save/restore errno because PyErr_SetFromErrno() can modify it */
            err = errno;
            PyErr_SetFromErrno(PyExc_OSError);
            errno = err;
        }
        return -1;
    }

    _Py_BEGIN_SUPPRESS_IPH
//#ifdef MS_WINDOWS
		//Windows is more narrow here.
    if (count > 32767 && isatty(fd)) {
        /* Issue #11395: the Windows console returns an error (12: not
           enough space error) on writing into stdout if stdout mode is
           binary and the length is greater than 66,000 bytes (or less,
           depending on heap usage). */
        count = 32767;
    }
    else if (count > INT_MAX)
        count = INT_MAX;
//#else
//    if (count > PY_SSIZE_T_MAX) {
//        /* write() should truncate count to PY_SSIZE_T_MAX, but it's safer
//         * to do it ourself to have a portable behaviour. */
//        count = PY_SSIZE_T_MAX;
//    }
//#endif

    if (gil_held) {
        do {
            Py_BEGIN_ALLOW_THREADS
            errno = 0;
//#ifdef MS_WINDOWS
//            n = write(fd, buf, (int)count);
//#else
            n = write(fd, buf, count);
//#endif
            /* save/restore errno because PyErr_CheckSignals()
             * and PyErr_SetFromErrno() can modify it */
            err = errno;
            Py_END_ALLOW_THREADS
        } while (n < 0 && err == EINTR &&
                !(async_err = PyErr_CheckSignals()));
    }
    else {
        do {
            errno = 0;
//#ifdef MS_WINDOWS
//            n = write(fd, buf, (int)count);
//#else
            n = write(fd, buf, count);
//#endif
            err = errno;
        } while (n < 0 && err == EINTR);
    }
    _Py_END_SUPPRESS_IPH

    if (async_err) {
        /* write() was interrupted by a signal (failed with EINTR)
           and the Python signal handler raised an exception (if gil_held is
           nonzero). */
        errno = err;
        assert(errno == EINTR && (!gil_held || PyErr_Occurred()));
        return -1;
    }
    if (n < 0) {
        if (gil_held)
            PyErr_SetFromErrno(PyExc_OSError);
        errno = err;
        return -1;
    }

    return n;
}

Py_ssize_t
_Py_write(int fd, const void *buf, size_t count)
{
    assert(!PyErr_Occurred());
    return _Py_write_impl(fd, buf, count, 1);
}

Py_ssize_t
_Py_write_noraise(int fd, const void *buf, size_t count)
{
    return _Py_write_impl(fd, buf, count, 0);
}


