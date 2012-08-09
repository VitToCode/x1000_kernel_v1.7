/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2007 by dsqiu (dsqiu@ingenic.cn)
 */

#include "testfunc.h"
#include "vnandinfo.h"
#include "nandinterface.h"
#include "pagelist.h"
#include "context.h"
#include "vNand.h"
#include <string.h>

extern NandInterface em_nand_ops;

static void test_APP_vnand(void)
{
	printf("init %p \n",em_nand_ops.iInitNand);
	printf("PageRaed %p \n",em_nand_ops.iPageRead);
	printf("Pagewrite %p \n",em_nand_ops.iPageWrite);
	printf("MultiPageRead %p\n",em_nand_ops.iMultiPageRead);
	printf("MultiPageWrite %p\n",em_nand_ops.iMultiPageWrite);
	printf("MultiBlockErase %p\n",em_nand_ops.iMultiBlockErase);
	printf("MarkBadBlock %p \n",em_nand_ops.iMarkBadBlock);
}

extern void test_operator_vnand(VNandInfo *vnandptr);

extern Context context;

int start_test_nand(int argc, char *argv[]){

	Context * conptr = &context;

	VNandInfo * vnandptr = &conptr->vnand;

	int i = 0;
	int j = 0;
	int ret = -1;
	unsigned char *ptr = (unsigned char *)malloc(2048);
	if(ptr == 0)
		return -1;
	PageList * pageptr = (PageList *)malloc(sizeof(*pageptr));	
	if(pageptr == 0)
		return -1;
	PageList * pageptr1 = (PageList *)malloc(sizeof(*pageptr1));	
	if(pageptr1 == 0)
		return -1;

	
/*test app driver init */
	printf("init get argment : \n");
	printf("startBlockID %d \n",vnandptr->startBlockID);
	printf("PagePerBlock %d \n",vnandptr->PagePerBlock);
	printf("BytePerPage  %d \n",vnandptr->BytePerPage);
	printf("TotalBlocks   %d \n",vnandptr->TotalBlocks);
	printf("hwSector     %d \n\n",vnandptr->hwSector);

/*test write and read */
	
    test_APP_vnand();
	memset(ptr , 0x55, 2048);
//	pageptr->head.next = pageptr1;

	singlelist_add(&pageptr->head,&pageptr1->head);
	
	printf("pagaptr %p \n",pageptr);
	printf("pagaptr1 %p pageptr->head.next %p\n",pageptr1,(pageptr->head).next);
	printf("pagelist %p\n",singlelist_entry((pageptr->head).next,PageList,head));

	unsigned int total = (vnandptr->PagePerBlock )*(vnandptr->TotalBlocks) / 4 ;
	for(i = 0 ; i < total; i++)
	{
		pageptr->startPageID = i;
		pageptr->Bytes = 2048;
		pageptr->OffsetBytes = 0;
		pageptr->pData = ptr;
		//pageptr->head.next = 0;

		pageptr1->startPageID = i+1;
		pageptr1->Bytes = 2048;
		pageptr1->OffsetBytes = 0;
		pageptr1->pData = ptr;
		pageptr1->head.next = 0;
		
		vNand_MultiPageWrite(vnandptr,pageptr);

		if(pageptr->retVal != 2048)
			printf("retval %d func %s line %d \n",pageptr->retVal,__FUNCTION__,__LINE__);
		if(pageptr1->retVal != 2048)
			printf("retval %d func %s line %d \n",pageptr1->retVal,__FUNCTION__,__LINE__);

		//printf("write %d \n", i);	
	}

	printf("write nand finish \n");

	memset(ptr,0x0,2048);
	unsigned char testread = 0x55;
	total = total * 2;
	for(i = 0 ; i < 2048; i++)
	{
		pageptr->startPageID = i;
		pageptr->Bytes = 2048;
		pageptr->OffsetBytes = 0;
		pageptr->pData = ptr;
		pageptr->head.next= 0;

		vNand_MultiPageRead(vnandptr,pageptr);

		if(pageptr->retVal != 2048)
			printf("retval %d func %s line %d \n",pageptr->retVal,__FUNCTION__,__LINE__);
		
		printf("read %d \n", i);

		while(j < 2048)
		{
			testread &=ptr[j++];
		}
	}
	
	printf("read end up testread value %02x \n",testread);

	free(pageptr1);
	free(pageptr);
	//free(pageptr1);
	free(ptr);

	return 0;
}
static int Handle(int argc, char *argv[]){

	int ret = InitNandTest(argc,argv);
	DeinitNandTest();
	return ret;
}

static char *mycommand="vn";
int GetInterface(PTestFunc *f, char **command){
	
	*f = Handle;
	*command = mycommand;
	return 0;
}
