HDRS = ../hdrs
LIB  = libdb.a
OBJS = attrib.o compress.o convert.o destroy.o inherit.o load.o runtime.o \
       save.o

include ../config.in

$(LIB): $(OBJS)
	rm -f $(LIB)
	ar qcs $(LIB) $(OBJS)

clean:
	rm -f $(LIB) $(OBJS)

attrib.o: $(HDRS)/attrib.h
