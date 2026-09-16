# FILE EXPLORER FOR Raspberry Pi Lite OS

## Description
A simple terminal-based file explorer (CLI) designed primarily for Raspberry Pi OS Lite and other ARM64 Linux systems.

## Features
Current version: 2.0
- Navigate between files and directories
- Create directories
- Delete files and directories
- Display file sizes
- Open plain-text files

## Requirements
- Linux ARM64 (OS on ARM64)
- Raspberry Pi OS Lite / Linux / macOS
- Terminal

## Installation
```bash
git clone https://github.com/Vejvr0/simple_file_explorer.git
cd simple_file_explorer
./install.sh
```
Then open a new terminal and run `fe` (or `fe <directory>`). When you quit with `q`, the terminal stays in the directory you ended in.

If dependencies are missing, the script tells you how to install them:
- Raspberry Pi OS / Debian: `sudo apt install build-essential libncurses-dev file`
- macOS: `xcode-select --install`

Everything is installed into `~/.local/share/file_explorer` (no `sudo` needed).

Uninstall: `./install.sh --uninstall`

## Contributing
Feel free to submit pull requests or open issues.

## Author
Vejvr0