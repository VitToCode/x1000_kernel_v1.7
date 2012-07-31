#include "zonememory.h"
#include "nanddebug.h"

#define MEM_MAX_SIZE (4 * 1024)
#define ALIGN4B(x) ((x + 3) / 4 * 4)
#undef DEBUG
#define DEBUG 

#define DEBUG_OUT(x,y...) do{ndprint(1,x,##y);while(1);}while(0)

static int get_bitmapsize(int tsize,int usize){
	int bmisize;
	int count;
	int n = tsize;
	bmisize = 0;
	do{
		n -= bmisize * 4;
		count = n / usize;
		bmisize = (count + 31) / 32;
	}while((count * usize + bmisize * 4) > tsize);
	return bmisize;
}
static ZoneBuffer* NewZoneBuffer(ZoneMemory* z,int usize,void *buf,int size){
	int bmisize,i;
	ZoneBuffer* zb;
	size = size - sizeof(ZoneBuffer);
	usize = ALIGN4B(usize);
	bmisize = get_bitmapsize(size,usize);
	zb = buf;
	zb->head.next = NULL;
	zb->bitmap = (unsigned int *)(zb + 1);
	zb->mBuffer = (void *)(zb->bitmap + bmisize);
	zb->bitsize = (size - bmisize * 4) / usize;
	
	for(i = 0;i < (zb->bitsize + 31) / 32;i++)
		*(zb->bitmap + i) = 0;
	return zb;
}
int ZoneMemory_Init(int unitsize ){

	int zonesize;
	ZoneMemory* z = Nand_VirtualAlloc(MEM_MAX_SIZE);
	zonesize = (MEM_MAX_SIZE - sizeof(ZoneMemory));
#ifdef DEBUG
	unitsize += 8;
#endif
	unitsize = ALIGN4B(unitsize);
	if(z){
		z->top = NewZoneBuffer(z,unitsize,(z+1),zonesize);
		z->top->head.next = NULL;
		z->usize = unitsize;		
		InitNandMutex(&z->mutex);
	}
	return (int) z;
}
static inline int get_mask(int len) {
	int i;
	unsigned int mask = 0;
	for(i = 0;i < len;i++) 
		mask |= (1 << i);
	return mask;
}
static int get_spaceindexs(unsigned int *bitmap,int count,int reqs){
	int i,j,n = 0;
	int mask;
	int d;
	int c;
	mask = get_mask(reqs);
	for(i = 0;i < count / 32;i++){
		if(*bitmap != 0xffffffff){
			if(n){
				c = get_mask(reqs - (32 - n));
				if((*bitmap & c) == 0)
					return ((i - 1) * 32 + n);
				
			}	
			for(j = 0;j < 32 - reqs + 1;j++){
				if((*bitmap & (mask << j)) == 0)
						return (i*32+j);
			}
			n = 0;
			for(;j < 32;j++){
				if((*bitmap & (1 << j)) == 0){
					n = j;
				}
			}
			
		}
		bitmap++;
	}

	if(n){
		c = get_mask(reqs - (32 - n));
		if((*bitmap & c) == 0)
			return ((i - 1) * 32 + n);
	}

	d = (count & 31) - reqs;
	if(d >= 0)
	{
		for(j = 0;j < d;j++){
			if((*bitmap & (mask << j)) == 0)
				return (i * 32 + j);
		}
	}
	return -1;
}

static int get_spacezero(unsigned int *bitmap,int count){
	int i;
	for(i = 0;i < (count + 31) / 32;i++){
		if(*bitmap != 0)
			return -1;
		bitmap++;
	}
	return 0;
}

static void set_bitmaps(unsigned int *bitmap,int index,int reqs){
	unsigned int mask = get_mask(reqs);
	int raw = index / 32;
	int col = index & 31;
	if(col + reqs  - 1 < 32)
		bitmap[raw] |= (mask << col);
	else {
		mask = get_mask(32 - col);
		bitmap[raw] |= (mask << col);
		mask = get_mask(reqs - (32 - col));
		bitmap[raw+1] |= mask;
		
	}

}
static void clear_bitmaps(unsigned int *bitmap,int index,int reqs){
	unsigned int mask = get_mask(reqs);
	int raw = index / 32;
	int col = index & 31;
	if(col + reqs - 1 < 32)
		bitmap[raw] &= ~(mask << col);
	else {
		mask = get_mask(32 - col);
		bitmap[raw] &= ~(mask << col);
		mask = get_mask(reqs - (32 - col));
		bitmap[raw+1] &= ~mask;
		
	}
}

