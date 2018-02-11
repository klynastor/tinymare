HDRS = ../hdrs
LIB  = libprog.a
OBJS = boolexp.o ctrl.o eval.o hash.o

include ../config.in

$(LIB): $(OBJS)
	rm -f $(LIB)
	ar qcs $(LIB) $(OBJS)

clean:
	rm -f $(LIB) $(OBJS)
