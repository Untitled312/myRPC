CC       ?= gcc
CFLAGS   ?= -Wall -Wextra -Werror -pedantic -std=gnu11 -O2
CPPFLAGS ?= -I../lib
LDFLAGS  ?= -L../lib
LDLIBS   ?= -lmyrpc

TARGET   = myrpc-server
SRCS     = myrpc_server.c
OBJS     = $(SRCS:.c=.o)

PKG_DIR  = package
PKG_NAME = myrpc-server_1.0_amd64.deb

.PHONY: all clean deb

all: $(TARGET)

$(TARGET): $(OBJS) ../lib/libmyrpc.a
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

%.o: %.c ../lib/myrpc_common.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

deb: all
	@rm -rf $(PKG_DIR)/usr $(PKG_DIR)/etc $(PKG_DIR)/lib \
	       $(PKG_DIR)/run $(PKG_DIR)/var
	@mkdir -p $(PKG_DIR)/usr/bin
	@mkdir -p $(PKG_DIR)/etc/myrpc
	@mkdir -p $(PKG_DIR)/lib/systemd/system
	@mkdir -p $(PKG_DIR)/run/myrpc-server
	@mkdir -p $(PKG_DIR)/var/log
	@cp $(TARGET) $(PKG_DIR)/usr/bin/
	@cp ../config/myrpc.conf $(PKG_DIR)/etc/myrpc/myrpc.conf
	@cp ../config/users.conf $(PKG_DIR)/etc/myrpc/users.conf
	@cp ../config/myrpc-server.service \
		$(PKG_DIR)/lib/systemd/system/myrpc-server.service
	@touch $(PKG_DIR)/var/log/myrpc.log
	@SIZE=$$(du -sk $(PKG_DIR)/usr $(PKG_DIR)/etc $(PKG_DIR)/lib \
	        | tail -1 | cut -f1); \
	sed -i "s/^Installed-Size:.*/Installed-Size: $$SIZE/" \
		$(PKG_DIR)/DEBIAN/control
	@dpkg-deb --build $(PKG_DIR) $(PKG_NAME)

clean:
	rm -f $(OBJS) $(TARGET) $(PKG_NAME)
	rm -rf $(PKG_DIR)/usr $(PKG_DIR)/etc $(PKG_DIR)/lib \
	       $(PKG_DIR)/run $(PKG_DIR)/var