#include "Python.h"

#define FSTATAT_DIR_FD_CONVERTER dir_fd_unavailable




PyDoc_STRVAR(os_stat__doc__,
"stat($module, /, path, *, dir_fd=None, follow_symlinks=True)\n"
"--\n"
"\n"
"Perform a stat system call on the given path.\n"
"\n"
"  path\n"
"    Path to be examined; can be string, bytes, or open-file-descriptor int.\n"
"  dir_fd\n"
"    If not None, it should be a file descriptor open to a directory,\n"
"    and path should be a relative string; path will then be relative to\n"
"    that directory.\n"
"  follow_symlinks\n"
"    If False, and the last element of the path is a symbolic link,\n"
"    stat will examine the symbolic link itself instead of the file\n"
"    the link points to.\n"
"\n"
"dir_fd and follow_symlinks may not be implemented\n"
"  on your platform.  If they are unavailable, using them will raise a\n"
"  NotImplementedError.\n"
"\n"
"It\'s an error to use dir_fd or follow_symlinks when specifying path as\n"
"  an open file descriptor.");

#define OS_STAT_METHODDEF    \
    {"stat", (PyCFunction)os_stat, METH_VARARGS|METH_KEYWORDS, os_stat__doc__},

static PyObject *
os_stat_impl(PyModuleDef *module, path_t *path, int dir_fd,
             int follow_symlinks);

static PyObject *
os_stat(PyModuleDef *module, PyObject *args, PyObject *kwargs)
{
    PyObject *return_value = NULL;
    static char *_keywords[] = {"path", "dir_fd", "follow_symlinks", NULL};
    path_t path = PATH_T_INITIALIZE("stat", "path", 0, 1);
    int dir_fd = DEFAULT_DIR_FD;
    int follow_symlinks = 1;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O&|$O&p:stat", _keywords,
        path_converter, &path, FSTATAT_DIR_FD_CONVERTER, &dir_fd, &follow_symlinks))
        goto exit;

    return_value = os_stat_impl(module, &path, dir_fd, follow_symlinks);

exit:
    /* Cleanup for path */
    path_cleanup(&path);

    return return_value;
}


PyDoc_STRVAR(os_chdir__doc__,
"chdir($module, /, path)\n"
"--\n"
"\n"
"Change the current working directory to the specified path.\n"
"\n"
"path may always be specified as a string.\n"
"On some platforms, path may also be specified as an open file descriptor.\n"
"  If this functionality is unavailable, using it raises an exception.");

#define OS_CHDIR_METHODDEF    \
    {"chdir", (PyCFunction)os_chdir, METH_VARARGS|METH_KEYWORDS, os_chdir__doc__},

static PyObject *
os_chdir_impl(PyModuleDef *module, path_t *path);

static PyObject *
os_chdir(PyModuleDef *module, PyObject *args, PyObject *kwargs)
{
    PyObject *return_value = NULL;
    static char *_keywords[] = {"path", NULL};
    path_t path = PATH_T_INITIALIZE("chdir", "path", 0, PATH_HAVE_FCHDIR);

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O&:chdir", _keywords,
        path_converter, &path))
        goto exit;
    return_value = os_chdir_impl(module, &path);

exit:
    /* Cleanup for path */
    path_cleanup(&path);

    return return_value;
}


PyDoc_STRVAR(os_listdir__doc__,
"listdir($module, /, path=None)\n"
"--\n"
"\n"
"Return a list containing the names of the files in the directory.\n"
"\n"
"path can be specified as either str or bytes.  If path is bytes,\n"
"  the filenames returned will also be bytes; in all other circumstances\n"
"  the filenames returned will be str.\n"
"If path is None, uses the path=\'.\'.\n"
"On some platforms, path may also be specified as an open file descriptor;\\\n"
"  the file descriptor must refer to a directory.\n"
"  If this functionality is unavailable, using it raises NotImplementedError.\n"
"\n"
"The list is in arbitrary order.  It does not include the special\n"
"entries \'.\' and \'..\' even if they are present in the directory.");

#define OS_LISTDIR_METHODDEF    \
    {"listdir", (PyCFunction)os_listdir, METH_VARARGS|METH_KEYWORDS, os_listdir__doc__},

static PyObject *
os_listdir_impl(PyModuleDef *module, path_t *path);

static PyObject *
os_listdir(PyModuleDef *module, PyObject *args, PyObject *kwargs)
{
	PyObject *return_value = NULL;
	static char *_keywords[] = { "path", NULL };
	path_t path = PATH_T_INITIALIZE("listdir", "path", 1, PATH_HAVE_FDOPENDIR);

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|O&:listdir", _keywords,
		path_converter, &path))
		goto exit;

    return_value = os_listdir_impl(module, &path);

exit:
    /* Cleanup for path */
    path_cleanup(&path);

    return return_value;
}


