

#include "Python.h"
#include "ff.h"
#include "SSOS_fun.h"
#include "fcntl.h"
#include "string.h"
//#include "SSOS_private.h"

FATFS USBDISKFatFs;
char U_P[4];

unsigned int process_current;

/*
Initialized: USB, FAT, SDRAM
*/
int SSOS_Init()
{
	process_current=0;
	
	init_memory_manager();
	SSOS_files_init();
	
	if(f_mount(&USBDISKFatFs, (TCHAR const*)U_P, 0) != FR_OK)
		return 0;
	
	return 1;
}

int SSOS_RunPython(void)
{
	Py_Initialize();
	process_current=1;
}

int SSOS_StopPython(void)
{
	Py_Finalize();
	process_current=0;
	//free_all_processid(1);
}

//TODO: implement this function
uint32_t SSOS_GetClockFrequency()
{
	return 168000000;
}

size_t wcslen(const wchar_t* s)
{
	register size_t len = 0;
	for (; s[len]; len++);
	return len;
}

int wcscmp(const wchar_t* s1, const wchar_t* s2)
{
	register size_t i = 0;
	do
	{
		if (s1[i]>s2[i])
			return 1;
		else if (s1[i]<s2[i])
			return -1;
		i++;
	} while (s1[i-1]);
	return 0;
}

wchar_t* wcschr(const wchar_t *S, wchar_t C)
{
	for (; *S != 0; S++)
	{
		if(*S == C)
			return (wchar_t*)S;
	}
	return 0;
}

size_t wcstombs(char *out, const wchar_t *in, size_t size)
{
	if (in == NULL || out == NULL) return -1;
	register size_t i = 0;
	for (;	(i<size) && (in[i]);  i++)
	{
		if((unsigned)(in[i])<255)
			out[i] = in[i];
		else
			return -1;
	}
	return i;
}

size_t mbstowcs(wchar_t *out, const char *in, size_t size)
{
	if (in == NULL || out == NULL) return -1;
	register size_t i = 0;
	for (;	(i<size) && (in[i]);  out[i] = in[i], i++);
	return i;
}

wchar_t *Py_GetPath()
{
	return L"Lib";
}

void exit(int exitcode)
{
	for(;;);
}

void abort()
{
	for(;;);
}

