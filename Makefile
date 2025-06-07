CC = cc

BUILD ?= build

ifeq ($(BUILD),)
$(error "Define BUILD to a non-empty value")
endif

SRC = src/main.c src/file.c src/vset.c \
      src/util/hash.c src/util/parse.c
OBJ = $(SRC:src/%.c=$(BUILD)/%.o)
DEP = $(OBJ:.o=.d)
EXE = $(BUILD)/arquivos
ZIP = $(BUILD)/arquivos.zip
GEN = $(BUILD)/.gitignore

BASECFLAGS = -O2 -MMD -std=gnu11 -Iinclude -Wall $(CFLAGS)
BASELDFLAGS = $(LDFLAGS)

all: $(EXE)

run: $(EXE)
	$(EXE)

zip: | $(BUILD)/
	zip -MM -r $(ZIP) . -i '*.c' '*.h' Makefile

clean:
	@rm -f $(OBJ) $(DEP) $(EXE) $(ZIP) $(GEN)
	@! [ -d $(BUILD) ] || find $(BUILD) -type d -delete

-include $(DEP)

# Roda casos de teste, não é relevante no runcodes
# (não está incluso no arquivo .zip)
-include test.mk

$(BUILD)/:
	@mkdir -p $@

$(BUILD)/.gitignore: $(BUILD)/
	@echo '*' > $@

$(BUILD)/%/:
	@mkdir -p $@

$(EXE): $(OBJ) | $(BUILD)/.gitignore
	$(CC) $(BASECFLAGS) $(OBJ) $(BASELDFLAGS) -o $@

$(BUILD)/%.o: src/%.c | $(dir $(OBJ))
	$(CC) $(BASECFLAGS) -c $< -o $@

.PHONY: all run zip clean
