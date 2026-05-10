#!/bin/bash
set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

ok() { echo -e "${GREEN}✓${NC} $1"; }
warn() { echo -e "${YELLOW}!${NC} $1"; }
err() {
  echo -e "${RED}✗${NC} $1"
  exit 1
}
info() { echo -e "${CYAN}→${NC} $1"; }

ask() {
  local prompt="$1"
  local reply
  echo -en "${BOLD}$prompt${NC} [y/N] "
  read -r reply
  [[ "$reply" =~ ^[Yy]$ ]]
}

install_deps() {
  if command -v pacman &>/dev/null; then
    sudo pacman -S --needed --noconfirm gcc make
  elif command -v apt &>/dev/null; then
    sudo apt install -y gcc make
  elif command -v dnf &>/dev/null; then
    sudo dnf install -y gcc make
  elif command -v zypper &>/dev/null; then
    sudo zypper install -y gcc make
  elif command -v xbps-install &>/dev/null; then
    sudo xbps-install -y gcc make
  else
    err "No supported package manager found. Install gcc and make manually."
  fi
}

detect_nerdfonts() {
  if command -v fc-list &>/dev/null; then
    if fc-list | grep -qi "nerd\|NF\|nerdfonts"; then
      echo "true"
      return
    fi
  fi
  local dirs=("$HOME/.local/share/fonts" "$HOME/.fonts" "/usr/share/fonts" "/usr/local/share/fonts")
  for dir in "${dirs[@]}"; do
    if [ -d "$dir" ] && find "$dir" \( -iname "*nerd*" -o -iname "*NF*" \) 2>/dev/null | grep -q .; then
      echo "true"
      return
    fi
  done
  echo "false"
}

echo ""
echo -e "${BOLD}CLI Clock — Installer${NC}"
echo "───────────────────────────────"
echo ""
info "This will compile and install cli-clock on your system."
info "Install location: /usr/local/bin/cli-clock"
info "Config file:      ~/.clockrc"
echo ""

ask "Proceed with installation?" || {
  echo "Aborted."
  exit 0
}
echo ""

MISSING_DEPS=0
if ! command -v gcc &>/dev/null && ! command -v clang &>/dev/null; then
  warn "No C compiler found"
  MISSING_DEPS=1
else
  ok "C compiler found"
fi

if ! command -v make &>/dev/null; then
  warn "make not found"
  MISSING_DEPS=1
else
  ok "make found"
fi

if [ "$MISSING_DEPS" = "1" ]; then
  ask "Try to install missing dependencies automatically?" || err "Please install gcc and make manually."
  install_deps
fi

echo ""
info "Compiling..."
make clean
make || err "Compilation failed."
ok "Compiled successfully"

echo ""
info "Installing to /usr/local/bin..."
sudo make install || err "Installation failed. Try running with sudo."
ok "Installed — cli-clock is ready"

echo ""
if [ -f "$HOME/.clockrc" ]; then
  warn "~/.clockrc already exists — skipping setup (your config was kept)"
else
  ask "Create default ~/.clockrc config?" || {
    echo ""
    echo -e "${GREEN}${BOLD}Done!${NC} Run: cli-clock"
    exit 0
  }

  make setup

  NERDFONTS=$(detect_nerdfonts)
  if [ "$NERDFONTS" = "true" ]; then
    ok "Nerd Fonts detected — battery icons enabled"
  else
    warn "Nerd Fonts not detected — using ASCII icons (BAT/CHG/LOW)"
    warn "To enable icons later: set nerdfonts=true in ~/.clockrc"
  fi
  echo "nerdfonts=$NERDFONTS" >>"$HOME/.clockrc"
  ok "Created ~/.clockrc with default settings"
fi

echo ""
echo -e "${GREEN}${BOLD}Done!${NC} Run: cli-clock"
echo ""
