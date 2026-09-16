CFLAGS ?= -O2 -Wall -Wextra
PREFIX ?= $(HOME)/.local
INSTALL_DIR = $(PREFIX)/share/file_explorer

BIN = file_explorer
SRC = src/main.c

# Linux needs the wide-char ncurses for UTF-8 names; on macOS libncurses already handles it
ifeq ($(shell uname -s),Darwin)
    LIBS = -lncurses
else
    LIBS = $(shell pkg-config --libs ncursesw 2>/dev/null || echo -lncursesw)
endif

.PHONY: all install uninstall clean

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LIBS)

install: $(BIN)
	mkdir -p "$(INSTALL_DIR)"
	cp $(BIN) "$(INSTALL_DIR)/$(BIN)"
	cp fe.sh "$(INSTALL_DIR)/fe.sh"

uninstall:
	rm -rf "$(INSTALL_DIR)"

clean:
	rm -f $(BIN)
