CC       ?= gcc
CFLAGS   ?= -Wall -Wextra -Werror -pedantic -std=gnu11 -O2
CPPFLAGS ?= -I../lib
LDFLAGS  ?= -L../lib
LDLIBS   ?= -lmyrpc

TARGET   = myrpc-client
SRCS     = myrpc_client.c
OBJS     = $(SRCS:.c=.o)

PKG_DIR  = package
PKG_NAME = myrpc-client_1.0_amd64.deb

.PHONY: all clean deb

all: $(TARGET)

$(TARGET): $(OBJS) ../lib/libmyrpc.a
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

%.o: %.c ../lib/myrpc_common.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

deb: all
	@rm -rf $(PKG_DIR)/usr $(PKG_DIR)/etc
	@mkdir -p $(PKG_DIR)/usr/bin
	@cp $(TARGET) $(PKG_DIR)/usr/bin/
	@SIZE=$$(du -sk $(PKG_DIR)/usr | cut -f1); \
	sed -i "s/^Installed-Size:.*/Installed-Size: $$SIZE/" \
		$(PKG_DIR)/DEBIAN/control
	@dpkg-deb --build $(PKG_DIR) $(PKG_NAME)

clean:
	rm -f $(OBJS) $(TARGET) $(PKG_NAME)
	rm -rf $(PKG_DIR)/usr $(PKG_DIR)/etc