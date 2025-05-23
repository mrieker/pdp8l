CFLAGS ?= -O2 -Werror
MACH   := $(shell uname -m)
TCLINC := -I$(shell ./tclinc.sh)
GPP    := g++ $(CFLAGS) $(TCLINC) -std=c++11 -g -Wall -fPIC -DFPPCB=$(FPPCB)
LNKFLG := -lpthread -lreadline -lrt -l$(shell ./tcllib.sh)

ifeq ($(MACH),armv6l)
	UNIPROC := 1
else
	UNIPROC := 0
endif

ifeq ($(MACH),armv7l)
	LNKFLG := $(LNKFLG) -latomic
endif

ifneq (,$(wildcard /usr/include/pulse/simple.h))
	HASPA := 1
	PALIB := -lpulse-simple
else
	HASPA := 0
	PALIB :=
endif

LIBS = lib.$(MACH).a

default: mcp23017.$(MACH) pipan8l.$(MACH) z8lcmemtest.$(MACH) z8lcore.$(MACH) z8ldmaloop.$(MACH) z8ldump.$(MACH) \
	z8lkbjam.$(MACH) z8lila.$(MACH) z8lmctrace.$(MACH) z8lpanel.$(MACH) z8lpbit.$(MACH) z8lpiotest.$(MACH) \
	z8lptp.$(MACH) z8lptr.$(MACH) z8lreal.$(MACH) z8lrk8je.$(MACH) \
	z8lsimtest.$(MACH) z8ltc08.$(MACH) z8ltrace.$(MACH) z8ltty.$(MACH) z8lvc8.$(MACH) z8lxmemtest.$(MACH)

lib.$(MACH).a: \
		assemble.$(MACH).o \
		cmd_pin.$(MACH).o \
		disassemble.$(MACH).o \
		i2clib.$(MACH).o \
		i2czlib.$(MACH).o \
		readprompt.$(MACH).o \
		simlib.$(MACH).o \
		tclmain.$(MACH).o \
		z8lutil.$(MACH).o
	rm -f lib.$(MACH).a
	ar rc $@ $^

binloader.rim: binloader.pal
	pdp8v/asm/assemble -pal binloader.pal binloader.obj > binloader.lis
	pdp8v/asm/link -o binloader.oct binloader.obj > binloader.map
	pdp8v/asm/octtorim < binloader.oct > binloader.rim

dttest.bin: dttest.asm
	pdp8v/asm/assemble dttest.asm dttest.obj > dttest.lis
	pdp8v/asm/link -o dttest.oct dttest.obj > dttest.map
	pdp8v/asm/octtobin < dttest.oct > dttest.bin

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

z8lmctrace.$(MACH): z8lmctrace.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ -lpthread

z8lpanel.$(MACH): z8lpanel.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ $(LNKFLG)

z8lpbit.$(MACH): z8lpbit.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ $(LNKFLG) $(PALIB)

z8lpiotest.$(MACH): z8lpiotest.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ $(LNKFLG)

z8lptp.$(MACH): z8lptp.$(MACH).o $(LIBS)
	$(GPP) -o $@ $^ $(LNKFLG)

z8lptr.$(MACH): z8lptr.$(MACH).o $(LIBS)
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
	$(GPP) -DHASPA=$(HASPA) -DUNIPROC=$(UNIPROC) -c -o $@ $<

doubleroll-8l.bin: doubleroll-8l.asm
	pdp8v/asm/assemble doubleroll-8l.asm doubleroll-8l.obj > doubleroll-8l.lis
	pdp8v/asm/link -o doubleroll-8l.oct doubleroll-8l.obj > doubleroll-8l.map
	pdp8v/asm/octtobin < doubleroll-8l.oct > doubleroll-8l.bin

kaleid-e.bin: kaleid-e.pal kaleid.pal
	pdp8v/asm/assemble -pal kaleid-e.pal kaleid-e.obj > kaleid-e.lis
	pdp8v/asm/link -o kaleid-e.oct kaleid-e.obj > kaleid-e.map
	pdp8v/asm/octtobin < kaleid-e.oct > kaleid-e.bin

kaleid-i.bin: kaleid-i.pal kaleid.pal
	pdp8v/asm/assemble -pal kaleid-i.pal kaleid-i.obj > kaleid-i.lis
	pdp8v/asm/link -o kaleid-i.oct kaleid-i.obj > kaleid-i.map
	pdp8v/asm/octtobin < kaleid-i.oct > kaleid-i.bin

