#ifndef __ERRHANDLE_H__
#define __ERRHANDLE_H__

typedef struct _ErrInfo ErrInfo;
struct _ErrInfo {
	unsigned short err_zoneid;
	int context;
};

int read_page0_err_handler(int errinfo);
int read_page1_err_handler(int errinfo);
int read_page2_err_handler(int errinfo);
int read_ecc_err_handler(int errinfo);
int read_first_pageinfo_err_handler(int errinfo);

#endif
