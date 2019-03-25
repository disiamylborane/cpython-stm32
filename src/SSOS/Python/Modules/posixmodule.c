
#define PY_SSIZE_T_CLEAN

#include "Python.h"
#include "structmember.h"
#include "SSOS_defs.h"
#include "SSOS_errno.h"
#include "ff.h"
#include "fcntl.h"



#ifdef __cplusplus
extern "C" {
#endif

PyDoc_STRVAR(posix__doc__,
"This module provides access to operating system functionality that is\n\
standardized by the C Standard and the POSIX standard (a thinly\n\
disguised Unix interface).  Refer to the library manual and\n\
corresponding Unix manual entries for more information on calls.");

#ifdef HAVE_LONG_LONG
#  define _PyLong_FromDev PyLong_FromLongLong
#else
#  define _PyLong_FromDev PyLong_FromLong
#endif

#ifdef AT_FDCWD
/*
 * Why the (int) cast?  Solaris 10 defines AT_FDCWD as 0xffd19553 (-3041965);
 * without the int cast, the value gets interpreted as uint (4291925331),
 * which doesn't play nicely with all the initializer lines in this file that
 * look like this:
 *      int dir_fd = DEFAULT_DIR_FD;
 */
#define DEFAULT_DIR_FD (int)AT_FDCWD
#else
#define DEFAULT_DIR_FD (-100)
#endif


static int
_fd_converter(PyObject *o, int *p, const char *allowed)
{
    int overflow;
    long long_value;

    PyObject *index = PyNumber_Index(o);
    if (index == NULL) {
        PyErr_Format(PyExc_TypeError,
                     "argument should be %s, not %.200s",
                     allowed, Py_TYPE(o)->tp_name);
        return 0;
    }

    long_value = PyLong_AsLongAndOverflow(index, &overflow);
    Py_DECREF(index);
    if (overflow > 0 || long_value > INT_MAX) {
        PyErr_SetString(PyExc_OverflowError,
                        "fd is greater than maximum");
        return 0;
    }
    if (overflow < 0 || long_value < INT_MIN) {
        PyErr_SetString(PyExc_OverflowError,
                        "fd is less than minimum");
        return 0;
    }

    *p = (int)long_value;
    return 1;
}

static int
dir_fd_converter(PyObject *o, void *p)
{
    if (o == Py_None) {
        *(int *)p = DEFAULT_DIR_FD;
        return 1;
    }
    return _fd_converter(o, (int *)p, "integer");
}
/*
 * A PyArg_ParseTuple "converter" function
 * that handles filesystem paths in the manner
 * preferred by the os module.
 *
 * path_converter accepts (Unicode) strings and their
 * subclasses, and bytes and their subclasses.  What
 * it does with the argument depends on the platform:
 *
 *   * On Windows, if we get a (Unicode) string we
 *     extract the wchar_t * and return it; if we get
 *     bytes we extract the char * and return that.
 *
 *   * On all other platforms, strings are encoded
 *     to bytes using PyUnicode_FSConverter, then we
 *     extract the char * from the bytes object and
 *     return that.
 *
 * path_converter also optionally accepts signed
 * integers (representing open file descriptors) instead
 * of path strings.
 *
 * Input fields:
 *   path.nullable
 *     If nonzero, the path is permitted to be None.
 *   path.allow_fd
 *     If nonzero, the path is permitted to be a file handle
 *     (a signed int) instead of a string.
 *   path.function_name
 *     If non-NULL, path_converter will use that as the name
 *     of the function in error messages.
 *     (If path.function_name is NULL it omits the function name.)
 *   path.argument_name
 *     If non-NULL, path_converter will use that as the name
 *     of the parameter in error messages.
 *     (If path.argument_name is NULL it uses "path".)
 *
 * Output fields:
 *   path.wide
 *     Removed. Only narrow can be used
 *   path.narrow
 *     Points to the path if it was expressed as bytes,
 *     or it was Unicode and was encoded to bytes.
 *   path.fd
 *     Contains a file descriptor if path.accept_fd was true
 *     and the caller provided a signed integer instead of any
 *     sort of string.
 *
 *     WARNING: if your "path" parameter is optional, and is
 *     unspecified, path_converter will never get called.
 *     So if you set allow_fd, you *MUST* initialize path.fd = -1
 *     yourself!
 *   path.length
 *     The length of the path in characters, if specified as
 *     a string.
 *   path.object
 *     The original object passed in.
 *   path.cleanup
 *     For internal use only.  May point to a temporary object.
 *     (Pay no attention to the man behind the curtain.)
 *
 *   At most one of path.wide or path.narrow will be non-NULL.
 *   If path was None and path.nullable was set,
 *     or if path was an integer and path.allow_fd was set,
 *     both path.wide and path.narrow will be NULL
 *     and path.length will be 0.
 *
 *   path_converter takes care to not write to the path_t
 *   unless it's successful.  However it must reset the
 *   "cleanup" field each time it's called.
 *
 * Use as follows:
 *      path_t path;
 *      memset(&path, 0, sizeof(path));
 *      PyArg_ParseTuple(args, "O&", path_converter, &path);
 *      // ... use values from path ...
 *      path_cleanup(&path);
 *
 * (Note that if PyArg_Parse fails you don't need to call
 * path_cleanup().  However it is safe to do so.)
 */
typedef struct {
    const char *function_name;
    const char *argument_name;
    int nullable;
    int allow_fd;
    char *narrow;
    //FIL *fp;
    Py_ssize_t length;
    PyObject *object;
    PyObject *cleanup;
} path_t;

#define PATH_T_INITIALIZE(function_name, argument_name, nullable, allow_fd) \
    {function_name, argument_name, nullable, allow_fd, NULL, 0, NULL, NULL}

static void
path_cleanup(path_t *path) {
    if (path->cleanup) {
        Py_CLEAR(path->cleanup);
    }
}

static int
path_converter(PyObject *o, void *p) {
    path_t *path = (path_t *)p;
    PyObject *unicode, *bytes;
    Py_ssize_t length;
    char *narrow;

#define FORMAT_EXCEPTION(exc, fmt) \
    PyErr_Format(exc, "%s%s" fmt, \
        path->function_name ? path->function_name : "", \
        path->function_name ? ": "                : "", \
        path->argument_name ? path->argument_name : "path")

    /* Py_CLEANUP_SUPPORTED support */
    if (o == NULL) {
        path_cleanup(path);
        return 1;
    }

    /* ensure it's always safe to call path_cleanup() */
    path->cleanup = NULL;

    if (o == Py_None) {
        if (!path->nullable) {
            FORMAT_EXCEPTION(PyExc_TypeError,
                             "can't specify None for %s argument");
            return 0;
        }
        path->narrow = NULL;
        path->length = 0;
        path->object = o;
        return 1;
    }

		unicode = PyUnicode_FromObject(o);
		if (unicode)
		{
			int converted = PyUnicode_FSConverter(unicode, &bytes);
			Py_DECREF(unicode);
			if (!converted)
				bytes = NULL;
		}
    else {
        PyErr_Clear();
        if (PyObject_CheckBuffer(o))
            bytes = PyBytes_FromObject(o);
        else
            bytes = NULL;
    }
    if (!bytes) {
        if (!PyErr_Occurred())
            FORMAT_EXCEPTION(PyExc_TypeError, "illegal type for %s parameter");
        return 0;
    }
		
    length = PyBytes_GET_SIZE(bytes);
			if (length > MAX_PATH-1) {
					FORMAT_EXCEPTION(PyExc_ValueError, "%s too long for Windows");
					Py_DECREF(bytes);
					return 0;
			}
    narrow = PyBytes_AS_STRING(bytes);
			if ((size_t)length != strlen(narrow)) {
					FORMAT_EXCEPTION(PyExc_ValueError, "embedded null character in %s");
					Py_DECREF(bytes);
					return 0;
			}

		path->narrow = narrow;
    path->length = length;
    path->object = o;
		path->cleanup = bytes;
    return Py_CLEANUP_SUPPORTED;
}

static void
argument_unavailable_error(char *function_name, char *argument_name) {
    PyErr_Format(PyExc_NotImplementedError,
        "%s%s%s unavailable on this platform",
        (function_name != NULL) ? function_name : "",
        (function_name != NULL) ? ": ": "",
        argument_name);
}

static int
dir_fd_unavailable(PyObject *o, void *p)
{
    int dir_fd;
    if (!dir_fd_converter(o, &dir_fd))
        return 0;
    if (dir_fd != DEFAULT_DIR_FD) {
        argument_unavailable_error(NULL, "dir_fd");
        return 0;
    }
    *(int *)p = dir_fd;
    return 1;
}









static int initialized;
static PyTypeObject StatResultType;
static PyTypeObject StatVFSResultType;
static newfunc structseq_new;

static PyObject *
statresult_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyStructSequence *result;
    int i;

    result = (PyStructSequence*)structseq_new(type, args, kwds);
    if (!result)
        return NULL;
    /* If we have been initialized from a tuple,
       st_?time might be set to None. Initialize it
       from the int slots.  */
    for (i = 7; i <= 9; i++) {
        if (result->ob_item[i+3] == Py_None) {
            Py_DECREF(Py_None);
            Py_INCREF(result->ob_item[i]);
            result->ob_item[i+3] = result->ob_item[i];
        }
    }
    return (PyObject*)result;
}

