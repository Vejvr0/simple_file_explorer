# Shell wrapper for the file explorer (bash + zsh).
#
# A program cannot change the working directory of the shell that started it,
# so the explorer writes its last directory into $EXPLORER_CWD_FILE on quit
# and this function does the `cd` inside the current shell.
#
# Usage: add this line to ~/.zshrc (macOS) or ~/.bashrc (Raspberry Pi):
#     source ~/.local/share/file_explorer/fe.sh   (install.sh does this for you)
# then run `fe` (optionally `fe <start_dir>`).

if [ -n "$ZSH_VERSION" ]; then
    FE_DIR="$(cd "$(dirname "${(%):-%x}")" && pwd)"
else
    FE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
fi

fe() {
    local tmp
    tmp="$(mktemp)"
    EXPLORER_CWD_FILE="$tmp" "$FE_DIR/file_explorer" "$@"
    if [ -s "$tmp" ]; then
        cd "$(cat "$tmp")"
    fi
    rm -f "$tmp"
}
