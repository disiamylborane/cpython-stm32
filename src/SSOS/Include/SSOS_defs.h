
#ifndef _SSOSDEFS__
#define _SSOSDEFS__

#define MAX_PATH 100

//CASE SENSITIVE!!!!
#define EXECUTABLE_PYSCRIPT ".py"
#define EXECUTABLE_SCRIPT ".sc"
#define EXECUTABLE_ASM ".as"
#define EXECUTABLE_APP ".ap"

#define SSOS_PY_CHECK_EXEC_EXT(dotpos) \
		strcmp(dotpos, EXECUTABLE_PYSCRIPT) == 0 || \
		strcmp(dotpos, EXECUTABLE_SCRIPT) == 0 || \
		strcmp(dotpos, EXECUTABLE_ASM) == 0 || \
		strcmp(dotpos, EXECUTABLE_APP) == 0

// size of read-write data in typeobject. Also the size of type attribute cache (2^x) => 12*2^x is the size of RWdata
//1 is enough for integrate(sin(x**2))
#define PYTHON_TYPEOBJECT_MCACHE_SIZE_EXP 4



#endif  /* _INC_ERRNO */
