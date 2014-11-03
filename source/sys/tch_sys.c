/*
 * tch_sys.c
 *
 *  Copyright (C) 2014 doowoong,lee
 *  All rights reserved.
 *
 *  This software may be modified and distributed under the terms
 *  of the LGPL v3 license.  See the LICENSE file for details.
 *
 *
 *  Created on: 2014. 6. 13.
 *      Author: innocentevil
 *
 *
 *
 *      this module contains most essential part of tch kernel.
 *
 */


#include "tch_kernel.h"
#include "tch_sys.h"
#include "tch_mem.h"
#include "tch_halcfg.h"
#include "tch_nclib.h"
#include "tch_port.h"
#include "tch_async.h"




#define TCH_STHREAD_STK_SIZE   ((uint32_t) 1 << 11)


static tch_kernel_instance tch_sys_instance;
const tch_kernel_instance* Sys = (const tch_kernel_instance*)&tch_sys_instance;



static DECLARE_THREADSTACK(sysThreadStk,TCH_STHREAD_STK_SIZE);
static DECLARE_THREADROUTINE(sysTaskHandler);
//static tch_lnode_t sysTaskQue;
static tch_threadId sysThreadId;
static tch_thread_queue sysThreadPort;
static tch_mailqId sysTaskQue;

/***
 *  Initialize Kernel including...
 *  - initailize device driver and bind HAL interface
 *  - initialize architecture dependent part and bind port interface
 *  - bind User APIs to API type
 *  - initialize Idle thread
 */
void tch_kernelInit(void* arg){

	/**
	 *  dynamic binding of dependecy
	 */
	tch_sys_instance.tch_api.Device = tch_kernel_initHAL();
	if(!tch_sys_instance.tch_api.Device)
		tch_kernel_errorHandler(FALSE,osErrorValue);


	if(!tch_kernel_initPort()){
		tch_kernel_errorHandler(FALSE,osErrorOS);
	}


	tch_port_kernel_lock();



	// put thread in wait queue
	tch_listPutFirst((tch_lnode_t*)&sysThreadPort,(tch_lnode_t*) &((tch_thread_header*)sysThreadId)->t_waitNode);

	/*Bind API Object*/
	tch* api = (tch*) &tch_sys_instance;
	api->uStdLib = tch_initCrt(NULL);
	api->Thread = Thread;
	api->Mtx = Mtx;
	api->Sem = Sem;
	api->Condv = Condv;
	api->Barrier = Barrier;
	api->Sig = Sig;
	api->Mempool = Mempool;
	api->MailQ = MailQ;
	api->MsgQ = MsgQ;
	api->Mem = Mem;
//	api->Async = Async;


	// create system task thread
	sysTaskQue = MailQ->create(sizeof(tch_sysTask), TCH_SYSTASK_QSIZE);

	tch_threadCfg sThcfg;
	sThcfg._t_name = "Sys";
	sThcfg._t_routine = sysTaskHandler;
	sThcfg._t_stack = sysThreadStk;
	sThcfg.t_proior = KThread;
	sThcfg.t_stackSize = TCH_STHREAD_STK_SIZE;
	sysThreadId = Thread->create(&sThcfg,sysTaskQue);


	tch_port_enableISR();                   // interrupt enable
	tch_schedInit(&tch_sys_instance);
	return;
}



