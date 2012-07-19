#ifndef __PARTCONTEXT_H__
#define __PARTCONTEXT_H__

#include "partitioninterface.h"

#define PARTCONTEXT(OBJ) ((PartContext*)OBJ)

#ifndef String
#define String char*
#endif


typedef struct _PartContext PartContext;

struct _PartContext {
  /** Attributes **/
  /*public*/
    int nmhandle;
    PartitionInterface* ptif;
  /** Associations **/
/** Operations **/
};

/** Operations **/

#endif
