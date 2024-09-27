CFLAGS ?= -O2 -Werror
MACH   := $(shell uname -m)
TCLINC := -I$(shell ./tclinc.sh)
GPP    := g++ $(CFLAGS) $(TCLINC) -std=c++11 -g -Wall -fPIC
LNKFLG := -lpthread -lreadline -lrt -ltcl

ifeq ($(MACH),armv6l)
	UNIPROC := 1
else
	UNIPROC := 0
endif

ifeq ($(MACH),armv7l)
	LNKFLG := $(LNKFLG) -latomic
endif

LIBS = lib.$(MACH).a

default: mcp23017.$(MACH) pipanel.$(MACH) ttpanel.$(MACH)

lib.$(MACH).a: \
		abcd.$(MACH).o \
		assemble.$(MACH).o \
		disassemble.$(MACH).o \
		i2clib.$(MACH).o \
		readprompt.$(MACH).o \
		simlib.$(MACH).o
	rm -f lib.$(MACH).a
	ar rc $@ $^

mcp23017.$(MACH): mcp23017.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ $(LNKFLG)

pipanel.$(MACH): pipanel.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ $(LNKFLG)

ttpanel.$(MACH): ttpanel.$(MACH).o
	$(GPP) -o $@ $^ -lpthread

%.$(MACH).o: %.cc *.h
	$(GPP) -DUNIPROC=$(UNIPROC) -c -o $@ $<

