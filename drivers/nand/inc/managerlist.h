#ifndef __MANAGERLIST_H__
#define __MANAGERLIST_H__

#include "partitioninterface.h"
#include "singlelist.h"
#include "partitioninterface.h"

typedef struct _ManagerList ManagerList;

struct _ManagerList {
  /** Attributes **/
  /*public*/
    int mode;
    PartitionInterface* nmi;
    struct singlelist head;
  /** Associations **/
/** Operations **/
};

/** Operations **/

#endif
