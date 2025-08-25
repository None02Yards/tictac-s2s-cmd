CC       ?= gcc
CFLAGS   ?= -Wall -Wextra -O2 -pthread -Iinclude
LDFLAGS  ?= -pthread
BINDIR   ?= bin
BUILDDIR ?= build

SRC := src/main.c src/s2s.c src/game.c src/ui.c src/bot.c src/storage.c
OBJ := $(patsubst src/%.c,$(BUILDDIR)/%.o,$(SRC))
BIN := $(BINDIR)/s2s-tictactoe

.PHONY: all clean host connect run

all: $(BIN)

$(BIN): $(BINDIR) $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

$(BUILDDIR)/%.o: src/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BINDIR):
	@mkdir -p $(BINDIR)

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

# run targets
PORT ?= 5555
NAME ?= Alpha
HOST ?= 127.0.0.1

host: $(BIN)
	@echo ">>> Starting HOST (X) on port $(PORT)"
	$(BIN) --host $(PORT) --name "$(NAME)"

connect: $(BIN)
	@echo ">>> Connecting CLIENT (O) to $(HOST):$(PORT)"
	$(BIN) --connect $(HOST) $(PORT) --name "$(NAME)"

run: host

clean:
	@rm -rf $(BUILDDIR) $(BIN)

ifeq ($(PROTO_JSON),1)
CFLAGS += -DPROTO_JSON=1
endif
