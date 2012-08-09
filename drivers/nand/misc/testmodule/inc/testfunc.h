#ifndef _TESTFUNC_H_
#define _TESTFUNC_H_

typedef int (*PTestFunc)(int argc, char *argv[]);
typedef int (*PInterface)(PTestFunc *f, char **command);

#endif /* _TESTFUNC_H_ */
