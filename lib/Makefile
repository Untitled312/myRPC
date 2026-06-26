CC       ?= gcc
CFLAGS   ?= -Wall -Wextra -Werror -pedantic -std=gnu11 -O2 -fPIC
AR       ?= ar

TARGET   = libmyrpc.a
SRCS     = myrpc_common.c
OBJS     = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) rcs $@ $^

%.o: %.c myrpc_common.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)