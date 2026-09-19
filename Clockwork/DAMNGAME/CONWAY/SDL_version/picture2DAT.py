#!/usr/bin/env python

import sys
from PIL import Image
import numpy as np

def preved_obrazek(velikost, vstup, vystup):
    try:
        # 2. Načtení a příprava obrázku
        img = Image.open(vstup).convert('L') # 'L' převede obrázek na stupně šedi
        img = img.resize((velikost, velikost))          # Násilné roztažení na 320x320
        
        # Převod na pole čísel
        img_data = np.array(img)

        # 3. Vytvoření prázdného pole pro hru
        game_data = np.zeros((velikost, velikost), dtype=np.uint8)

        # 4. Práh (Thresholding)
        # Vše, co je světlejší než 128, se stane živou buňkou (1)
        game_data[img_data > 128] = 1

        # 5. Uložení binárního souboru
        with open(vystup, 'wb') as f:
            f.write(game_data.tobytes())
            
        print(f"Hotovo! Soubor {vystup} byl uspesne vytvoren.")
        print(f"Velikost: {len(game_data.tobytes())} bajtu.")

    except FileNotFoundError:
        print(f"Chyba: Nemuzu najit soubor '{vstup}'")
    except Exception as e:
        print(f"Nastala neocekavana chyba: {e}")

if __name__ == "__main__":
    # Kontrola, zda uživatel zadal oba argumenty
    if len(sys.argv) != 4:
        print("Použití: python prevodnik.py <velikost_pixelu> <vstupni_obrazek> <vystupni_soubor>")
        print("Příklad: python prevodnik.py 320 mapa.jpg LIFE_7.DAT")
    else:
        velikost = sys.argv[1]
        vstup = sys.argv[2]
        vystup = sys.argv[3]
        preved_obrazek(int(velikost), vstup, vystup)
