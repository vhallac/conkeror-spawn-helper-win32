# Wedit Makefile for project conkerorspawnhelper
SRCDIR=f:\tools\conkeror\win32
CFLAGS=-I"c:\tools\lcc\include"  -O  -A  -unused
CC=$(LCCROOT)\bin\lcc.exe
LINKER=$(LCCROOT)\bin\lcclnk.exe
OBJS=\
	conkeror-spawn-helper.obj

LIBS=
EXE=conkeror-spawn-helper.exe

$(EXE):	$(OBJS) Makefile
	$(LINKER)  -s -subsystem windows -map f:\tools\conkeror\win32\lcc\conkeror-spawn-helper.map -o $(SRCDIR)\lcc\conkeror-spawn-helper.exe $(OBJS) $(LIBS)

# Build conkeror-spawn-helper.c
CONKEROR-SPAWN-HELPER_C=\

conkeror-spawn-helper.obj: $(CONKEROR-SPAWN-HELPER_C) $(SRCDIR)\conkeror-spawn-helper.c
	$(CC) -c $(CFLAGS) $(SRCDIR)\conkeror-spawn-helper.c

link:
	$(LINKER)  -s -subsystem windows -map f:\tools\conkeror\win32\lcc\conkeror-spawn-helper.map -o $(SRCDIR)\lcc\conkeror-spawn-helper.exe $(OBJS) $(LIBS)

clean:
	del $(OBJS) conkeror-spawn-helper.exe
