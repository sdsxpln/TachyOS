/*
 * tch_err.c
 *
 *  Created on: 2015. 7. 4.
 *      Author: innocentevil
 */

#include "tch_kernel.h"
#include "tch_err.h"


void tch_kernel_raise_error(tch_threadId who,int errno,const char* msg){			//managable error which is cuased by thread level erroneous behavior

}

void tch_kernel_panic(const char* floc,int lno, const char* msg){
	while(TRUE){
	}
}

void tch_kernelOnHardFault(int fault,int type){
	while(TRUE){
	}
}
