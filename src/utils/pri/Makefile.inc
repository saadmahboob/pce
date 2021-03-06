# src/utils/pri/Makefile.inc

rel := src/utils/pri

DIRS += $(rel)
DIST += $(rel)/Makefile.inc

PRI_BAS := main
PRI_SRC := $(foreach f,$(PRI_BAS),$(rel)/$(f).c)
PRI_OBJ := $(foreach f,$(PRI_BAS),$(rel)/$(f).o)
PRI_HDR := $(foreach f,$(PRI_BAS),$(rel)/$(f).h)
PRI_MAN1 := $(rel)/pri.1
PRI_BIN := $(rel)/pri$(EXEEXT)

PRI_OBJ_EXT := \
	src/lib/getopt.o \
	src/drivers/pri/pri.a \
	src/drivers/psi/psi.a

BIN  += $(PRI_BIN)
MAN1 += $(PRI_MAN1)
CLN  += $(PRI_BIN) $(PRI_OBJ)
DIST += $(PRI_SRC) $(PRI_HDR) $(PRI_MAN1)

$(rel)/main.o: $(rel)/main.c

$(rel)/pri$(EXEEXT): $(PRI_OBJ_EXT) $(PRI_OBJ)
	$(QP)echo "  LD     $@"
	$(QR)$(LD) $(LDFLAGS_DEFAULT) -o $@ $(PRI_OBJ) $(PRI_OBJ_EXT)