PyDoc_STRVAR(os_rename__doc__,
"rename($module, /, src, dst, *, src_dir_fd=None, dst_dir_fd=None)\n"
"--\n"
"\n"
"Rename a file or directory.\n"
"\n"
"If either src_dir_fd or dst_dir_fd is not None, it should be a file\n"
"  descriptor open to a directory, and the respective path string (src or dst)\n"
"  should be relative; the path will then be relative to that directory.\n"
"src_dir_fd and dst_dir_fd, may not be implemented on your platform.\n"
"  If they are unavailable, using them will raise a NotImplementedError.");

#define OS_RENAME_METHODDEF    \
    {"rename", (PyCFunction)os_rename, METH_VARARGS|METH_KEYWORDS, os_rename__doc__},

static PyObject *
os_rename_impl(PyModuleDef *module, path_t *src, path_t *dst, int src_dir_fd,
               int dst_dir_fd);

static PyObject *
os_rename(PyModuleDef *module, PyObject *args, PyObject *kwargs)
{
    PyObject *return_value = NULL;
    static char *_keywords[] = {"src", "dst", "src_dir_fd", "dst_dir_fd", NULL};
    path_t src = PATH_T_INITIALIZE("rename", "src", 0, 0);
    path_t dst = PATH_T_INITIALIZE("rename", "dst", 0, 0);
    int src_dir_fd = DEFAULT_DIR_FD;
    int dst_dir_fd = DEFAULT_DIR_FD;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O&O&|$O&O&:rename", _keywords,
        path_converter, &src, path_converter, &dst, dir_fd_converter, &src_dir_fd, dir_fd_converter, &dst_dir_fd))
        goto exit;
    return_value = os_rename_impl(module, &src, &dst, src_dir_fd, dst_dir_fd);

exit:
    /* Cleanup for src */
    path_cleanup(&src);
    /* Cleanup for dst */
    path_cleanup(&dst);

    return return_value;
}

PyDoc_STRVAR(os_replace__doc__,
"replace($module, /, src, dst, *, src_dir_fd=None, dst_dir_fd=None)\n"
"--\n"
"\n"
"Rename a file or directory, overwriting the destination.\n"
"\n"
"If either src_dir_fd or dst_dir_fd is not None, it should be a file\n"
"  descriptor open to a directory, and the respective path string (src or dst)\n"
"  should be relative; the path will then be relative to that directory.\n"
"src_dir_fd and dst_dir_fd, may not be implemented on your platform.\n"
"  If they are unavailable, using them will raise a NotImplementedError.\"");

#define OS_REPLACE_METHODDEF    \
    {"replace", (PyCFunction)os_replace, METH_VARARGS|METH_KEYWORDS, os_replace__doc__},

static PyObject *
os_replace_impl(PyModuleDef *module, path_t *src, path_t *dst,
                int src_dir_fd, int dst_dir_fd);

static PyObject *
os_replace(PyModuleDef *module, PyObject *args, PyObject *kwargs)
{
    PyObject *return_value = NULL;
    static char *_keywords[] = {"src", "dst", "src_dir_fd", "dst_dir_fd", NULL};
    path_t src = PATH_T_INITIALIZE("replace", "src", 0, 0);
    path_t dst = PATH_T_INITIALIZE("replace", "dst", 0, 0);
    int src_dir_fd = DEFAULT_DIR_FD;
    int dst_dir_fd = DEFAULT_DIR_FD;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O&O&|$O&O&:replace", _keywords,
        path_converter, &src, path_converter, &dst, dir_fd_converter, &src_dir_fd, dir_fd_converter, &dst_dir_fd))
        goto exit;
    return_value = os_replace_impl(module, &src, &dst, src_dir_fd, dst_dir_fd);

exit:
    /* Cleanup for src */
    path_cleanup(&src);
    /* Cleanup for dst */
    path_cleanup(&dst);

    return return_value;
}

PyDoc_STRVAR(os_open__doc__,
"open($module, /, path, flags, mode=511, *, dir_fd=None)\n"
"--\n"
"\n"
"Open a file for low level IO.  Returns a file descriptor (integer).\n"
"\n"
"If dir_fd is not None, it should be a file descriptor open to a directory,\n"
"  and path should be relative; path will then be relative to that directory.\n"
"dir_fd may not be implemented on your platform.\n"
"  If it is unavailable, using it will raise a NotImplementedError.");

#define OS_OPEN_METHODDEF    \
    {"open", (PyCFunction)os_open, METH_VARARGS|METH_KEYWORDS, os_open__doc__},

static int
os_open_impl(PyModuleDef *module, path_t *path, int flags, int mode,
             int dir_fd);

