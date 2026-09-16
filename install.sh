#!/bin/sh
# Builds and installs the file explorer, then hooks the `fe` command into the shell.
#
#   ./install.sh              install
#   ./install.sh --uninstall  remove everything again

set -e

cd "$(dirname "$0")"

PREFIX="${PREFIX:-$HOME/.local}"
INSTALL_DIR="$PREFIX/share/file_explorer"
SOURCE_LINE="source \"$INSTALL_DIR/fe.sh\""
MARKER="# file_explorer"

case "$(basename "${SHELL:-sh}")" in
    zsh)  RC_FILE="$HOME/.zshrc" ;;
    bash) RC_FILE="$HOME/.bashrc" ;;
    *)    RC_FILE="" ;;
esac

if [ "$1" = "--uninstall" ]; then
    make uninstall PREFIX="$PREFIX"
    if [ -n "$RC_FILE" ] && [ -f "$RC_FILE" ]; then
        grep -v "$MARKER" "$RC_FILE" > "$RC_FILE.tmp" || true
        mv "$RC_FILE.tmp" "$RC_FILE"
    fi
    echo "Uninstalled. Open a new terminal."
    exit 0
fi

missing=""
command -v cc   >/dev/null 2>&1 || missing="$missing compiler"
command -v make >/dev/null 2>&1 || missing="$missing make"
command -v file >/dev/null 2>&1 || missing="$missing file"

if [ "$(uname -s)" = "Linux" ] && [ ! -f /usr/include/ncurses.h ]; then
    missing="$missing ncurses"
fi

if [ -n "$missing" ]; then
    echo "Missing dependencies:$missing"
    if [ "$(uname -s)" = "Darwin" ]; then
        echo "Install them with:  xcode-select --install"
    else
        echo "Install them with:  sudo apt install build-essential libncurses-dev file"
    fi
    exit 1
fi

make install PREFIX="$PREFIX"

if [ -z "$RC_FILE" ]; then
    echo "Unknown shell. Add this line to your shell config manually:"
    echo "    $SOURCE_LINE"
elif ! grep -q "$MARKER" "$RC_FILE" 2>/dev/null; then
    echo "$SOURCE_LINE  $MARKER" >> "$RC_FILE"
    echo "Added 'fe' to $RC_FILE"
fi

echo "Done. Open a new terminal (or run: source $RC_FILE) and type: fe"
