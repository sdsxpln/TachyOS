/*
 * tch_usart.c
 *
 *  Copyright (C) 2014 doowoong,lee
 *  All rights reserved.
 *
 *  This software may be modified and distributed under the terms
 *  of the LGPL v3 license.  See the LICENSE file for details.
 *
 *
 *  Created on: 2014. 6. 27.
 *      Author: innocentevil
 */


#include "tch_hal.h"
#include "tch_halinit.h"
#include "tch_dma.h"
#include "tch_halcfg.h"


#define TCH_UART_CLASS_KEY     ((uint16_t) 0x3D02)




#define TCH_UTX_ID                          ((uint32_t) 0x5D)
#define TCH_URX_ID                          ((uint32_t) 0x5F)

#define USART_Parity_Pos_CR1                (uint8_t) (9)
#define USART_Parity_NON                    (uint8_t) (0 << 1 | 0 << 0)
#define USART_Parity_ODD                    (uint8_t) (1 << 1 | 1 << 0)
#define USART_Parity_EVEN                   (uint8_t) (1 << 1 | 0 << 0)

#define USART_StopBit_Pos_CR2               (uint8_t) (12)
#define USART_StopBit_1B                    (uint8_t) (0)
#define USART_StopBit_0dot5B                (uint8_t) (1)
#define USART_StopBit_2B                    (uint8_t) (2)
#define USART_StopBit_1dot5B                (uint8_t) (3)


#define INIT_UART_STOPBIT                   {\
	                                          USART_StopBit_0dot5B,\
	                                          USART_StopBit_1B,\
	                                          USART_StopBit_1dot5B,\
	                                          USART_StopBit_2B\
}


#define INIT_UART_PARITY                   {\
	                                          USART_Parity_NON,\
	                                          USART_Parity_ODD,\
	                                          USART_Parity_EVEN\
}


struct tch_lld_usart_arg_t {
	uint8_t*     c;
	void*        handle;
};




typedef struct tch_lld_usart_handle_prototype_t {
	tch_UartHandle                    pix;
	uint8_t                           idx;
	tch_DmaHandle*                    txDma;
	tch_DmaHandle*                    rxDma;
	tch_asyncId                       ast;
	tch_GpioHandle*                   ioHandle;
	tch_mtxId                         rxMtx;
	tch_condvId                       rxCondv;
	tch_mtxId                         txMtx;
	tch_condvId                       txCondv;
	uint32_t                          status;
	tch*                              env;
}tch_UartHandlePrototype;

typedef struct tch_lld_uart_prototype {
	tch_lld_usart                     pix;
	tch_mtxId                         mtx;
	tch_condvId                       condv;
	uint16_t                          occp_state;
	uint16_t                          lpoccp_state;
}tch_lld_uart_prototype;

static tch_UartHandle* tch_uartOpen(tch* env,tch_UartCfg* cfg,uint32_t timeout,tch_PwrOpt popt);
static BOOL tch_uartClose(tch_UartHandle* handle);

static tchStatus tch_uartPutc(tch_UartHandle* handle,uint8_t c);
static tchStatus tch_uartGetc(tch_UartHandle* handle,uint8_t* rc,uint32_t timeout);
static tchStatus tch_uartWrite(tch_UartHandle* handle,const uint8_t* bp,size_t sz);
static tchStatus tch_uartRead(tch_UartHandle* handle,uint8_t* bp, size_t* sz,uint32_t timeout);
static tchStatus tch_uartWriteCstr(tch_UartHandle* handle,const char* cstr);
static tchStatus tch_uartReadCstr(tch_UartHandle* handle,char* cstr,uint32_t timeout);

static tchStatus tch_uartPutcDma(tch_UartHandle* handle,uint8_t c);
static tchStatus tch_uartGetcDma(tch_UartHandle* handle,uint8_t* rc,uint32_t timeout);
static tchStatus tch_uartWriteDma(tch_UartHandle* handle,const uint8_t* bp,size_t sz);
static tchStatus tch_uartReadDma(tch_UartHandle* handle,uint8_t* bp, size_t* sz,uint32_t timeout);


static BOOL tch_uartHandleInterrupt(tch_UartHandlePrototype* _handle,void* _hw);
static inline void tch_uartValidate(tch_UartHandlePrototype* _handle);
static inline void tch_uartInvalidate(tch_UartHandlePrototype* _handle);
static inline BOOL tch_uartIsValid(tch_UartHandlePrototype* _handle);
static DECL_ASYNC_TASK(tch_uartTxAsyncTask);
static DECL_ASYNC_TASK(tch_uartRxAsyncTask);