static PyObject *
os_open(PyModuleDef *module, PyObject *args, PyObject *kwargs)
{
    PyObject *return_value = NULL;
    static char *_keywords[] = {"path", "flags", "mode", "dir_fd", NULL};
    path_t path = PATH_T_INITIALIZE("open", "path", 0, 0);
    int flags;
    int mode = 511;
    int dir_fd = DEFAULT_DIR_FD;
    int _return_value;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O&i|i$O&:open", _keywords,
        path_converter, &path, &flags, &mode, OPENAT_DIR_FD_CONVERTER, &dir_fd))
        goto exit;
    _return_value = os_open_impl(module, &path, flags, mode, dir_fd);
    if ((_return_value == -1) && PyErr_Occurred())
        goto exit;
    return_value = PyLong_FromLong((long)_return_value);

exit:
    /* Cleanup for path */
    path_cleanup(&path);

    return return_value;
}


PyDoc_STRVAR(os_close__doc__,
"close($module, /, fd)\n"
"--\n"
"\n"
"Close a file descriptor.");

#define OS_CLOSE_METHODDEF    \
    {"close", (PyCFunction)os_close, METH_VARARGS|METH_KEYWORDS, os_close__doc__},

static PyObject *
os_close_impl(PyModuleDef *module, int fd);

static PyObject *
os_close(PyModuleDef *module, PyObject *args, PyObject *kwargs)
{
    PyObject *return_value = NULL;
    static char *_keywords[] = {"fd", NULL};
    int fd;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "i:close", _keywords,
        &fd))
        goto exit;
    return_value = os_close_impl(module, fd);

exit:
    return return_value;
}


PyDoc_STRVAR(os_mkdir__doc__,
"mkdir($module, /, path, mode=511, *, dir_fd=None)\n"
"--\n"
"\n"
"Create a directory.\n"
"\n"
"If dir_fd is not None, it should be a file descriptor open to a directory,\n"
"  and path should be relative; path will then be relative to that directory.\n"
"dir_fd may not be implemented on your platform.\n"
"  If it is unavailable, using it will raise a NotImplementedError.\n"
"\n"
"The mode argument is ignored on Windows.");

#define OS_MKDIR_METHODDEF    \
    {"mkdir", (PyCFunction)os_mkdir, METH_VARARGS|METH_KEYWORDS, os_mkdir__doc__},

static PyObject *
os_mkdir_impl(PyModuleDef *module, path_t *path, int mode, int dir_fd);

static PyObject *
os_mkdir(PyModuleDef *module, PyObject *args, PyObject *kwargs)
{
    PyObject *return_value = NULL;
    static char *_keywords[] = {"path", "mode", "dir_fd", NULL};
    path_t path = PATH_T_INITIALIZE("mkdir", "path", 0, 0);
    int mode = 511;
    int dir_fd = DEFAULT_DIR_FD;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O&|i$O&:mkdir", _keywords,
        path_converter, &path, &mode, MKDIRAT_DIR_FD_CONVERTER, &dir_fd))
        goto exit;
    return_value = os_mkdir_impl(module, &path, mode, dir_fd);

exit:
    /* Cleanup for path */
    path_cleanup(&path);

    return return_value;
}
PyDoc_STRVAR(os_read__doc__,
"read($module, fd, length, /)\n"
"--\n"
"\n"
"Read from a file descriptor.  Returns a bytes object.");

#define OS_READ_METHODDEF    \
    {"read", (PyCFunction)os_read, METH_VARARGS, os_read__doc__},

static PyObject *
os_read_impl(PyModuleDef *module, int fd, Py_ssize_t length);

static PyObject *
os_read(PyModuleDef *module, PyObject *args)
{
    PyObject *return_value = NULL;
    int fd;
    Py_ssize_t length;

    if (!PyArg_ParseTuple(args, "in:read",
        &fd, &length))
        goto exit;
    return_value = os_read_impl(module, fd, length);

exit:
    return return_value;
}



PyDoc_STRVAR(os_write__doc__,
"write($module, fd, data, /)\n"
"--\n"
"\n"
"Write a bytes object to a file descriptor.");

#define OS_WRITE_METHODDEF    \
    {"write", (PyCFunction)os_write, METH_VARARGS, os_write__doc__},

static Py_ssize_t
os_write_impl(PyModuleDef *module, int fd, Py_buffer *data);

static PyObject *
os_write(PyModuleDef *module, PyObject *args)
{
    PyObject *return_value = NULL;
    int fd;
    Py_buffer data = {NULL, NULL};
    Py_ssize_t _return_value;

    if (!PyArg_ParseTuple(args, "iy*:write",
        &fd, &data))
        goto exit;
    _return_value = os_write_impl(module, fd, &data);
    if ((_return_value == -1) && PyErr_Occurred())
        goto exit;
    return_value = PyLong_FromSsize_t(_return_value);

exit:
    /* Cleanup for data */
    if (data.obj)
       PyBuffer_Release(&data);

    return return_value;
}
