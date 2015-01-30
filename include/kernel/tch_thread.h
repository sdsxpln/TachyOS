/*
 * tch_thread.h
 *
 *
 * Copyright (C) 2014 doowoong,lee
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the LGPL v3 license.  See the LICENSE file for details.
 *
 *  Created on: 2014. 6. 30.
 *      Author: innocentevil
 */

#ifndef TCH_THREAD_H_
#define TCH_THREAD_H_


#include "tch_TypeDef.h"

#if defined(__cplusplus)
extern "C"{
#endif


#define THREAD_ROOT_BIT    ((uint8_t) 1 << 0)
#define THREAD_DEATH_BIT     ((uint8_t) 1 << 1)


extern BOOL tch_threadIsValid(tch_threadId thread);

#if defined(__cplusplus)
}
#endif

#endif /* TCH_THREAD_H_ */