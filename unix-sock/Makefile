# ------------------------------------------------
# A Generic Makefile for Building Multiple Targets
#
# Author: yangsheng1107@gmail.com
# Date  : 2016-12-22
#
# Changelog :
#   0.01 - first version
# ------------------------------------------------
LDFLAGS_PATH := 
LDFLAGS := -lpthread $(LDFLAGS_PATH)
CFLAGS_INC := 
CFLAGS := -g -Wall $(CFLAGS_INC)

SRCS := $(wildcard *.c)
OBJS := $(patsubst %.c,%.o,$(SRCS))
BINS := $(patsubst %.c,%,$(SRCS))
#$(info $$SRCS is [$(SRCS)])
#$(info $$OBJS is [$(OBJS)])
#$(info $$BINS is [$(BINS)])

.PHONY : clean all

all: $(BINS)

$(BINS): %: %.o
#	$(info $$@ is [$@], and $$< is [$<])
	$(CC) $< $(LDFLAGS) -o $@

$(OBJS): %.o: %.c
#	$(info $$@ is [$@], and $$< is [$<])
	$(CC) $(CFLAGS) -c $< $(LDFLAGS) -o $@

clean:
	@rm -f $(OBJS) $(BINS)
