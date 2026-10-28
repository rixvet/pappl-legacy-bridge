include ../Makedefs

ifeq ($(MAKECMDGOALS),NEC)
 CFLAGS	:=	$(CFLAGS) -DNEC
endif

TARGETS	=	FXM_PF
NEC_TARGETS	=	NCM_PF

OBJS	=	fxprefilter.o paramlist.o buflist.o

all:	$(TARGETS)
NEC:	$(NEC_TARGETS)

clean:
	$(RM) $(OBJS) $(TARGETS) $(NEC_TARGETS)


FXM_PF:fxprefilter.o paramlist.o buflist.o ../Makedefs ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ fxprefilter.o paramlist.o buflist.o $(LINKCUPS)


NCM_PF:fxprefilter.o paramlist.o buflist.o ../Makedefs ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ fxprefilter.o paramlist.o buflist.o $(LINKCUPS)
