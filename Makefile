CC = cc

BUILD = build

ifeq ($(BUILD),)
$(error "Define BUILD to a non-empty value")
endif

SRC = src/main.c src/file.c src/hash.c
OBJ = $(SRC:src/%.c=$(BUILD)/%.o)
DEP = $(OBJ:.o=.d)
EXE = $(BUILD)/arquivos
ZIP = $(BUILD)/arquivos.zip
GEN = $(BUILD)/.gitignore

BASECFLAGS = -O2 -MMD -std=gnu99 -Iinclude $(CFLAGS)
BASELDFLAGS = $(LDFLAGS)

all: $(EXE)

run: $(EXE)
	$(EXE)

zip: $(ZIP)

clean:
	rm -f $(OBJ) $(DEP) $(EXE) $(ZIP) $(GEN)
	! [ -d $(BUILD) ] || find $(BUILD) -type d -delete

$(BUILD)/:
	mkdir -p $@
	echo '*' > $@.gitignore

$(BUILD)/%/:
	mkdir -p $@

$(ZIP): $(SRC) Makefile | $(BUILD)/
	zip -r $@ . -i $^

$(EXE): $(OBJ) $(GEN)
	$(CC) $(BASECFLAGS) $(OBJ) $(BASELDFLAGS) -o $@

$(BUILD)/%.o: src/%.c | $(dir $(OBJ))
	$(CC) $(BASECFLAGS) -c $< -o $@

.PHONY: all run zip clean