static void free_zonebufferlist(ZoneBuffer* zb){
	struct singlelist *prev = 0;
	struct singlelist *dels = NULL;
	struct singlelist *pz;
	ZoneBuffer *ztmp;
	prev = &zb->head;
	singlelist_for_each(pz,zb->head.next){
		if(dels){
			ztmp = singlelist_entry(dels,ZoneBuffer,head);
			singlelist_del(prev,dels);
			Nand_VirtualFree(ztmp);
			dels = NULL;
		}
		ztmp = singlelist_entry(pz,ZoneBuffer,head);
		if(ztmp->mBuffer == NULL){
			dels = pz;
		}else
			prev = pz;
	}
	if(dels){
		
		ztmp = singlelist_entry(dels,ZoneBuffer,head);
		singlelist_del(prev,dels);
		Nand_VirtualFree(ztmp);
	}
	
}
#if 1
void* ZoneMemory_NewUnits(int zid,int count){
	ZoneMemory* z = (ZoneMemory*)zid;
	ZoneBuffer* zb = z->top,*ztmp,*t;
	struct singlelist *pz,*prev;
	int index;
	int freed = 0;
	void *buf = NULL;
	NandMutex_Lock(&z->mutex);
	prev = &(zb->head);
	singlelist_for_each(pz,&(zb->head)){
		ztmp = singlelist_entry(pz,ZoneBuffer,head);
		if(buf){
			//check use free ztmp;
			
			index = get_spacezero(ztmp->bitmap,ztmp->bitsize);
			if((index == 0) && (z->top != ztmp)){
				//will delete
				ztmp->mBuffer = NULL;
				freed = 1;
			}
		} else {
			index = get_spaceindexs(ztmp->bitmap,ztmp->bitsize,count);
			if( index >= 0 ){
				buf = (void *)((unsigned int)ztmp->mBuffer + index * z->usize);
				set_bitmaps(ztmp->bitmap,index,count);				
			}
		}
		prev = pz;
	}
	if(buf){		
		if(freed)
			free_zonebufferlist(z->top);
	}else{
		
		
		t = (ZoneBuffer*) Nand_VirtualAlloc(MEM_MAX_SIZE);
		if(t == NULL){
			NandMutex_Unlock(&z->mutex);
			return 0;
		}
		ztmp = NewZoneBuffer(z,z->usize,t, MEM_MAX_SIZE);
		singlelist_add(prev,&(ztmp->head));
		buf = ztmp->mBuffer;
		set_bitmaps(ztmp->bitmap,0,count);
	}
#ifdef DEBUG
	{
		unsigned int *v = buf;
		*v = (unsigned int)(v + 2);
		*(v+1) = count;
		NandMutex_Unlock(&z->mutex);
		return (void *)(v + 2);
	}
#endif
	NandMutex_Unlock(&z->mutex);
	return buf;
}
#else
void* ZoneMemory_NewUnits(int zid,int count){
	ZoneMemory* z = (ZoneMemory*)zid;
	return Nand_VirtualAlloc(z->usize * count);
}
#endif

void* ZoneMemory_NewUnit (int zid){
	return ZoneMemory_NewUnits(zid,1);
}
#if 1
void ZoneMemory_DeleteUnits(int zid,void* pu,int count){
#ifdef DEBUG
	int release = 0;
#endif

	int index;
	ZoneMemory *z = (ZoneMemory*)zid;
	ZoneBuffer *ztmp;
	struct singlelist *pz;
	NandMutex_Lock(&z->mutex);

	singlelist_for_each(pz,&(z->top->head)){
		ztmp = singlelist_entry(pz,ZoneBuffer,head);
		if(((unsigned int)pu >= (unsigned int)ztmp->mBuffer) && ((unsigned int)pu < (unsigned int)ztmp->mBuffer + ztmp->bitsize * z->usize)){
			
			index = (unsigned int)pu - (unsigned int)ztmp->mBuffer;
			index = index / z->usize;
#ifdef DEBUG
			{
				unsigned int *v = pu;
				if(*(v-2) != (unsigned int)pu)
					DEBUG_OUT("free address error address expected 0x%08x but %p\n",*(v-2),pu);
				if(*(v-1) != count)
					DEBUG_OUT("free count error count expected %d but %d\n",*(v-1),count);
				release = 1;
				*(v - 1) = 0;
				*(v-2) = 0;
			}
#endif
			clear_bitmaps(ztmp->bitmap,index,count);
			break;
		}

	}
#ifdef DEBUG
	if(release == 0)
		DEBUG_OUT("not find address %p\n",pu);
#endif
	NandMutex_Unlock(&z->mutex);
}
#else
void ZoneMemory_DeleteUnits(int zid,void* pu,int count){
	Nand_VirtualFree(pu);
}
#endif
void ZoneMemory_DeleteUnit(int zid,void* pu ){
	ZoneMemory_DeleteUnits(zid,pu,1);
}

void ZoneMemory_DeInit(int zid){
	ZoneMemory *z = (ZoneMemory*)zid;
	struct singlelist *prev,*pz_j;
	ZoneBuffer *ztmp;
	prev = &z->top->head;
	pz_j = z->top->head.next;
	while(pz_j){
		prev = pz_j;
		pz_j = pz_j->next;
		
		ztmp = singlelist_entry(prev,ZoneBuffer,head);
		singlelist_del(prev,prev);
		Nand_VirtualFree(ztmp);		
	}
	DeinitNandMutex(&z->mutex);
	Nand_VirtualFree(z);
}

