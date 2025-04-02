CC = cc

SRC = main.c
OBJ = $(SRC:.c=.o)
DEP = $(OBJ:.o=.d)
EXE = arquivos
ZIP = ../arquivos.zip

BASECFLAGS = -O2 -MMD $(CFLAGS)
BASELDFLAGS = $(LDFLAGS)

all: $(EXE)

run: $(EXE)
	./$(EXE)

zip: $(ZIP)

clean:
	rm -f $(OBJ) $(DEP) $(EXE) $(ZIP)

$(ZIP): $(SRC) Makefile
	zip -r $@ . -i $^

$(EXE): $(OBJ) $(GEN)
	$(CC) $(BASECFLAGS) $(OBJ) $(BASELDFLAGS) -o $@

%.o: %.c
	$(CC) $(BASECFLAGS) -c $< -o $@

.PHONY: all run zip clean
