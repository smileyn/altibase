# $Id: Makefile 29457 2008-11-20 05:24:42Z ksjall $ #

# �����ؾ� ��  ����
#  DESTDIR  : �׽��� ���� ���丮
#  BINS     : ����ȭ�ϸ�
#  BINSRCS  : ����ȭ�� �ҽ�
#  run.sh   : �׽�Ʈ ������ �ҽ� �ڵ�

include ../tsmEnv.mk

DESTDIR = $(TSM_DIR)/tsm_test
BINS =  tsm_test tsm_test_multi

RUN_SHELL= run.sh
BINSRCS1 = tsm_test.cpp
BINSRCS2 = tsm_test_multi.cpp
BINOBJS1 = $(BINSRCS1:.cpp=.$(OBJEXT))
BINOBJS2 = $(BINSRCS2:.cpp=.$(OBJEXT))
cmd_ld = $(LD) $(LFLAGS) $(LDOUT)$@ $^ $(TSMLIB) $(SMLIB) $(ID_LIB) $(LIBS)

all: $(COMPILE) install

compile:  $(OBJS) archive $(BINS)

install: $(BINS)
	$(RM) $(DESTDIR)
	mkdir $(DESTDIR)
	cp $(BINS) $(RUN_SHELL) $(DESTDIR)

tsm_test: $(BINOBJS1)  
	$(cmd_ld)

tsm_test_multi: $(BINOBJS2)
	$(cmd_ld)

clean:
	$(RM) core .dependency $(BINS) *.$(OBJEXT) *.d SunWS_cache *~ ./*.*~

dep:
	-$(RM) ./.dependency
	$(CXX) $(CXXOPT_DEPENDANCY) $(INCLUDES) $(DEFINES) $(BINSRCS) > .dependency

ifeq (./.dependency,$(wildcard ./.dependency))
include ./.dependency
endif
