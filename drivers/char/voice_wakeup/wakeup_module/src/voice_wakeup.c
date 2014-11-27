#include "ivDefine.h"
#include "ivIvwDefine.h"
#include "ivIvwErrorCode.h"
#include "ivIVW.h"
#include "ivPlatform.h"

#include "jz_dma.h"
#include "circ_buf.h"
#include "dmic_config.h"
#include "interface.h"
#include "voice_wakeup.h"
#include <common.h>

#define VOICE_RES_MEM_START	(0x8ff28000)

unsigned int __res_mem wakeup_res[] = {
#include "ivModel_v21.data"
};

//#define MAX_PROCESS_LEN		(512 * 1000)
#define MAX_PROCESS_LEN		(128 * 1000)
int total_process_len;

unsigned char __attribute__((aligned(32))) pIvwObjBuf[20*1024];
unsigned char __attribute__((aligned(32))) nReisdentBuf[38];

ivPointer __attribute__((aligned(32))) pIvwObj;
ivPointer __attribute__((aligned(32))) pResidentRAM;

int send_data_to_process(ivPointer pIvwObj, unsigned char *pdata, unsigned int size)
{
	unsigned int nSize_PCM = size;
	unsigned int tmp;
	int i;
	ivInt16 CMScore  =  0;
	ivInt16 ResID = 0;
	ivInt16 KeywordID = 0;
	ivUInt32 StartMS = 0;
	ivUInt32 EndMS = 0;
	unsigned int Samples;
	unsigned int BytesPerSample;
	unsigned char *pData = pdata;
	unsigned int nSamples_count;
	ivStatus iStatus1;
	ivStatus iStatus2;
	Samples = 110;

	//printf("size:%d\n", size);
	BytesPerSample = 2;
	nSamples_count = nSize_PCM / (Samples * BytesPerSample);
	/*process 16k, 16bit samples*/
	for(i = 0; i <= nSamples_count; i++ )
	{
		if(i == nSamples_count) {
			tmp = nSize_PCM % (Samples*BytesPerSample);
			if(!tmp) {
				break;
			}
			Samples = tmp / BytesPerSample;
		}
		iStatus1 = IvwAppendAudioData(pIvwObj,(ivCPointer)pData, Samples);
		pData = pData + Samples * BytesPerSample;
		if( iStatus1 != IvwErrID_OK )
		{
			if( iStatus1 == IvwErr_BufferFull ){
				//printf("IvwAppendAudioData Error:IvwErr_BufferFull\n");
			}else if( iStatus1 ==  IvwErr_InvArg ){
				//printf("IvwAppendAudioData Error:IvwErr_InvArg\n");
			}else if( iStatus1 ==  IvwErr_InvCal ){
				//printf("IvwAppendAudioData Error:IvwErr_InvCal\n");
			}
			//printf("IvwAppendAudioData Error: %d\n", iStatus1);
		} else {
			//printf("IvwAppendAudioData Ok!!!!!!!!\n");
		}
		iStatus2 = IvwRunStepEx( pIvwObj, &CMScore,  &ResID, &KeywordID,  &StartMS, &EndMS );
		if( IvwErr_WakeUp == iStatus2 ){
			//printf("%d: #########System is Wake Up\n", i+1);
			return IvwErr_WakeUp;
		}
#if 0
		else if( iStatus2 == IvwErrID_OK )
			printf("%d: iStatus2 == IvwErrID_OK\n", i);
		else if( iStatus2 == IvwErr_InvArg )
			printf("%d: iStatus2 == IvwErr_InvArg\n", i);
		else if( iStatus2 == IvwErr_ReEnter )
			printf("%d: iStatus2 == IvwErr_ReEnter\n", i);
		else if( iStatus2 == IvwErr_InvCal )
			printf("%d: iStatus2 == IvwErr_InvCal\n", i);
		else if( iStatus2 == IvwErr_BufferEmpty )
			printf("%d: iStatus2 == IvwErr_BufferEmpty\n", i);
#endif
	}
	return 0;
}



struct fifo {
	struct circ_buf xfer;
	u32	n_size;
};
struct fifo rx_fifo[0];
//#define DMA_CHANNEL		(5)

int wakeup_open(void)
{
	ivSize nIvwObjSize = 20 * 1024;
	pIvwObj = (ivPointer)pIvwObjBuf;
	ivUInt16 nResidentRAMSize = 38;
	pResidentRAM = (ivPointer)nReisdentBuf;
	unsigned char __attribute__((aligned(32))) *pResKey = (unsigned char *)wakeup_res;

	ivUInt16 nWakeupNetworkID = 0;

	ivStatus iStatus;
	printf("wakeu module init#####\n");
	printf("pIvwObj:%x, pResidentRAM:%x, pResKey:%x\n", pIvwObj, pResidentRAM, pResKey);
	iStatus = IvwCreate(pIvwObj, &nIvwObjSize, pResidentRAM, &nResidentRAMSize, pResKey, nWakeupNetworkID);
	if( IvwErrID_OK != iStatus ){
		printf("IvwVreate Error: %d\n", iStatus);
		return ivFalse;
	}
	printf("pIvwObj create ok!!!!!\n");
	IvwSetParam( pIvwObj, IVW_CM_THRESHOLD, 10, 0 ,0);
	IvwSetParam( pIvwObj, IVW_CM_THRESHOLD, 20, 1 ,0);
	IvwSetParam( pIvwObj, IVW_CM_THRESHOLD, 15, 2 ,0);
	printf("###ivw param set ok!!!!\n");
	/* code that need rewrite */
	struct circ_buf *xfer = &rx_fifo->xfer;
	rx_fifo->n_size	= BUF_SIZE; /*tcsm 4kBytes*/
	//xfer->buf = (char *)0xb3427000;
	xfer->buf = (char *)TCSM_BANK_5;
	xfer->head = (char *)(pdma_trans_addr(DMA_CHANNEL, 2) | 0xA0000000) - xfer->buf;
	xfer->tail = xfer->head;

	printf("pdma_trans_addr:%x, xfer->buf:%x, xfer->head:%x, xfer->tail:%x\n",pdma_trans_addr(DMA_CHANNEL, 2), xfer->buf, xfer->head, xfer->tail);
	return 0;
}