void tch_kernelSvCall(uint32_t sv_id,uint32_t arg1, uint32_t arg2){
	tch_thread_header* cth = NULL;
	tch_thread_header* nth = NULL;
	tch_exc_stack* sp = NULL;
	switch(sv_id){
	case SV_EXIT_FROM_SV:
		sp = (tch_exc_stack*)tch_port_getThreadSP();
		sp++;
		tch_port_setThreadSP((uint32_t)sp);
		tch_port_kernel_unlock();
		break;
	case SV_THREAD_START:              // start thread first time
		tch_schedStartThread((tch_threadId) arg1);
		break;
	case SV_THREAD_SLEEP:
		tch_schedSleep(arg1,SLEEP);    // put current thread in pending queue and will be waken up at given after given time duration is passed
		break;
	case SV_THREAD_JOIN:
		if(((tch_thread_header*)arg1)->t_state != TERMINATED){                                 // check target if thread has terminated
			tch_schedSuspend((tch_thread_queue*)&((tch_thread_header*)arg1)->t_joinQ,arg2);    //if not, thread wait
			break;
		}
		tch_kernelSetResult(tch_currentThread,osOK);                                           //..otherwise, it returns immediately
		break;
	case SV_THREAD_RESUME:
		tch_schedResumeM((tch_thread_queue*) arg1,1,arg2,TRUE);
		break;
	case SV_THREAD_RESUMEALL:
		tch_schedResumeM((tch_thread_queue*) arg1,SCHED_THREAD_ALL,arg2,TRUE);
		break;
	case SV_THREAD_SUSPEND:
		tch_schedSuspend((tch_thread_queue*)arg1,arg2);
		break;
	case SV_THREAD_TERMINATE:
		cth = (tch_thread_header*) arg1;
		tch_schedTerminate((tch_threadId) cth,arg2);
		break;
	case SV_SIG_WAIT:
		cth = (tch_thread_header*) tch_schedGetRunningThread();
		cth->t_sig.match_target = arg1;                         ///< update thread signal pattern
		tch_schedSuspend((tch_thread_queue*)&cth->t_sig.sig_wq,arg2);///< suspend to signal wq
		break;
	case SV_SIG_MATCH:
		cth = (tch_thread_header*) arg1;
		tch_schedResumeM((tch_thread_queue*)&cth->t_sig.sig_wq,SCHED_THREAD_ALL,osOK,TRUE);
		break;
	case SV_MSGQ_PUT:
		cth = tch_currentThread;
		tch_kernelSetResult(cth,tch_msgq_kput((tch_msgqId) arg1,(void*) arg2));
		break;
	case SV_MSGQ_GET:
		cth = tch_currentThread;
		tch_kernelSetResult(cth,tch_msgq_kget((tch_msgqId) arg1,(void*) arg2));
		break;
	case SV_MSGQ_DESTROY:
		cth = tch_currentThread;
		tch_kernelSetResult(cth,tch_msgq_kdestroy((tch_msgqId) arg1));
		break;
	case SV_MAILQ_ALLOC:
		cth = tch_currentThread;
		tch_kernelSetResult(cth,tch_mailq_kalloc((tch_mailqId) arg1,(void*) arg2));
		break;
	case SV_MAILQ_FREE:
		cth = tch_currentThread;
		tch_kernelSetResult(cth,tch_mailq_kfree((tch_mailqId) arg1,(void*) arg2));
		break;
	case SV_MAILQ_DESTROY:
		cth = tch_currentThread;
		tch_kernelSetResult(cth,tch_mailq_kdestroy((tch_mailqId) arg1,0));
		break;
	case SV_ASYNC_WAIT:
		cth = tch_currentThread;
//		tch_kernelSetResult(cth,tch_async_kwait(arg1,arg2,&sysTaskQue));
		break;
	case SV_ASYNC_NOTIFY:
		cth = tch_currentThread;
//		tch_kernelSetResult(cth,tch_async_knotify(arg1,arg2));
		break;
	case SV_ASYNC_DESTROY:
		cth = tch_currentThread;
//		tch_kernelSetResult(cth,tch_async_kdestroy(arg1));
		break;
	case SV_UNIX_SBRK:
		cth = tch_currentThread;
		tch_kernelSetResult(cth,(tchStatus)tch_sbrk_k((void*)arg1,arg2));
		if(cth->t_kRet == (uint32_t)NULL)
			tch_schedTerminate(cth,osErrorNoMemory);
		break;
	}
}



static DECLARE_THREADROUTINE(sysTaskHandler){

	tch_mailqId taskq = (tch_mailqId) env->Thread->getArg();
	osEvent evt;
	env->uStdLib->string->memset(&evt,0,sizeof(osEvent));
	tch_sysTask* tsk = NULL;

	while(TRUE){
		evt = env->MailQ->get(taskq,osWaitForever);
		if(evt.value.p){
			if(evt.status == osEventMail){
				tsk = evt.value.p;
				tsk->tsk_result = tsk->tsk_fn(tsk->tsk_id,tsk->tsk_arg);
				if(tsk->tsk_result != osOK){
					// print error msg
				}
				env->MailQ->free(evt.value.p);
			}
		}
	}

	/*
	while(TRUE){
		while(!tch_listIsEmpty(&sysTaskQue)){    //  thread perform task until task queue is empty
			struct tch_sys_task_t* task = (struct tch_sys_task_t*) tch_listDequeue(&sysTaskQue);
			task->tsk_result = task->tsk_fn(task->tsk_id,task->tsk_arg);
		}
		if(tch_port_enterSvFromUsr(SV_THREAD_SUSPEND,(uint32_t)&sysThreadPort,osWaitForever) != osOK)   // if there's no more task, thread will suspended
			return osErrorOS;
	}
	*/
	return osOK;
}



void tch_kernel_errorHandler(BOOL dump,tchStatus status){

	/**
	 *  optional dump register
	 */
	while(1){
		asm volatile("NOP");
	}
}



void tch_kernel_faulthandle(int fault){
	switch(fault){
	case FAULT_TYPE_BUS:
		break;
	case FAULT_TYPE_HARD:
		break;
	case FAULT_TYPE_MEM:
		break;
	case FAULT_TYPE_USG:
		break;
	}
	while(1){
		asm volatile("NOP");
	}
}


