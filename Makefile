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

tgz: all
	@mkdir -p build
	@cp lib/libmyrpc.a build/
	@cp lib/myrpc_common.h build/
	@cp client/myrpc-client build/
	@cp server/myrpc-server build/
	@cp -r config build/
	@cp README.md build/
	@cp -r client/README.md build/client_README.md
	@cp -r server/README.md build/server_README.md
	@tar -czvf myrpc-$(shell date +%Y%m%d).tar.gz -C build .
	@rm -rf build/
	@ls -lh *.tar.gz