static PyObject *
_PyLong_FromTime_t(time_t t)
{
#if defined(HAVE_LONG_LONG) && SIZEOF_TIME_T == SIZEOF_LONG_LONG
	return PyLong_FromLongLong((PY_LONG_LONG)t);
#else
	assert(sizeof(time_t) <= sizeof(long));
	return PyLong_FromLong((long)t);
#endif
}

static PyObject *billion = NULL;
static int _stat_float_times = 1;

static void
fill_time(PyObject *v, int index, time_t sec, unsigned long nsec)
{
    PyObject *s = _PyLong_FromTime_t(sec);
    PyObject *ns_fractional = PyLong_FromUnsignedLong(nsec);
    PyObject *s_in_ns = NULL;
    PyObject *ns_total = NULL;
    PyObject *float_s = NULL;

    if (!(s && ns_fractional))
        goto exit;

    s_in_ns = PyNumber_Multiply(s, billion);
    if (!s_in_ns)
        goto exit;

    ns_total = PyNumber_Add(s_in_ns, ns_fractional);
    if (!ns_total)
        goto exit;

    if (_stat_float_times) {
        float_s = PyFloat_FromDouble(sec + 1e-9*nsec);
        if (!float_s)
            goto exit;
    }
    else {
        float_s = s;
        Py_INCREF(float_s);
    }

    PyStructSequence_SET_ITEM(v, index, s);
    PyStructSequence_SET_ITEM(v, index+3, float_s);
    PyStructSequence_SET_ITEM(v, index+6, ns_total);
    s = NULL;
    float_s = NULL;
    ns_total = NULL;
exit:
    Py_XDECREF(s);
    Py_XDECREF(ns_fractional);
    Py_XDECREF(s_in_ns);
    Py_XDECREF(ns_total);
    Py_XDECREF(float_s);
}


