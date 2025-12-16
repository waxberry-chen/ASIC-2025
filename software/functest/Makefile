SRC_DIR := src
NAMES ?= $(basename $(notdir $(wildcard $(SRC_DIR)/*.c)))
SRCS := $(addprefix $(SRC_DIR)/, $(NAMES))
BASE_PORT := $(abspath ../base-port)
SIM_PATH = $(abspath ../../simulator)
.PHONY: all
all: $(NAMES)

# Build rule set in ../base-port/Makefile
# Support make all test funcs
%: src/%.c
	@printf "NAMES:=%s\nSRCS:=%s\nBASE_PORT:=%s\nSIM_PATH:=%s\ninclude %s/Makefile\n" "$@" "$(basename $<)" "$(BASE_PORT)" "$(SIM_PATH)" "$(BASE_PORT)" > $@.mk
	@make -s -f $@.mk run
	@rm -rf $@.mk

run: all

clean:
	@rm -rf build