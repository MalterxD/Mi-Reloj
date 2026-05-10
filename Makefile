CC      = gcc
CFLAGS  = -Wall -Wextra -pedantic -std=c11 -O2 -Isrc
TARGET  = cli-clock
SRC     = src/clock.c src/battery.c
HDR     = src/clock.h src/battery.h
PREFIX  = /usr/local
BINDIR  = $(PREFIX)/bin

.PHONY: all clean run install uninstall setup

all: $(TARGET)

$(TARGET): $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

install: $(TARGET)
	@mkdir -p $(BINDIR)
	@install -m 755 $(TARGET) $(BINDIR)/$(TARGET)

setup:
	@echo "# CLI Clock configuration" > $(HOME)/.clockrc
	@echo "# https://github.com/MalterxD/CLI-Clock" >> $(HOME)/.clockrc
	@echo "" >> $(HOME)/.clockrc
	@echo "# Clock color (name or 0-255). Ignored if autocolor=true" >> $(HOME)/.clockrc
	@echo "color=cyan" >> $(HOME)/.clockrc
	@echo "" >> $(HOME)/.clockrc
	@echo "# Auto-detect color based on your distro" >> $(HOME)/.clockrc
	@echo "autocolor=true" >> $(HOME)/.clockrc
	@echo "" >> $(HOME)/.clockrc
	@echo "# Distro name color: dim or auto (matches digit color)" >> $(HOME)/.clockrc
	@echo "distro_color=dim" >> $(HOME)/.clockrc
	@echo "" >> $(HOME)/.clockrc
	@echo "# Time format: 24h or 12h" >> $(HOME)/.clockrc
	@echo "format=24h" >> $(HOME)/.clockrc
	@echo "" >> $(HOME)/.clockrc
	@echo "# Show large seconds" >> $(HOME)/.clockrc
	@echo "seconds=false" >> $(HOME)/.clockrc
	@echo "" >> $(HOME)/.clockrc
	@echo "# Show battery status" >> $(HOME)/.clockrc
	@echo "show_battery=false" >> $(HOME)/.clockrc
	@echo "" >> $(HOME)/.clockrc
	@echo "# Date display: small, large, hidden" >> $(HOME)/.clockrc
	@echo "date_size=small" >> $(HOME)/.clockrc
	@echo "" >> $(HOME)/.clockrc
	@echo "# Only show the clock, hide everything else" >> $(HOME)/.clockrc
	@echo "clean_mode=false" >> $(HOME)/.clockrc

uninstall:
	@rm -f $(BINDIR)/$(TARGET)

clean:
	@rm -f $(TARGET)