__attribute__((section(".data"))) static tch_lld_uart_prototype UART_StaticInstance = {
		{
				INIT_UART_STOPBIT,
				INIT_UART_PARITY,
				MFEATURE_UART,
				tch_uartOpen,
				tch_uartClose
		},
		NULL,
		NULL,
		0,
		0
};


const tch_lld_usart* tch_usart_instance = (const tch_lld_usart*) &UART_StaticInstance;

static tch_UartHandle* tch_uartOpen(tch* env,tch_UartCfg* cfg,uint32_t timeout,tch_PwrOpt popt){
	if(cfg->UartCh >= MFEATURE_UART)    // if requested channel is larger than uart channel count
		return NULL;                    // return null
	if(!UART_StaticInstance.mtx)
		UART_StaticInstance.mtx = env->Mtx->create();
	if(!UART_StaticInstance.condv)
		UART_StaticInstance.condv = env->Condv->create();
	if(env->Device->interrupt->isISR())
		return NULL;
	uint32_t umskb = 1 << cfg->UartCh;
	if(env->Mtx->lock(UART_StaticInstance.mtx,timeout) != osOK)
		return NULL;
	while(UART_StaticInstance.occp_state & umskb){
		if(env->Condv->wait(UART_StaticInstance.condv,UART_StaticInstance.mtx,timeout) != osOK)
			return NULL;
	}
	UART_StaticInstance.occp_state |= umskb;
	env->Mtx->unlock(UART_StaticInstance.mtx);

	tch_uart_descriptor* uDesc = &UART_HWs[cfg->UartCh];
	USART_TypeDef* uhw = (USART_TypeDef*) uDesc->_hw;
	tch_uart_bs* ubs = &UART_BD_CFGs[cfg->UartCh];

	tch_GpioCfg iocfg;
	iocfg.Af = ubs->afv;
	env->Device->gpio->initCfg(&iocfg);

	uint32_t io_msk = (1 << ubs->rxp) | (1 << ubs->txp);
	if(cfg->FlowCtrl && (ubs->rtsp != -1) && (ubs->ctsp != -1)){
		io_msk |= (1 << ubs->rtsp) | (1 << ubs->ctsp);
	}

	tch_GpioHandle* iohandle = env->Device->gpio->allocIo(env,ubs->port,io_msk,&iocfg,timeout,popt); // try get io handle
	if(!iohandle){   // if requested io has been occupied, then clear uart occupation mark and return null
		if(env->Mtx->lock(UART_StaticInstance.mtx,timeout) != osOK)
			return NULL;
		UART_StaticInstance.occp_state &= ~umskb;
		env->Condv->wakeAll(UART_StaticInstance.condv);
		env->Mtx->unlock(UART_StaticInstance.mtx);
		return NULL;
	}

	tch_UartHandlePrototype* uins = env->Mem->alloc(sizeof(tch_UartHandlePrototype));   // if successfully get io handle, create uart handle instance
	uins->rxMtx = env->Mtx->create();
	uins->rxCondv = env->Condv->create();
	uins->txMtx = env->Mtx->create();
	uins->txCondv = env->Condv->create();
	uins->ast = env->Async->create(1);

	uDesc->_handle = uins;
	uins->idx = cfg->UartCh;
	uins->ioHandle = iohandle;
	uins->env = env;

	/*  Uart Baudrate Configuration  */
	uint8_t psc = 2;
	if(uDesc->_clkenr == &RCC->APB1ENR)
		psc = 4;

	uint32_t ioclk = SYS_CLK / psc;

	float udiv = (float)ioclk / (float)cfg->Buadrate;

	int mantisa = (int)udiv;
	if(mantisa > udiv)
		mantisa--;
	float frac = udiv - (float) mantisa;
	int dfrac = (int)(udiv * 16);


	*uDesc->_clkenr |= uDesc->clkmsk; // enable clk source
	if(popt == ActOnSleep)  // if sleep - active should be supported, enable lp clk also
		*uDesc->_lpclkenr |= uDesc->lpclkmsk;

	uhw->BRR = 0;   // initialize buadrate register
	uhw->BRR |= (mantisa << 4);
	uhw->BRR |= dfrac;  // set calcuated value

	uhw->CR1 = (cfg->Parity << USART_Parity_Pos_CR1) | USART_CR1_PEIE | USART_CR1_TE | USART_CR1_RE;   // enable interrupt packet error / transmit error / receiving error
	uhw->CR2 = (cfg->StopBit << USART_StopBit_Pos_CR2);  // set stop bit

	tch_DmaCfg dmaCfg;
	tch_lld_dma* DMA = (tch_lld_dma*)env->Device->dma;

	uins->txDma = NULL;
	uins->rxDma = NULL;

	if(ubs->txdma != DMA_NOT_USED){ // setup tx dma
		dmaCfg.BufferType = DMA->BufferType.Normal;
		dmaCfg.Ch = ubs->txch;
		dmaCfg.Dir = DMA->Dir.MemToPeriph;
		dmaCfg.FlowCtrl = DMA->FlowCtrl.DMA;
		dmaCfg.Priority = DMA->Priority.Normal;
		dmaCfg.mAlign = DMA->Align.Byte;
		dmaCfg.mBurstSize = DMA->BurstSize.Burst1;
		dmaCfg.mInc = TRUE;
		dmaCfg.pAlign = DMA->Align.Byte;
		dmaCfg.pBurstSize = DMA->BurstSize.Burst1;
		dmaCfg.pInc = FALSE;

		uins->txDma = DMA->allocDma(env,ubs->txdma,&dmaCfg,timeout,popt); // can be null
	}

	if(ubs->rxdma != DMA_NOT_USED){ // setup rx dma
		dmaCfg.BufferType = DMA->BufferType.Normal;
		dmaCfg.Ch = ubs->rxch;
		dmaCfg.Dir = DMA->Dir.PeriphToMem;
		dmaCfg.FlowCtrl = DMA->FlowCtrl.DMA;
		dmaCfg.Priority = DMA->Priority.Normal;
		dmaCfg.mAlign = DMA->Align.Byte;
		dmaCfg.mBurstSize = DMA->BurstSize.Burst1;
		dmaCfg.mInc = TRUE;
		dmaCfg.pAlign = DMA->Align.Byte;
		dmaCfg.pBurstSize = DMA->BurstSize.Burst1;
		dmaCfg.pInc = FALSE;

		uins->rxDma = DMA->allocDma(env,ubs->rxdma,&dmaCfg,timeout,popt);  // can be null
	}


	if(uins->txDma){ // if tx dma is non-null (available), uart handle routines supporting dma are bound
		uins->pix.putc = tch_uartPutcDma;
		uins->pix.write = tch_uartWriteDma;
		uins->pix.writeCstr = tch_uartWriteCstr;

		uhw->CR1 &= ~USART_CR1_TCIE;
		uhw->CR3 |= USART_CR3_DMAT;
	}else{  // otherwise, non-dma routines are bound
		uins->pix.putc = tch_uartPutc;
		uins->pix.write = tch_uartWrite;
		uins->pix.writeCstr = tch_uartWriteCstr;

		uhw->CR3 &= ~USART_CR3_DMAT;
		uhw->CR1 |= USART_CR1_TCIE;
	}

	if(uins->rxDma){
		uins->pix.getc = tch_uartGetcDma;
		uins->pix.read = tch_uartReadDma;
		uins->pix.readCstr = tch_uartReadCstr;

		uhw->CR1 &= ~USART_CR1_RXNEIE;
		uhw->CR3 |= USART_CR3_DMAR;
	}else{
		uins->pix.getc = tch_uartGetc;
		uins->pix.read = tch_uartRead;
		uins->pix.readCstr = tch_uartReadCstr;

		uhw->CR3 &= ~USART_CR3_DMAR;
	}


	tch_uartValidate(uins);
	env->Device->interrupt->enable(uDesc->irq);
	return (tch_UartHandle*) uins;
}



