HDRS = ../hdrs
include ../config.in

DIR  = ../../bin
OBJS = config.o crypt.o rand.o stats.o timer.o utils.o version.o
BIN  = netmare$(CYGWIN)

$(BIN): $(OBJS) $(LIBS)
	$(CC) -o $(BIN) $(OBJS) $(LIBS) $(LDLIBS) $(LDFLAGS)

install: $(BIN)
	@rm -f .version
	@if [ ! -d $(DIR) ]; then rm -rf $(DIR);mkdir $(DIR); fi
	@if [ -f $(DIR)/$(BIN) ]; then mv -f $(DIR)/$(BIN) $(DIR)/$(BIN)~; fi
	cp -f $(BIN) $(DIR)

clean:
	rm -f $(BIN) $(OBJS)

config.o: $(HDRS)/config.h
