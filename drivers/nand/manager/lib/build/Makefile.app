
CFLAGS += -pthread -lrt -ldl
CFLAGS += -DNO_ERROR

LIB = libnm.a

$(LIB):$(lib_obj)
	$(AR) $(ARFLAG) $@ $^