static BOOL tch_uartClose(tch_UartHandle* handle){
	tch_UartHandlePrototype* ins = (tch_UartHandlePrototype*) handle;
	tch_uart_descriptor* uDesc = &UART_HWs[ins->idx];
	tch* env = ins->env;
	if(!tch_uartIsValid(ins))
		return FALSE;
	if(env->Device->interrupt->isISR())
		return FALSE;
	if(env->Mtx->lock(ins->rxMtx,osWaitForever) != osOK)
		return FALSE;

	if(ins->txDma){
		env->Device->dma->freeDma(ins->txDma);
	}
	if(ins->rxDma){
		env->Device->dma->freeDma(ins->rxDma);
	}

	tch_uartInvalidate(ins);
	env->Condv->destroy(ins->rxCondv);
	env->Mtx->destroy(ins->rxMtx);
	env->Device->gpio->freeIo(ins->ioHandle);


	if(env->Mtx->lock(UART_StaticInstance.mtx,osWaitForever) != osOK){
		return FALSE;
	}

	UART_StaticInstance.occp_state &= ~(1 << ins->idx); // clear Occupation flag
	UART_StaticInstance.lpoccp_state &= ~(1 << ins->idx);
	env->Condv->wake(UART_StaticInstance.condv);

	env->Mtx-> unlock(UART_StaticInstance.mtx);
	return TRUE;
}


