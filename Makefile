SUBDIRS = lib client server

.PHONY: all clean deb $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	@for d in $(SUBDIRS); do $(MAKE) -C $$d clean; done

deb: all
	$(MAKE) -C client deb
	$(MAKE) -C server deb
	@ls -lh client/*.deb server/*.deb