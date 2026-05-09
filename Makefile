CC      = gcc
CFLAGS  = -Wall -Wextra -pedantic -std=c11 -O2
TARGET  = cli-clock
SRC     = clock.c battery.c
HDR     = clock.h battery.h
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
	@echo "color=cyan" > $(HOME)/.clockrc
	@echo "autocolor=true" >> $(HOME)/.clockrc
	@echo "format=24h" >> $(HOME)/.clockrc
	@echo "seconds=false" >> $(HOME)/.clockrc
	@echo "show_battery=false" >> $(HOME)/.clockrc

uninstall:
	@rm -f $(BINDIR)/$(TARGET)

clean:
	@rm -f $(TARGET)
