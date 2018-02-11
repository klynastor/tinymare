HDRS = ./hdrs
include config.in

all: config.in version
	@(for i in $(SUBDIRS); do (cd $$i;$(MAKE)); \
	  if [ "$$?" != "0" ]; then exit 1; fi; done)

$(SUBDIRS): dummy
	@(cd $@;$(MAKE))

config.in:
	./configure

version:
	@if [ ! -f mare/.version ]; then \
	  ./mkvers; touch mare/.version; fi

install: all
	@(cd mare;$(MAKE) install)
	@./dirsize

clean:
	rm -f */*.[oa]
	@(cd mare;$(MAKE) clean)

distclean: clean
	rm -f config.in .config

dummy: # Always force subdirectories.