static tchStatus tch_uartPutc(tch_UartHandle* handle,uint8_t c){
	tch_UartHandlePrototype* ins = (tch_UartHandlePrototype*) handle;
	USART_TypeDef* uhw = UART_HWs[ins->idx]._hw;
	tchStatus result = osOK;
	tch* env = ins->env;
	if(!tch_uartIsValid(ins))
		return osErrorResource;
	if(env->Device->interrupt->isISR())
		return osErrorISR;

	if((result = env->Mtx->lock(ins->txMtx,osWaitForever)) != osOK)
		return result;
	while(!(uhw->SR & USART_SR_TXE)){
		if((result = env->Condv->wait(ins->txCondv,ins->txMtx,osWaitForever)) != osOK)
			return result;
	}
	struct tch_lld_usart_arg_t args;
	*args.c = c;
	args.handle = handle;
	result = env->Async->wait(ins->ast,TCH_UTX_ID,tch_uartTxAsyncTask,osWaitForever,&args);

	env->Condv->wakeAll(ins->txCondv);
	env->Mtx->unlock(ins->txMtx);
	return result;
}

static tchStatus tch_uartGetc(tch_UartHandle* handle,uint8_t* rc,uint32_t timeout){
	tch_UartHandlePrototype* ins = (tch_UartHandlePrototype*) handle;
	USART_TypeDef* uhw = (USART_TypeDef*)UART_HWs[ins->idx]._hw;
	tch* env = ins->env;
	tchStatus result = osOK;
	*rc = '\0';
	if(!tch_uartIsValid(ins))
		return osErrorResource;
	if(env->Device->interrupt->isISR())
		return osErrorISR;
	if((result = env->Mtx->lock(ins->rxMtx,timeout)) != osOK)
		return result;
	while(!(uhw->SR & USART_SR_RXNE)){
		if((result = env->Condv->wait(ins->rxCondv,ins->rxMtx,timeout)) != osOK)
			return result;
	}
	if((result = env->Async->wait(ins->ast,TCH_URX_ID,tch_uartRxAsyncTask,osWaitForever,handle)) != osOK)
		return result;
	*rc = uhw->DR;    // read data from data register, and hw clear rxne flag in status register
	uhw->SR &= ~USART_SR_RXNE;
	env->Condv->wakeAll(ins->rxCondv);

	return env->Mtx->unlock(ins->rxMtx);
}

static tchStatus tch_uartWrite(tch_UartHandle* handle,const uint8_t* bp,size_t sz){
	tch_UartHandlePrototype* ins = (tch_UartHandlePrototype*) handle;
	USART_TypeDef* uhw = UART_HWs[ins->idx]._hw;
	tch* env = ins->env;
	tchStatus result = osOK;
	if(!tch_uartIsValid(ins))
		return osErrorResource;
	if(env->Device->interrupt->isISR())
		return osErrorISR;

	if(env->Mtx->lock(ins->txMtx,osWaitForever) != osOK)
		return osErrorResource;
	struct tch_lld_usart_arg_t args;
	args.handle = handle;
	size_t idx = 0;
	for(;idx < sz;idx++){
		while(!(uhw->SR & USART_SR_TXE)){
			if((result = env->Condv->wait(ins->txCondv,ins->txMtx,osWaitForever)) != osOK)
				return result;
		}
		args.c = (uint8_t*) bp + idx;
		result = env->Async->wait(ins->ast,TCH_UTX_ID,tch_uartTxAsyncTask,osWaitForever,&args);
	}
	env->Condv->wakeAll(ins->txCondv);
	env->Mtx->unlock(ins->txMtx);
	return result;
}

