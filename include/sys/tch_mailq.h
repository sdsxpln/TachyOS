/*
 * tch_mailq.h
 *
 *  Created on: 2014. 9. 5.
 *      Author: innocentevil
 */

#ifndef TCH_MAILQ_H_
#define TCH_MAILQ_H_


#if defined(__cplusplus)
extern "C" {
#endif


typedef void* tch_mailqId;


struct _tch_mailbox_ix_t {
	tch_mailqId (*create)(size_t sz,uint32_t qlen);
	void* (*alloc)(tch_mailqId qid,uint32_t millisec,tchStatus* result);
	void* (*calloc)(tch_mailqId qid,uint32_t millisec,tchStatus* result);
	tchStatus (*put)(tch_mailqId qid,void* mail);
	osEvent (*get)(tch_mailqId qid,uint32_t millisec);
	tchStatus (*free)(tch_mailqId qid,void* mail);
	tchStatus (*destroy)(tch_mailqId qid);
};

#if defined(__cplusplus)
}
#endif

#endif /* TCH_MAILQ_H_ */