HDRS = ../hdrs
LIB  = libcomm.a
OBJS = admin.o com.o create.o give.o look.o move.o set.o speech.o

include ../config.in

$(LIB): $(OBJS)
	rm -f $(LIB)
	ar qcs $(LIB) $(OBJS)

clean:
	rm -f $(LIB) $(OBJS)
