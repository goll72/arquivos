CC = cc
AR = ar

# Set to 0 originally, set to 1 in the zip file
RUNCODES_ENV = 0

SRC = 
OBJ = $(SRC:.c=.o)
EXE = arquivos
ZIP = ../arquivos.zip

BASECFLAGS = -O2 -DRUNCODES_ENV=$(RUNCODES_ENV) $(CFLAGS)
BASELDFLAGS = -L. -lmd $(LDFLAGS)

all: $(EXE)

run: $(EXE)
	./$(EXE)

zip: $(ZIP)

clean:
	rm -f $(OBJ) $(EXTRA_OBJ) $(GEN) $(EXE) $(ZIP)

# This is very cursed
$(ZIP): $(SRC) Makefile
	sed -i 's/^RUNCODES_ENV = 0$$/RUNCODES_ENV = 1/' Makefile
	zip -r $@ . -i $(SRC) Makefile
	sed -i 's/^RUNCODES_ENV = 1$$/RUNCODES_ENV = 0/' Makefile

ifeq ($(RUNCODES_ENV),0)
EXTRA_SRC = md.c
EXTRA_OBJ = $(EXTRA_SRC:.c=.o)

GEN += libmd.a

libmd.a: md.o
	$(AR) r $@ $<
endif

$(EXE): $(OBJ) $(GEN)
	$(CC) $(BASECFLAGS) $(OBJ) $(BASELDFLAGS) -o $@

%.o: %.c
	$(CC) $(BASECFLAGS) -c $< -o $@

.PHONY: all run zip clean
