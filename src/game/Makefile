HDRS = ../hdrs
LIB  = libgame.a
OBJS = command.o help.o match.o player.o powers.o predicates.o queue.o \
       unparse.o

include ../config.in

$(LIB): $(OBJS)
	rm -f $(LIB)
	ar qcs $(LIB) $(OBJS)

clean:
	rm -f $(LIB) $(OBJS)
