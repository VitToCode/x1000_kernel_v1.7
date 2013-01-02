#ifndef _NDFIFO_H_
#define _NDFIFO_H_
#include <bilist.h>
#include <zonememory.h>

typedef struct _TmpTableNode TmpTableNode;
typedef struct _TmpTableList TmpTableList;

struct _TmpTableNode
{
	struct bilist_head head;
	unsigned int data;
};
struct _TmpTableList
{
	int zm;
	int fifocount;
	struct bilist_head top;
};
int createfifo(void);
void releasefifo(int handle);
int pushfifo(int handle,unsigned int data);
unsigned int popfifo(int handle);
int fifocount(int handle);

#endif /* _NDFIFO_H_ */

