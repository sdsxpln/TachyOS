/*
 * tch_port.h
 *
 *  Created on: 2014. 6. 13.
 *      Author: innocentevil
 */

#ifndef TCH_PORT_H_
#define TCH_PORT_H_



/****
 *  define exception entry / exit stack data structure
 */
typedef struct _tch_exc_stack_t tch_exc_stack;
typedef struct _tch_thread_context_t tch_thread_context;



typedef struct _tch_port_ix tch_port_ix;


/***
 *  port interface
 */
struct _tch_port_ix {
	void (*_kernel_lock)(void);
	void (*_kernel_unlock)(void);
	void (*_switchContext)(void* nth,void* cth) __attribute__((always_naked));
	void (*_saveContext)(void* cth) __attribute__((always_naked));
	void (*_restoreContext)(void* cth) __attribute__((always_naked));
	void (*_enterSvFromUsr)(int sv_id,void* arg1,void* arg2);
	void (*_enterSvFromIsr)(int sv_id,void* arg1,void* arg2);
};

const tch_port_ix* PORT_OBJ;








#endif /* TCH_PORT_H_ */
