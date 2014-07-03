/*
 * tch_vtimer.c
 *
 *  Created on: 2014. 6. 30.
 *      Author: manics99
 */


#include "kernel/tch_kernel.h"




static tch_timer_id tch_timer_create(const tch_timer_def* timer_def,tch_timer_type type,void* arg);
static osStatus tch_timer_start(tch_timer_id timer,uint32_t millisec);
static osStatus tch_timer_stop(tch_timer_id timer);
static osStatus tch_timer_delete(tch_timer_id timer);



__attribute__((section(".data"))) static tch_timer_ix TIMER_StaticInstance = {
		tch_timer_create,
		tch_timer_start,
		tch_timer_stop,
		tch_timer_delete
};

const tch_timer_ix* Timer = (const tch_timer_ix*)&TIMER_StaticInstance;




tch_timer_id tch_timer_create(const tch_timer_def* timer_def,tch_timer_type type,void* arg){

}

osStatus tch_timer_start(tch_timer_id timer,uint32_t millisec){

}

osStatus tch_timer_stop(tch_timer_id timer){

}

osStatus tch_timer_delete(tch_timer_id timer){

}