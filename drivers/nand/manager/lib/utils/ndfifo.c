#include "utils/ndfifo.h"
int createfifo(void) {
	int zm;
	TmpTableList *tmp;
	zm = ZoneMemory_Init (sizeof(TmpTableNode));
	tmp = ZoneMemory_NewUnits(zm,(sizeof(TmpTableList) + sizeof(TmpTableNode) - 1) / sizeof(TmpTableNode));
	tmp->zm = zm;
	tmp->fifocount = 0;
	INIT_BILIST_HEAD(&tmp->top);
	return (int)tmp;
}
void releasefifo(int handle) {
	int zm;
	TmpTableList *tmp = (TmpTableList *)handle;
	TmpTableNode *node;
	struct bilist_head *pos;
	zm = tmp->zm;
	bilist_for_each(pos,&(tmp->top)) {
		node = bilist_entry(pos,TmpTableNode,head);
		__bilist_del(pos->prev,pos->next);
		ZoneMemory_DeleteUnits(zm,node,1);
	}
	ZoneMemory_DeInit(zm);
}

int pushfifo(int handle,unsigned int data) {
	TmpTableList *tmp = (TmpTableList *)handle;
	TmpTableNode *node;
	node = ZoneMemory_NewUnits(tmp->zm,1);
	if(node == NULL) return -1;
	node->data = data;
	bilist_add(&node->head,&tmp->top);
	tmp->fifocount++;
	return 0;
}
unsigned int popfifo(int handle) {
	TmpTableList *tmp = (TmpTableList *)handle;
	TmpTableNode *node;
	unsigned int data = -1;
	if(tmp->top.prev) {
		node = bilist_entry(tmp->top.prev,TmpTableNode,head);
		data = node->data;
		bilist_del(tmp->top.prev);
		tmp->fifocount--;
		if(tmp->fifocount < 0)
			tmp->fifocount = 0;
	}
	return data;
}
int fifocount(int handle) {
	TmpTableList *tmp = (TmpTableList *)handle;
	return tmp->fifocount;
}

int fifodelete(int handle,unsigned int data) {
	TmpTableList *tmp = (TmpTableList *)handle;
	struct bilist_head *pos;
	TmpTableNode *node;
	int zm = tmp->zm;
	int ret = -1;
	bilist_for_each(pos,&(tmp->top)) {
		node = bilist_entry(pos,TmpTableNode,head);
		if(node->data == data) {
			bilist_del(pos);
			ZoneMemory_DeleteUnits(zm,node,1);
			ret = 0;
			tmp->fifocount--;
			break;
		}
	}
	return ret;
}