/* pack a system stat C structure into the Python stat tuple
   (used by posix_stat() and posix_fstat()) */
static PyObject*
_pystat_fromstructstat(struct _Py_stat_struct *st)
{
    unsigned long ansec, mnsec, cnsec;
    PyObject *v = PyStructSequence_New(&StatResultType);
    if (v == NULL)
        return NULL;

    PyStructSequence_SET_ITEM(v, 0, PyLong_FromLong((long)st->st_mode));
#ifdef HAVE_LARGEFILE_SUPPORT
    PyStructSequence_SET_ITEM(v, 1,
                              PyLong_FromLongLong((PY_LONG_LONG)st->st_ino));
#else
    PyStructSequence_SET_ITEM(v, 1, PyLong_FromLong((long)st->st_ino));
#endif
#ifdef MS_WINDOWS
    PyStructSequence_SET_ITEM(v, 2, PyLong_FromUnsignedLong(st->st_dev));
#else
    PyStructSequence_SET_ITEM(v, 2, _PyLong_FromDev(st->st_dev));
#endif
    PyStructSequence_SET_ITEM(v, 3, PyLong_FromLong((long)st->st_nlink));
#if defined(MS_WINDOWS) || defined(PY_SSOS)
    PyStructSequence_SET_ITEM(v, 4, PyLong_FromLong(0));
    PyStructSequence_SET_ITEM(v, 5, PyLong_FromLong(0));
#else
    PyStructSequence_SET_ITEM(v, 4, _PyLong_FromUid(st->st_uid));
    PyStructSequence_SET_ITEM(v, 5, _PyLong_FromGid(st->st_gid));
#endif
#ifdef HAVE_LARGEFILE_SUPPORT
    PyStructSequence_SET_ITEM(v, 6,
                              PyLong_FromLongLong((PY_LONG_LONG)st->st_size));
#else
    PyStructSequence_SET_ITEM(v, 6, PyLong_FromLong(st->st_size));
#endif

#if defined(HAVE_STAT_TV_NSEC)
    ansec = st->st_atim.tv_nsec;
    mnsec = st->st_mtim.tv_nsec;
    cnsec = st->st_ctim.tv_nsec;
#elif defined(HAVE_STAT_TV_NSEC2)
    ansec = st->st_atimespec.tv_nsec;
    mnsec = st->st_mtimespec.tv_nsec;
    cnsec = st->st_ctimespec.tv_nsec;
#elif defined(HAVE_STAT_NSEC)
    ansec = st->st_atime_nsec;
    mnsec = st->st_mtime_nsec;
    cnsec = st->st_ctime_nsec;
#else
    ansec = mnsec = cnsec = 0;
#endif
    fill_time(v, 7, st->st_atime, ansec);
    fill_time(v, 8, st->st_mtime, mnsec);
    fill_time(v, 9, st->st_ctime, cnsec);

#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
    PyStructSequence_SET_ITEM(v, ST_BLKSIZE_IDX,
                              PyLong_FromLong((long)st->st_blksize));
#endif
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
    PyStructSequence_SET_ITEM(v, ST_BLOCKS_IDX,
                              PyLong_FromLong((long)st->st_blocks));
#endif
#ifdef HAVE_STRUCT_STAT_ST_RDEV
    PyStructSequence_SET_ITEM(v, ST_RDEV_IDX,
                              PyLong_FromLong((long)st->st_rdev));
#endif
#ifdef HAVE_STRUCT_STAT_ST_GEN
    PyStructSequence_SET_ITEM(v, ST_GEN_IDX,
                              PyLong_FromLong((long)st->st_gen));
#endif
#ifdef HAVE_STRUCT_STAT_ST_BIRTHTIME
    {
      PyObject *val;
      unsigned long bsec,bnsec;
      bsec = (long)st->st_birthtime;
#ifdef HAVE_STAT_TV_NSEC2
      bnsec = st->st_birthtimespec.tv_nsec;
#else
      bnsec = 0;
#endif
      if (_stat_float_times) {
        val = PyFloat_FromDouble(bsec + 1e-9*bnsec);
      } else {
        val = PyLong_FromLong((long)bsec);
      }
      PyStructSequence_SET_ITEM(v, ST_BIRTHTIME_IDX,
                                val);
    }
#endif
#ifdef HAVE_STRUCT_STAT_ST_FLAGS
    PyStructSequence_SET_ITEM(v, ST_FLAGS_IDX,
                              PyLong_FromLong((long)st->st_flags));
#endif
#ifdef HAVE_STRUCT_STAT_ST_FILE_ATTRIBUTES
    PyStructSequence_SET_ITEM(v, ST_FILE_ATTRIBUTES_IDX,
                              PyLong_FromUnsignedLong(st->st_file_attributes));
#endif

    if (PyErr_Occurred()) {
        Py_DECREF(v);
        return NULL;
    }

    return v;
}


