# TODO: dist should depend on rel & clean

# set default shell
SHELL := sh.exe

# needed in sub-makefiles
ifndef SWOS
export SWOS := f:\\games\\swos
endif

# variables used only here
STORE_DIR = c:\\backup\\swos\\swospp
#WEB_FILES_DIR = g:\usr\home_page\files
ZIP = "c:\Program Files\Winzip\winzip32.exe"

OBJ_DIR  := obj
BIN_DIR  := bin
DIST_DIR := dist
VAR_DIR  := var
DBG_DIR  := $(OBJ_DIR)/dbg
REL_DIR  := $(OBJ_DIR)/rel

$(shell mkdir -p $(BIN_DIR))
$(shell mkdir -p $(VAR_DIR))

all : rel dbg

dbg : $(BIN_DIR)/loader.bin $(BIN_DIR)/patchit.com
	@cd pe2bin && $(MAKE)
	@cd mapcvt && $(MAKE)
	@cd patch && $(MAKE)
	@cd loader && $(MAKE)
	@cd main && perl Makefile.pl dbg
	@cp -f -u $(BIN_DIR)/swospp.bin $(SWOS)
	@cp -f -u $(BIN_DIR)/loader.bin $(SWOS)
	@cp -f -u $(BIN_DIR)/patchit.com $(SWOS)

rel : $(BIN_DIR)/loader.bin $(BIN_DIR)/patchit.com
	@cd pe2bin && $(MAKE)
	@cd mapcvt && $(MAKE)
	@cd patch && $(MAKE)
	@cd loader && $(MAKE)
	@cd main && perl Makefile.pl rel
	@cp -f -u $(BIN_DIR)/swospp.bin $(SWOS)
	@cp -f -u $(BIN_DIR)/loader.bin $(SWOS)
	@cp -f -u $(BIN_DIR)/patchit.com $(SWOS)

$(BIN_DIR)/loader.bin :
	@cd loader && $(MAKE)
	@cp -f $(BIN_DIR)/loader.bin $(SWOS)

$(BIN_DIR)/patchit.com :
	@cd patch && $(MAKE)
	@cp -f $(BIN_DIR)/patchit.com $(SWOS)

dist : rel $(DIST_DIR)/swospp.zip

$(DIST_DIR)/swospp.zip : README.md LICENSE.txt \
                         $(BIN_DIR)\\patchit.com $(BIN_DIR)\\loader.bin \
                         $(BIN_DIR)\\swospp.bin
	@echo Creating distributable archive...
	@$(ZIP) -min -a -ex $(DIST_DIR)/swospp.zip $?

store :
	@echo Making backup...
	@rm -rf $(STORE_DIR)/*
	@cp -r -p * --target-directory=$(STORE_DIR)

.PHONY : all dbg rel dist store clean
