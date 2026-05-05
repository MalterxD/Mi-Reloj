CC      = gcc
CFLAGS  = -Wall -Wextra -pedantic -std=c11 -O2
TARGET  = clock
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
	@echo "Installing $(TARGET) to $(BINDIR)..."
	@mkdir -p $(BINDIR)
	@install -m 755 $(TARGET) $(BINDIR)/$(TARGET)
	@echo "Done! You can now use the '$(TARGET)' command."

setup:
	@echo "Creating initial configuration in $(HOME)/.clockrc..."
	@echo "color=cyan" > $(HOME)/.clockrc
	@echo "autocolor=true" >> $(HOME)/.clockrc
	@echo "format=24h" >> $(HOME)/.clockrc
	@echo "✓ Configuration ready."

uninstall:
	@echo "Removing $(TARGET) from $(BINDIR)..."
	@rm -f $(BINDIR)/$(TARGET)

clean:
	@echo "Cleaning up..."
	@rm -f $(TARGET)
