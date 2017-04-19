BIN=fatboy
CXX=clang++
CC=clang
SRCS=$(wildcard *.c) elmchan/src/ff.c elmchan/src/diskio.c elmchan/src/option/unicode.c
OBJS=$(patsubst %.c,%.o,$(SRCS))
#LDFLAGS=
CFLAGS=-g --std=c11 -MP -MMD
all: $(BIN)

%.o: %.cpp
	$(info [ CXX ] $<)
	@$(CXX) -c -o $@ $< $(CXXFLAGS)

$.o: $.c
	$(info [ CC  ] $<)
	@$(CC) -c -o $@ $< $(CFLAGS)

$(BIN): $(OBJS)
	$(info [ LNK ] $@)
	@$(CXX) -o $(BIN) $(OBJS) $(LDFLAGS) $(CXXFLAGS)

.PHONY: clean
clean:
	rm -f $(BIN) $(OBJS)

-include $(OBJS:.o=.d)