static PyObject *
path_error(path_t *path)
{
    return PyErr_SetFromErrnoWithFilenameObject(PyExc_OSError, path->object);
}

static PyObject *
path_error2(path_t *path, path_t *path2)
{
    return PyErr_SetFromErrnoWithFilenameObjects(PyExc_OSError,
        path->object, path2->object);
}


static PyObject *
posix_do_stat(char *function_name, path_t *path,
int dir_fd, int follow_symlinks)
{
	struct _Py_stat_struct st;
	int result;
//	if (path_and_dir_fd_invalid("stat", path, dir_fd) ||
//		dir_fd_and_fd_invalid("stat", dir_fd, path->fd) ||
//		fd_and_follow_symlinks_invalid("stat", path->fd, follow_symlinks))
//		return NULL;
	
	FILINFO fno;
	fno.lfsize = 260;
	fno.lfname = (char*)malloc(260);
	
	if(f_stat(path->narrow, &fno) != FR_OK)
		return path_error(path);
	memset(&st, 0, sizeof st);
	st.st_size=fno.fsize;
	
	if (fno.fattrib & AM_DIR)
		st.st_mode |= _S_IFDIR | 0111; // IFEXEC for user,group,other 
	else
		st.st_mode |= _S_IFREG;
	if (fno.fattrib & AM_RDO)
		st.st_mode |= 0444;
	else
		st.st_mode |= 0666;
	
	char *dot = strrchr(path->narrow, '.');
	if (dot)
		if (SSOS_PY_CHECK_EXEC_EXT(dot))
			st.st_mode |= 0111;

	free(fno.lfname);
	return _pystat_fromstructstat(&st);
};

	
	
	
static PyObject *
os_stat_impl(PyModuleDef *module, path_t *path, int dir_fd,
             int follow_symlinks)
{
    return posix_do_stat("stat", path, dir_fd, follow_symlinks);
}






static PyObject *
posix_error(void)
{
    return PyErr_SetFromErrno(PyExc_OSError);
}




static PyObject *
os_chdir_impl(PyModuleDef *module, path_t *path)
/*[clinic end generated code: output=7358e3a20fb5aa93 input=1a4a15b4d12cb15d]*/
{
    FRESULT result;
    result = f_chdir(path->narrow);
    if (result != FR_OK) {
        return path_error(path);
    }

    Py_RETURN_NONE;
}


static PyObject *
_listdir_SSOS(path_t *path, PyObject *list)
{
	PyObject *v;
	FRESULT res;
	DIR dir;
	
	FILINFO fno;
	fno.lfsize = 260;
	fno.lfname = (char*)malloc(260);
	
	if(f_opendir(&dir, path->narrow?path->narrow:".") != FR_OK)
		return NULL;

	if ((list = PyList_New(0)) == NULL)
    goto exit;

	for (;;) 
	{
		res = f_readdir(&dir, &fno);
		if(fno.fname[0] == 0)
				break;
		if(res!=FR_OK)
		{
			Py_DECREF(list);
			list = path_error(path);
			goto exit;
    }
		char* tmp = *fno.lfname ? fno.lfname : fno.fname;
		v = PyUnicode_DecodeFSDefaultAndSize(tmp, strlen(tmp));
		if (v == NULL) 
		{
			Py_CLEAR(list);
			break;
    }
		if (PyList_Append(list, v) != 0) 
		{
			Py_DECREF(v);
			Py_CLEAR(list);
			break;
    }
    Py_DECREF(v);
	}
	
exit:
	
	free(fno.lfname);
	return list;	
}


