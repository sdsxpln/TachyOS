/*
 * tch_msgq.h
 *
 *  Created on: 2014. 9. 5.
 *      Author: innocentevil
 */

#ifndef TCH_MSGQ_H_
#define TCH_MSGQ_H_

#if defined(__cplusplus)
extern "C"{
#endif


typedef struct _tch_msgq_karg_t{
	uword_t     msg;
	uint32_t    timeout;
}tch_msgq_karg;

extern tchStatus tch_msgq_kput(tch_msgqId,tch_msgq_karg* arg);
extern tchStatus tch_msgq_kget(tch_msgqId,tch_msgq_karg* arg);
extern tchStatus tch_msgq_kdestroy(tch_msgqId);



#if defined(__cplusplus)
}
#endif

#endif /* TCH_MSGQ_H_ */