static tchStatus tch_uartRead(tch_UartHandle* handle,uint8_t* bp, size_t* sz,uint32_t timeout){
	tch_UartHandlePrototype* ins = (tch_UartHandlePrototype*) handle;
	USART_TypeDef* uhw = UART_HWs[ins->idx]._hw;
	tch* env = ins->env;
	tchStatus result = osOK;
	*bp = '\0';
	size_t idx = 0;
	if(!tch_uartIsValid(ins))
		return osErrorResource;
	if(!env->Device->interrupt->isISR())
		return osErrorISR;
	if((result = env->Mtx->lock(ins->rxMtx,timeout)) != osOK)
		return result;
	for(;idx < *sz;idx++){
		while(!(uhw->SR & USART_SR_RXNE)){
			if((result = env->Condv->wait(ins->rxCondv,ins->rxMtx,timeout)) != osOK)
				return result;
		}
		result = env->Async->wait(ins->ast,TCH_URX_ID,tch_uartRxAsyncTask,timeout,handle);
		*(bp + idx) = uhw->DR;
	}
	env->Mtx->unlock(ins->rxMtx);
	return result;
}


static tchStatus tch_uartPutcDma(tch_UartHandle* handle,uint8_t c){

}

static tchStatus tch_uartGetcDma(tch_UartHandle* handle,uint8_t* rc,uint32_t timeout){

}

static tchStatus tch_uartWriteDma(tch_UartHandle* handle,const uint8_t* bp,size_t sz){

}

static tchStatus tch_uartReadDma(tch_UartHandle* handle,uint8_t* bp, size_t* sz,uint32_t timeout){

}

static tchStatus tch_uartWriteCstr(tch_UartHandle* handle,const char* cstr){

}

static tchStatus tch_uartReadCstr(tch_UartHandle* handle,char* cstr,uint32_t timeout){

}



static inline void tch_uartValidate(tch_UartHandlePrototype* _handle){
	_handle->status = (((uint32_t) _handle) & 0xFFFF) ^ TCH_UART_CLASS_KEY;
}

static inline void tch_uartInvalidate(tch_UartHandlePrototype* _handle){
	_handle->status &= ~0xFFFF;

}

static inline BOOL tch_uartIsValid(tch_UartHandlePrototype* _handle){
	return (_handle->status & 0xFFFF) == (((uint32_t) _handle) & 0xFFFF) ^ TCH_UART_CLASS_KEY;
}

static DECL_ASYNC_TASK(tch_uartTxAsyncTask){
	struct tch_lld_usart_arg_t* arg_u = (struct tch_lld_usart_arg_t*)arg;
	tch_UartHandlePrototype* handle = (tch_UartHandlePrototype*)arg_u->handle;
	USART_TypeDef* uhw = (USART_TypeDef*)&UART_HWs[handle->idx]._hw;
	uhw->DR = *arg_u->c;
	return TRUE;
}

static DECL_ASYNC_TASK(tch_uartRxAsyncTask){
	tch_UartHandlePrototype* handle = (tch_UartHandlePrototype*) arg;
	USART_TypeDef* uhw = (USART_TypeDef*)&UART_HWs[handle->idx]._hw;
	uhw->CR1 |= USART_CR1_RXNEIE;
}

static BOOL tch_uartHandleInterrupt(tch_UartHandlePrototype* _handle,void* _hw){
	USART_TypeDef* uhw = (USART_TypeDef*) _hw;
	tch* env = _handle->env;
	if(uhw->SR & USART_SR_RXNE){
		if(_handle->rxDma)
			return FALSE;
		uhw->CR1 &= ~USART_CR1_RXNEIE;    // disable rxne interrupt
	}
	if(uhw->SR & USART_SR_TC){
		if(_handle->txDma)
			return FALSE;
		uhw->SR &= ~USART_SR_TC;
		env->Async->notify(_handle->ast,TCH_UTX_ID,osOK);
		return TRUE;
	}
	if(uhw->SR & USART_SR_ORE){

	}
	if(uhw->SR & USART_SR_FE){

	}
	if(uhw->SR & USART_SR_PE){

	}
}


void USART1_IRQHandler(void){
	tch_uart_descriptor* uDesc = &UART_HWs[0];
	USART_TypeDef* uhw = (USART_TypeDef*) uDesc->_hw;
	tch_UartHandlePrototype* ins = uDesc->_handle;
	if(!ins){ // if handle is not bound to io, clear raised interrupt
		uint8_t dummy = uhw->DR;
		uhw->SR = 0;
		return;
	}



}

void USART2_IRQHandler(void){

}

void USART3_IRQHandler(void){

}

void UART4_IRQHandler(void){

}


