OS := $(shell uname)
ifeq ($(OS),Darwin)
	CXX=clang++
	CC=clang
else
	CXX=g++
	CC=gcc
endif

BIN=fatboy

SRCS=$(wildcard *.c) elmchan/src/ff.c elmchan/src/diskio.c elmchan/src/option/unicode.c
OBJS=$(patsubst %.c,%.o,$(SRCS))
CFLAGS=-g -O3 --std=c11 -MP -MMD
all: $(BIN)

%.o: %.cpp
	$(info [ CXX ] $<)
	@$(CXX) -c -o $@ $< $(CXXFLAGS)

%.o: %.c
	$(info [ CC  ] $<)
	@$(CC) -c -o $@ $< $(CFLAGS)

$(BIN): $(OBJS)
	$(info [ LNK ] $@)
	@$(CXX) -o $(BIN) $(OBJS) $(LDFLAGS) $(CXXFLAGS)

.PHONY: clean
clean:
	$(info [CLEAN])
	@rm -f $(BIN) $(OBJS) $(OBJS:.o=.d)

-include $(OBJS:.o=.d)
