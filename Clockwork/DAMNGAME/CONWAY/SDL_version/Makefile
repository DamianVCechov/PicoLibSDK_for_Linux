# Kompilátor
CC = g++

# Vlajky pro kompilátor (zapnutá varování a optimalizace)
CFLAGS =

# Knihovny k přilinkování
LDFLAGS = -lSDL2 -lSDL2_ttf

# Název výstupního souboru a zdrojáku
TARGET = conway
SRC = conway.c

# Výchozí pravidlo (spustí se při pouhém zavolání 'make')
all: $(TARGET)

# Pravidlo pro sestavení programu
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

# Pravidlo pro smazání zkompilovaného souboru (make clean)
clean:
	rm -f $(TARGET)