static PyObject *
os_listdir_impl(PyModuleDef *module, path_t *path)
{
    return _listdir_SSOS(path, NULL);
}

static PyObject *
internal_rename(path_t *src, path_t *dst, int src_dir_fd, int dst_dir_fd, int is_replace)
/*[clinic end generated code: output=131d012eed8d3b8b input=25515dfb107c8421]*/
{
	uint32_t do_remove=0;
	FRESULT fr;
	FILINFO fno;
	
	if ((src_dir_fd != DEFAULT_DIR_FD) || (dst_dir_fd != DEFAULT_DIR_FD)) 
	{
			argument_unavailable_error(is_replace?"replace":"rename", "src_dir_fd and dst_dir_fd");
			return NULL;
	}
		
	fr = f_stat(dst->narrow, &fno);
	if(fr == FR_OK)
	{
		if(is_replace)
			do_remove=1;
		else
			path_error2(src, dst);
	}	
	fr = f_stat(src->narrow, &fno);
	if(fr != FR_OK)
	{
			path_error(src);
	}
		
	if(do_remove)
		if(f_unlink(dst->narrow) != FR_OK)
			return path_error(dst);
	
	fr = f_rename(src->narrow, dst->narrow);
	if(fr != FR_OK)
		path_error2(src, dst);
	
  Py_RETURN_NONE;
}


/*[clinic input]
os.rename

    src : path_t
    dst : path_t
    *
    src_dir_fd : dir_fd = None
    dst_dir_fd : dir_fd = None

Rename a file or directory.

If either src_dir_fd or dst_dir_fd is not None, it should be a file
  descriptor open to a directory, and the respective path string (src or dst)
  should be relative; the path will then be relative to that directory.
src_dir_fd and dst_dir_fd, may not be implemented on your platform.
  If they are unavailable, using them will raise a NotImplementedError.
[clinic start generated code]*/

static PyObject *
os_rename_impl(PyModuleDef *module, path_t *src, path_t *dst, int src_dir_fd,
               int dst_dir_fd)
/*[clinic end generated code: output=08033bb2ec27fb5f input=faa61c847912c850]*/
{
    return internal_rename(src, dst, src_dir_fd, dst_dir_fd, 0);
}

/*[clinic input]
os.replace = os.rename

Rename a file or directory, overwriting the destination.

If either src_dir_fd or dst_dir_fd is not None, it should be a file
  descriptor open to a directory, and the respective path string (src or dst)
  should be relative; the path will then be relative to that directory.
src_dir_fd and dst_dir_fd, may not be implemented on your platform.
  If they are unavailable, using them will raise a NotImplementedError."
[clinic start generated code]*/

static PyObject *
os_replace_impl(PyModuleDef *module, path_t *src, path_t *dst,
                int src_dir_fd, int dst_dir_fd)
/*[clinic end generated code: output=131d012eed8d3b8b input=25515dfb107c8421]*/
{
    return internal_rename(src, dst, src_dir_fd, dst_dir_fd, 1);
}

static int
os_open_impl(PyModuleDef *module, path_t *path, int flags, int mode,
             int dir_fd)
/*[clinic end generated code: output=47e8cc63559f5ddd input=ad8623b29acd2934]*/
{
    int fd;
    int async_err = 0;

    _Py_BEGIN_SUPPRESS_IPH
    do {
        Py_BEGIN_ALLOW_THREADS
            fd = open(path->narrow, flags, mode);
        Py_END_ALLOW_THREADS
    } while (fd < 0 && errno == EINTR && !(async_err = PyErr_CheckSignals()));
    _Py_END_SUPPRESS_IPH

    if (fd == -1) {
        if (!async_err)
            PyErr_SetFromErrnoWithFilenameObject(PyExc_OSError, path->object);
        return -1;
    }

    return fd;
}


static PyObject *
os_close_impl(PyModuleDef *module, int fd)
/*[clinic end generated code: output=47bf2ea536445a26 input=2bc42451ca5c3223]*/
{
    int res;
//    if (!_PyVerify_fd(fd))
//        return posix_error();
    /* We do not want to retry upon EINTR: see http://lwn.net/Articles/576478/
     * and http://linux.derkeiler.com/Mailing-Lists/Kernel/2005-09/3000.html
     * for more details.
     */
//    Py_BEGIN_ALLOW_THREADS
//			_Py_BEGIN_SUPPRESS_IPH
//				res = close(fd);
//			_Py_END_SUPPRESS_IPH
//    Py_END_ALLOW_THREADS
    if (close(fd) < 0)
        return posix_error();
    Py_RETURN_NONE;
}


