#ifndef __LPARTITION_H__
#define __LPARTITION_H__

#define LPARTITION(OBJ) ((LPartition*)OBJ)

#ifndef String
#define String char*
#endif

typedef struct _LPartition LPartition;

struct _LPartition {
  /** Attributes **/
  /*public*/
	struct singlelist head; 
	int startSector;
    int sectorCount;
    char* name;
    int mode;
    //PartContext* pc;
  /** Associations **/
/** Operations **/
};

/** Operations **/
#endif
