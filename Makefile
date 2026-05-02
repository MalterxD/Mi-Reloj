CC      = gcc
CFLAGS  = -Wall -Wextra -pedantic -std=c11 -O2
TARGET  = clock
SRC     = reloj.c
HDR     = reloj.h

PREFIX  = /usr/local
BINDIR  = $(PREFIX)/bin

.PHONY: all clean run install uninstall setup

all: $(TARGET)

$(TARGET): $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

install: $(TARGET)
	@echo "Instalando $(TARGET) en $(BINDIR)..."
	@mkdir -p $(BINDIR)
	@install -m 755 $(TARGET) $(BINDIR)/$(TARGET)
	@echo "¡Listo! Ahora puedes usar el comando '$(TARGET)'"

setup:
	@echo "Creando configuración inicial en ~/.relojrc..."
	@echo "color=cyan" > $(HOME)/.relojrc
	@echo "✓ Configuración lista."


uninstall:
	@echo "Eliminando $(TARGET) de $(BINDIR)..."
	@rm -f $(BINDIR)/$(TARGET)

clean:
	rm -f $(TARGET)