static PyObject *
os_mkdir_impl(PyModuleDef *module, path_t *path, int mode, int dir_fd)
/*[clinic end generated code: output=8bf1f738873ef2c5 input=e965f68377e9b1ce]*/
{
    int result;

    Py_BEGIN_ALLOW_THREADS
        result = f_mkdir(path->narrow);
    Py_END_ALLOW_THREADS
    if (result != FR_OK)
        return path_error(path);
    Py_RETURN_NONE;
}


static PyObject *
os_read_impl(PyModuleDef *module, int fd, Py_ssize_t length)
/*[clinic end generated code: output=be24f44178455e8b input=1df2eaa27c0bf1d3]*/
{
    Py_ssize_t n;
    PyObject *buffer;

    if (length < 0) {
        errno = EINVAL;
        return posix_error();
    }

#ifdef MS_WINDOWS
    /* On Windows, the count parameter of read() is an int */
    if (length > INT_MAX)
        length = INT_MAX;
#endif

    buffer = PyBytes_FromStringAndSize((char *)NULL, length);
    if (buffer == NULL)
        return NULL;

    n = _Py_read(fd, PyBytes_AS_STRING(buffer), length);
    if (n == -1) {
        Py_DECREF(buffer);
        return NULL;
    }

    if (n != length)
        _PyBytes_Resize(&buffer, n);

    return buffer;
}
/*[clinic input]
os.write -> Py_ssize_t

    fd: int
    data: Py_buffer
    /

Write a bytes object to a file descriptor.
[clinic start generated code]*/

static Py_ssize_t
os_write_impl(PyModuleDef *module, int fd, Py_buffer *data)
/*[clinic end generated code: output=58845c93c9ee1dda input=3207e28963234f3c]*/
{
    return _Py_write(fd, data->buf, data->len);
}


#ifdef HAVE_FACCESSAT
    #define FACCESSAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define FACCESSAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_FCHMODAT
    #define FCHMODAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define FCHMODAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_FCHOWNAT
    #define FCHOWNAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define FCHOWNAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_FSTATAT
    #define FSTATAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define FSTATAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_LINKAT
    #define LINKAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define LINKAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_MKDIRAT
    #define MKDIRAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define MKDIRAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_MKFIFOAT
    #define MKFIFOAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define MKFIFOAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_MKNODAT
    #define MKNODAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define MKNODAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_OPENAT
    #define OPENAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define OPENAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_READLINKAT
    #define READLINKAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define READLINKAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_SYMLINKAT
    #define SYMLINKAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define SYMLINKAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_UNLINKAT
    #define UNLINKAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define UNLINKAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_FCHDIR
    #define PATH_HAVE_FCHDIR 1
#else
    #define PATH_HAVE_FCHDIR 0
#endif

#ifdef HAVE_FCHMOD
    #define PATH_HAVE_FCHMOD 1
#else
    #define PATH_HAVE_FCHMOD 0
#endif

#ifdef HAVE_FCHOWN
    #define PATH_HAVE_FCHOWN 1
#else
    #define PATH_HAVE_FCHOWN 0
#endif

#ifdef HAVE_FDOPENDIR
    #define PATH_HAVE_FDOPENDIR 1
#else
    #define PATH_HAVE_FDOPENDIR 0
#endif

#ifdef HAVE_FEXECVE
    #define PATH_HAVE_FEXECVE 1
#else
    #define PATH_HAVE_FEXECVE 0
#endif

#ifdef HAVE_FPATHCONF
    #define PATH_HAVE_FPATHCONF 1
#else
    #define PATH_HAVE_FPATHCONF 0
#endif

#ifdef HAVE_FSTATVFS
    #define PATH_HAVE_FSTATVFS 1
#else
    #define PATH_HAVE_FSTATVFS 0
#endif

#ifdef HAVE_FTRUNCATE
    #define PATH_HAVE_FTRUNCATE 1
#else
    #define PATH_HAVE_FTRUNCATE 0
#endif
#include "clinic/posixmodule.c.h"

static PyMethodDef posix_methods[] = {
	OS_STAT_METHODDEF
	OS_CHDIR_METHODDEF
	OS_LISTDIR_METHODDEF
	OS_MKDIR_METHODDEF
	//OS_RENAME_METHODDEF
	OS_REPLACE_METHODDEF
	//OS_RMDIR_METHODDEF
	//OS_UNLINK_METHODDEF
	//OS_REMOVE_METHODDEF
	//OS_EXECV_METHODDEF
	//OS_EXECVE_METHODDEF
	OS_OPEN_METHODDEF
	OS_CLOSE_METHODDEF
	//OS_LSEEK_METHODDEF
	OS_READ_METHODDEF
	OS_WRITE_METHODDEF
	//OS_FSTAT_METHODDEF
	//OS_URANDOM_METHODDEF//for sympy
	{ NULL, NULL }            /* Sentinel */
};





#define INITFUNC PyInit_nt
#define MODNAME "nt"