int wakeup_close(void)
{

	return 0;
}




//#define MAX_PROCESS_LEN		(16*2 * 1000 * 5)

int process_dma_data_2(void)
{

	unsigned int dma_addr;
	int nbytes;
	int ret;
	struct circ_buf *xfer;
	//printk("MAX_PROCESS_LEN:%08x\n", MAX_PROCESS_LEN);
	ret = SYS_NEED_DATA;

	dma_addr = pdma_trans_addr(DMA_CHANNEL, DMA_DEV_TO_MEM);
	xfer = &rx_fifo->xfer;
	xfer->head = (char *)(dma_addr | 0xA0000000) - xfer->buf;

	//serial_put_hex(dma_addr);
	if(0) {
		printf("current dma transaddr :%x, xfer->head:%x, xfer->tail:%x, xfer->buf:%x\n",
				dma_addr, xfer->head, xfer->tail, xfer->buf);
	}
	nbytes = CIRC_CNT(xfer->head, xfer->tail, rx_fifo->n_size);
	if(0) {
		printf("nbytes:%d, xfer->head:%d, xfer->tail:%d,dma_addr:%x\n", nbytes, xfer->head, xfer->tail, dma_addr);
		xfer->tail += nbytes;
		xfer->tail %= rx_fifo->n_size;
	}
	if(nbytes > 220) {
		//printf("process 512 bytes!!!\n");
		/*process data every 512 bytes.*/
		while(1) {
			int nread;
			nread = CIRC_CNT(xfer->head, xfer->tail, rx_fifo->n_size);
			if(nread > CIRC_CNT_TO_END(xfer->head, xfer->tail, rx_fifo->n_size)) {
				nread = CIRC_CNT_TO_END(xfer->head, xfer->tail, rx_fifo->n_size);
			} else if(nread == 0) {
				break;
			}
			ret = send_data_to_process(pIvwObj, (unsigned char *)xfer->buf + xfer->tail, nread);
			if(ret == IvwErr_WakeUp) {
				//printf("####system wakeup ok!!!\n");
				return SYS_WAKEUP_OK;
			}

			xfer->tail += nread;
			xfer->tail %= rx_fifo->n_size;
		}
	} else {

	}

	total_process_len += nbytes;
	if(total_process_len >= MAX_PROCESS_LEN) {
		total_process_len = 0;
		wakeup_failed_times++;
		return SYS_WAKEUP_FAILED;
	} else {
		return SYS_NEED_DATA;
	}

	//printk("total_process_len:%d\n", total_process_len);
	return SYS_WAKEUP_FAILED;
}

int process_dma_data(void)
{

	unsigned int dma_addr;
	int nbytes;
	int ret;
	struct circ_buf *xfer;

	dma_addr = pdma_trans_addr(DMA_CHANNEL, DMA_DEV_TO_MEM);
	xfer = &rx_fifo->xfer;
	xfer->head = (char *)(dma_addr | 0xA0000000) - xfer->buf;

	//serial_put_hex(dma_addr);
	if(0) {
		printf("current dma transaddr :%x, xfer->head:%x, xfer->tail:%x, xfer->buf:%x\n",
				dma_addr, xfer->head, xfer->tail, xfer->buf);
	}
	nbytes = CIRC_CNT(xfer->head, xfer->tail, rx_fifo->n_size);
	if(0) {
		printf("nbytes:%d, xfer->head:%d, xfer->tail:%d,dma_addr:%x\n", nbytes, xfer->head, xfer->tail, dma_addr);
		xfer->tail += nbytes;
		xfer->tail %= rx_fifo->n_size;
	}
	if(nbytes > 220) {
		//printf("process 512 bytes!!!\n");
		/*process data every 512 bytes.*/
		while(1) {
			int nread;
			nread = CIRC_CNT(xfer->head, xfer->tail, rx_fifo->n_size);
			if(nread > CIRC_CNT_TO_END(xfer->head, xfer->tail, rx_fifo->n_size)) {
				nread = CIRC_CNT_TO_END(xfer->head, xfer->tail, rx_fifo->n_size);
			} else if(nread == 0) {
				break;
			}
			ret = send_data_to_process(pIvwObj, (unsigned char *)xfer->buf + xfer->tail, nread);
			if(ret == IvwErr_WakeUp) {
				printf("####system wakeup ok!!!\n");
				return SYS_WAKEUP_OK;
			}

			xfer->tail += nread;
			xfer->tail %= rx_fifo->n_size;
		}
	} else {
		/*need more data*/
		//return SYS_WAKEUP_FAILED;
	}
	return SYS_WAKEUP_FAILED;
}

int process_buffer_data(unsigned char *buf, unsigned long len)
{
	int ret;
	ret = send_data_to_process(pIvwObj, buf, len);
	if(ret == IvwErr_WakeUp) {
		return SYS_WAKEUP_OK;
	}
	return SYS_WAKEUP_FAILED;
}
