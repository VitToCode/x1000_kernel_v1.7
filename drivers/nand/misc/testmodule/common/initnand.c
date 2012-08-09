#include "testfunc.h"
#include "vnandinfo.h"
#include "nandinterface.h"
#include "pagelist.h"
#include "context.h"
#include "vNand.h"

Context context;
static int g_ret = -1;
extern int start_test_nand(int argc, char *argv[]);
extern void ND_Init(void);

VNandManager *g_vm;

static void start(int handle){
	int ret;
	Context * conptr = (Context * )handle;
 	g_vm = NULL;
	ret = vNand_Init(&g_vm);
	if(ret != 0){
		printf("vNand_Init error  func %s line %d\n",__FUNCTION__,__LINE__);
		return;
	}
	
	CONV_PT_VN(g_vm->pt->ppt,&conptr->vnand);
	
	printf("vnand = %p\n",g_vm);
	g_ret = 0;
}
int InitNandTest(int argc, char *argv[]){
	//Context * conptr = &context;
	Register_StartNand(start,(int)&context);
	ND_Init();
	if(g_ret == 0)
		return start_test_nand(argc,argv);
	return -1;
}
void DeinitNandTest(){
//	Context * conptr = &context;

	vNand_Deinit(&g_vm);
}

