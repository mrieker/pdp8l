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

default: mcp23017.$(MACH) pipan8l.$(MACH) z8lcmemtest.$(MACH) z8lcore.$(MACH) z8ldmaloop.$(MACH) z8ldump.$(MACH) \
	z8lkbjam.$(MACH) z8lila.$(MACH) z8lpin.$(MACH) z8lpiotest.$(MACH) z8lreal.$(MACH) z8lrk8je.$(MACH) \
	z8lsimtest.$(MACH) z8ltc08.$(MACH) z8ltrace.$(MACH) z8ltty.$(MACH) z8lvc8.$(MACH) z8lxmemtest.$(MACH)

lib.$(MACH).a: \
		assemble.$(MACH).o \
		disassemble.$(MACH).o \
		i2clib.$(MACH).o \
		readprompt.$(MACH).o \
		simlib.$(MACH).o \
		tclmain.$(MACH).o \
		z8llib.$(MACH).o \
		z8lutil.$(MACH).o
	rm -f lib.$(MACH).a
	ar rc $@ $^

mcp23017.$(MACH): mcp23017.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ $(LNKFLG)

pipan8l.$(MACH): pipan8l.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ $(LNKFLG)

z8lcmemtest.$(MACH): z8lcmemtest.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ -lpthread

z8lcore.$(MACH): z8lcore.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ $(LNKFLG)

z8ldmaloop.$(MACH): z8ldmaloop.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ -lpthread

z8ldump.$(MACH): z8ldump.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ -lpthread

z8lkbjam.$(MACH): z8lkbjam.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ $(LNKFLG)

z8lila.$(MACH): z8lila.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ -lpthread

z8lpin.$(MACH): z8lpin.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ $(LNKFLG)

z8lpiotest.$(MACH): z8lpiotest.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ $(LNKFLG)

z8lreal.$(MACH): z8lreal.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ $(LNKFLG)

z8lrk8je.$(MACH): z8lrk8je.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ $(LNKFLG)

z8lsimtest.$(MACH): z8lsimtest.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ -lpthread

z8ltc08.$(MACH): z8ltc08.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ $(LNKFLG)

z8ltrace.$(MACH): z8ltrace.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ -lpthread

z8ltty.$(MACH): z8ltty.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ $(LNKFLG)

z8lvc8.$(MACH): z8lvc8.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ -lX11

z8lxmemtest.$(MACH): z8lxmemtest.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ -lpthread

%.$(MACH).o: %.cc *.h
	$(GPP) -DUNIPROC=$(UNIPROC) -c -o $@ $<

kaleid-e.bin: kaleid-e.pal kaleid.pal
	pdp8v/asm/assemble -pal kaleid-e.pal kaleid-e.obj > kaleid-e.lis
	pdp8v/asm/link -o kaleid-e.oct kaleid-e.obj > kaleid-e.map
	pdp8v/asm/octtobin < kaleid-e.oct > kaleid-e.bin

kaleid-i.bin: kaleid-i.pal kaleid.pal
	pdp8v/asm/assemble -pal kaleid-i.pal kaleid-i.obj > kaleid-i.lis
	pdp8v/asm/link -o kaleid-i.oct kaleid-i.obj > kaleid-i.map
	pdp8v/asm/octtobin < kaleid-i.oct > kaleid-i.bin

