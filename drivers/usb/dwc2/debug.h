#ifndef __DRIVERS_USB_DWC2_DEBUG_H
#define __DRIVERS_USB_DWC2_DEBUG_H

#include "core.h"

#ifdef CONFIG_DEBUG_FS
extern int dwc2_debugfs_init(struct dwc2 *);
extern void dwc2_debugfs_exit(struct dwc2 *);
#else
static inline int dwc2_debugfs_init(struct dwc2 *d)
{  return 0;  }
static inline void dwc2_debugfs_exit(struct dwc2 *d)
{  }
#endif

#define DWC2_REQ_TRACE_STAGE_QUEUED	1
#define DWC2_REQ_TRACE_STAGE_TRANS	2
#define DWC2_REQ_TRACE_STAGE_FINI	3

#ifdef CONFIG_USB_DWC2_REQ_TRACER
void dwc2_req_trace_record(struct dwc2_request *req, u8 stage);
#else
#define dwc2_req_trace_record(r, s) do {  } while(0)
#endif

#endif	/* __DRIVERS_USB_DWC2_DEBUG_H */