PyDoc_STRVAR(stat_result__doc__,
"stat_result: Result from stat, fstat, or lstat.\n\n\
This object may be accessed either as a tuple of\n\
  (mode, ino, dev, nlink, uid, gid, size, atime, mtime, ctime)\n\
or via the attributes st_mode, st_ino, st_dev, st_nlink, st_uid, and so on.\n\
\n\
Posix/windows: If your platform supports st_blksize, st_blocks, st_rdev,\n\
or st_flags, they are available as attributes only.\n\
\n\
See os.stat for more information.");

static PyStructSequence_Field stat_result_fields[] = {
    {"st_mode",    "protection bits"},
    {"st_ino",     "inode"},
    {"st_dev",     "device"},
    {"st_nlink",   "number of hard links"},
    {"st_uid",     "user ID of owner"},
    {"st_gid",     "group ID of owner"},
    {"st_size",    "total size, in bytes"},
    /* The NULL is replaced with PyStructSequence_UnnamedField later. */
    {NULL,   "integer time of last access"},
    {NULL,   "integer time of last modification"},
    {NULL,   "integer time of last change"},
    {"st_atime",   "time of last access"},
    {"st_mtime",   "time of last modification"},
    {"st_ctime",   "time of last change"},
    {"st_atime_ns",   "time of last access in nanoseconds"},
    {"st_mtime_ns",   "time of last modification in nanoseconds"},
    {"st_ctime_ns",   "time of last change in nanoseconds"},
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
    {"st_blksize", "blocksize for filesystem I/O"},
#endif
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
    {"st_blocks",  "number of blocks allocated"},
#endif
#ifdef HAVE_STRUCT_STAT_ST_RDEV
    {"st_rdev",    "device type (if inode device)"},
#endif
#ifdef HAVE_STRUCT_STAT_ST_FLAGS
    {"st_flags",   "user defined flags for file"},
#endif
#ifdef HAVE_STRUCT_STAT_ST_GEN
    {"st_gen",    "generation number"},
#endif
#ifdef HAVE_STRUCT_STAT_ST_BIRTHTIME
    {"st_birthtime",   "time of creation"},
#endif
#ifdef HAVE_STRUCT_STAT_ST_FILE_ATTRIBUTES
    {"st_file_attributes", "Windows file attribute bits"},
#endif
    {0}
};
static PyStructSequence_Desc stat_result_desc = {
    "stat_result", /* name */
    stat_result__doc__, /* doc */
    stat_result_fields,
    10
};

PyDoc_STRVAR(statvfs_result__doc__,
"statvfs_result: Result from statvfs or fstatvfs.\n\n\
This object may be accessed either as a tuple of\n\
  (bsize, frsize, blocks, bfree, bavail, files, ffree, favail, flag, namemax),\n\
or via the attributes f_bsize, f_frsize, f_blocks, f_bfree, and so on.\n\
\n\
See os.statvfs for more information.");

static PyStructSequence_Field statvfs_result_fields[] = {
    {"f_bsize",  },
    {"f_frsize", },
    {"f_blocks", },
    {"f_bfree",  },
    {"f_bavail", },
    {"f_files",  },
    {"f_ffree",  },
    {"f_favail", },
    {"f_flag",   },
    {"f_namemax",},
    {0}
};

static PyStructSequence_Desc statvfs_result_desc = {
    "statvfs_result", /* name */
    statvfs_result__doc__, /* doc */
    statvfs_result_fields,
    10
};


static PyTypeObject TerminalSizeType;

PyDoc_STRVAR(TerminalSize_docstring,
    "A tuple of (columns, lines) for holding terminal window size");

static PyStructSequence_Field TerminalSize_fields[] = {
    {"columns", "width of the terminal window in characters"},
    {"lines", "height of the terminal window in characters"},
    {NULL, NULL}
};

static PyStructSequence_Desc TerminalSize_desc = {
    "os.terminal_size",
    TerminalSize_docstring,
    TerminalSize_fields,
    2,
};


static struct PyModuleDef posixmodule = {
    PyModuleDef_HEAD_INIT,
    MODNAME,
    posix__doc__,
    -1,
    posix_methods,
    NULL,
    NULL,
    NULL,
    NULL
};


