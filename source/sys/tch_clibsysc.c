/*
 * tch_clibsysc.c
 *
 *  Created on: 2014. 7. 23.
 *      Author: innocentevil
 *
 */


/*  Implementation of Basic Set of  Unix System Call Stub (for Using C Standard library)  */
/*
 *
 */

#include "tch_kernel.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <errno.h>

#undef errno
extern int errno;

char* __env[1] = {0};
char** environ = __env;



char* _sbrk_r(struct _reent* reent,size_t incr){
	if(tch_port_isISR()){
		return tch_port_enterSvFromIsr(SV_UNIX_SBRK,reent,incr);
	}else{
		return tch_port_enterSvFromUsr(SV_UNIX_SBRK,reent,incr);
	}
}

long _write_r(void* reent,int fd,const void* buf,size_t cnt){
	switch(fd){
	case STDIN_FILENO:
		return cnt;
	case STDERR_FILENO:
		return cnt;
	case STDOUT_FILENO:
		return cnt;
	}
	return cnt;
}

int _close_r(void *reent, int fd){
	return -1;
}

off_t _lseek_r(void *reent,int fd, off_t pos, int whence){
	return 0;
}

long _read_r(void *reent,int fd, void *buf, size_t cnt){
	return cnt;
}

int _fork_r(void *reent){
	return 0;
}

int _wait_r(void *reent, int *status){
	return 0;
}

int _stat_r(void *reent,const char *file, void* pstat){
	return 0;
}

int _fstat_r(void *reent,int fd, void* pstat){
	*((uint32_t*)reent) = EINVAL;
	return 0;
}

int _link_r(void *reent,const char *old, const char *new){
	errno = EMLINK;
	return -1;
}

int _unlink_r(void *reent, const char *file){
	return -1;
}

int _isatty_r(int file){
	return 1;
}

void exit(int code){
	while(1);
}

void abort(void){
	while(1);
}


int _open_r(void *reent,const char *file, int flags, int mode){
	return -1;
}

int _getpid(void){
	return -1;
}


time_t _times(void* _reent){
	return (time_t) 0;
}

int _gettimeofday(void* _reent){
	return -1;
}