PyMODINIT_FUNC
INITFUNC(void)
{
	PyObject *m;//, *v;
    PyObject *list;
    //char **trace;

//#if defined(HAVE_SYMLINK) && defined(MS_WINDOWS)
//    win32_can_symlink = enable_symlink();
//#endif

    m = PyModule_Create(&posixmodule);
    if (m == NULL)
        return NULL;

    /* Initialize environ dictionary */
    //v = convertenviron();
	PyModule_AddObject(m, "environ", PyDict_New());
    //Py_XINCREF(v);
    //if (v == NULL || PyModule_AddObject(m, "environ", v) != 0)
    //    return NULL;
    //Py_DECREF(v);

//#ifdef F_OK
//	if (PyModule_AddIntMacro(m, F_OK)) return NULL;
//#endif
//#ifdef R_OK
//	if (PyModule_AddIntMacro(m, R_OK)) return NULL;
//#endif
//#ifdef W_OK
//	if (PyModule_AddIntMacro(m, W_OK)) return NULL;
//#endif
//#ifdef X_OK
//	if (PyModule_AddIntMacro(m, X_OK)) return NULL;
//#endif
#ifdef O_RDONLY
	if (PyModule_AddIntMacro(m, O_RDONLY)) return NULL;
#endif
#ifdef O_WRONLY
	if (PyModule_AddIntMacro(m, O_WRONLY)) return NULL;
#endif
#ifdef O_RDWR
	if (PyModule_AddIntMacro(m, O_RDWR)) return NULL;
#endif
#ifdef O_CREAT
	if (PyModule_AddIntMacro(m, O_CREAT)) return NULL;
#endif
#ifdef O_EXCL
	if (PyModule_AddIntMacro(m, O_EXCL)) return NULL;
#endif
#ifdef O_TRUNC
	if (PyModule_AddIntMacro(m, O_TRUNC)) return NULL;
#endif
//#ifdef O_BINARY
//	if (PyModule_AddIntMacro(m, O_BINARY)) return NULL;
//#endif
//#ifdef O_TEXT
//	if (PyModule_AddIntMacro(m, O_TEXT)) return NULL;
//#endif

    //if (all_ins(m))
    //    return NULL;

    //if (setup_confname_tables(m))
    //    return NULL;

    Py_INCREF(PyExc_OSError);
    PyModule_AddObject(m, "error", PyExc_OSError);

//#ifdef HAVE_PUTENV
//    if (posix_putenv_garbage == NULL)
//        posix_putenv_garbage = PyDict_New();
//#endif

    if (!initialized) {
        stat_result_desc.name = "os.stat_result"; /* see issue #19209 */
        stat_result_desc.fields[7].name = PyStructSequence_UnnamedField;
        stat_result_desc.fields[8].name = PyStructSequence_UnnamedField;
        stat_result_desc.fields[9].name = PyStructSequence_UnnamedField;
        if (PyStructSequence_InitType2(&StatResultType, &stat_result_desc) < 0)
            return NULL;
        structseq_new = StatResultType.tp_new;
        StatResultType.tp_new = statresult_new;

        statvfs_result_desc.name = "os.statvfs_result"; /* see issue #19209 */
        if (PyStructSequence_InitType2(&StatVFSResultType,
                                       &statvfs_result_desc) < 0)
            return NULL;

        /* initialize TerminalSize_info */
        if (PyStructSequence_InitType2(&TerminalSizeType,
                                       &TerminalSize_desc) < 0)
            return NULL;

        /* initialize scandir types */
//        if (PyType_Ready(&ScandirIteratorType) < 0)
//            return NULL;
//        if (PyType_Ready(&DirEntryType) < 0)
//            return NULL;
    }
    //Py_INCREF((PyObject*) &StatResultType);
    //PyModule_AddObject(m, "stat_result", (PyObject*) &StatResultType);
    //Py_INCREF((PyObject*) &StatVFSResultType);
    //PyModule_AddObject(m, "statvfs_result",
    //                   (PyObject*) &StatVFSResultType);
	
    //times_result_desc.name = MODNAME ".times_result";
    //if (PyStructSequence_InitType2(&TimesResultType, &times_result_desc) < 0)
    //    return NULL;
    //PyModule_AddObject(m, "times_result", (PyObject *)&TimesResultType);

    //uname_result_desc.name = MODNAME ".uname_result";
    //if (PyStructSequence_InitType2(&UnameResultType, &uname_result_desc) < 0)
    //    return NULL;
    //PyModule_AddObject(m, "uname_result", (PyObject *)&UnameResultType);

    //Py_INCREF(&TerminalSizeType);
    //PyModule_AddObject(m, "terminal_size", (PyObject*) &TerminalSizeType);

    billion = PyLong_FromLong(1000000000);
    if (!billion)
        return NULL;

    /* suppress "function not used" warnings */
    {
    int ignored;
    //fd_specified("", -1);
    //follow_symlinks_specified("", 1);
    //dir_fd_and_follow_symlinks_invalid("chmod", DEFAULT_DIR_FD, 1);
    dir_fd_converter(Py_None, &ignored);
    dir_fd_unavailable(Py_None, &ignored);
    }

    /*
     * provide list of locally available functions
     * so os.py can populate support_* lists
     */
    list = PyList_New(0);
    if (!list)
        return NULL;
    //for (trace = have_functions; *trace; trace++) {
    //    PyObject *unicode = PyUnicode_DecodeASCII(*trace, strlen(*trace), NULL);
    //    if (!unicode)
    //        return NULL;
    //    if (PyList_Append(list, unicode))
    //        return NULL;
    //    Py_DECREF(unicode);
    //}
    PyModule_AddObject(m, "_have_functions", list);

    initialized = 1;

    return m;
}



#ifdef __cplusplus
}
#endif



