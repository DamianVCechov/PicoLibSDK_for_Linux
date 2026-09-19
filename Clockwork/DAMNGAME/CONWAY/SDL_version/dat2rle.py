#!/usr/bin/env python

import os
import re
import argparse
from typing import Set, Tuple

# Možnosti rozlišení tvého C programu
RES_OPTIONS = [8, 16, 32, 64, 128, 256, 320]

def detect_resolution(file_size: int) -> int:
    for res in RES_OPTIONS:
        if file_size == res * res:
            return res
    return 0

# =============================================================================
# DAT -> RLE (Export z tvé hry pro zbytek světa)
# =============================================================================

def encode_rle_string(s: str) -> str:
    """Zkomprimuje řetězec znaků do RLE (např. 'ooobboo' -> '3o2b2o')."""
    if not s:
        return ""
    result = ""
    count = 1
    current = s[0]
    for i in range(1, len(s)):
        if s[i] == current:
            count += 1
        else:
            result += (str(count) if count > 1 else "") + current
            current = s[i]
            count = 1
    result += (str(count) if count > 1 else "") + current
    return result

def dat_to_rle(input_dat: str, output_rle: str):
    if not os.path.exists(input_dat):
        print(f"Chyba: Vstupní soubor '{input_dat}' neexistuje.")
        return

    file_size = os.path.getsize(input_dat)
    res = detect_resolution(file_size)
    if res == 0:
        print(f"Chyba: Neznámá velikost souboru ({file_size} bajtů).")
        return

    # Načtení dat a nalezení živých buněk
    live_cells: Set[Tuple[int, int]] = set()
    with open(input_dat, "rb") as f:
        data = f.read()
        for i, byte in enumerate(data):
            if byte != 0:
                live_cells.add((i % res, i // res))

    if not live_cells:
        print("Upozornění: Plocha je prázdná, exportuji prázdné RLE.")
        with open(output_rle, "w") as f:
            f.write("x = 0, y = 0, rule = B3/S23\n!\n")
        return

    # Výpočet Bounding Boxu
    min_x = min(x for x, y in live_cells)
    max_x = max(x for x, y in live_cells)
    min_y = min(y for x, y in live_cells)
    max_y = max(y for x, y in live_cells)
    
    width = max_x - min_x + 1
    height = max_y - min_y + 1

    rle_rows = []
    for y in range(min_y, max_y + 1):
        row_chars = []
        for x in range(min_x, max_x + 1):
            row_chars.append('o' if (x, y) in live_cells else 'b')
        
        # Oříznutí koncových mrtvých buněk ('b') na řádku
        row_str = "".join(row_chars).rstrip('b')
        rle_rows.append(encode_rle_string(row_str) if row_str else "")

    # Spojení řádků pomocí '$' a komprese prázdných řádků (např. '$$$' -> '3$')
    raw_rle_data = ""
    empty_lines = 0
    for row in rle_rows:
        if row == "":
            empty_lines += 1
        else:
            if empty_lines > 0:
                raw_rle_data += (str(empty_lines + 1) if empty_lines > 0 else "") + "$"
                empty_lines = 0
            else:
                if raw_rle_data:
                    raw_rle_data += "$"
            raw_rle_data += row
    
    raw_rle_data += "!"

    # Zápis do souboru s max délkou 70 znaků na řádek
    with open(output_rle, "w") as f:
        f.write(f"#C Vygenerovano DAT-RLE konvertorem\n")
        f.write(f"x = {width}, y = {height}, rule = B3/S23\n")
        for i in range(0, len(raw_rle_data), 70):
            f.write(raw_rle_data[i:i+70] + "\n")
            
    print(f"Uspech: [{input_dat} -> {output_rle}] (Velikost vzoru: {width}x{height})")


# =============================================================================
# RLE -> DAT (Import z internetu do tvé hry)
# =============================================================================

def rle_to_dat(input_rle: str, output_dat: str, target_res: int = 320):
    if not os.path.exists(input_rle):
        print(f"Chyba: Vstupní soubor '{input_rle}' neexistuje.")
        return

    # 1. Přečtení RLE a vyfiltrování komentářů a metadat
    rle_string = ""
    with open(input_rle, "r") as f:
        for line in f:
            line = line.strip()
            if line.startswith("#") or line.startswith("x =") or line.startswith("x="):
                continue
            rle_string += line

    # 2. Dekódování RLE vzoru do souřadnic
    live_cells: Set[Tuple[int, int]] = set()
    x, y = 0, 0
    
    matches = re.findall(r'(\d*)([bo\$!])', rle_string)
    
    for count_str, char in matches:
        count = int(count_str) if count_str else 1
        
        if char == '!':
            break
        elif char == '$':
            y += count
            x = 0
        elif char == 'o':
            for _ in range(count):
                live_cells.add((x, y))
                x += 1
        elif char == 'b':
            x += count

    if not live_cells:
        print("Upozornění: Načteno prázdné RLE.")
    
    # 3. Vycentrování na cílovou mřížku
    min_x = min(x for x, y in live_cells) if live_cells else 0
    max_x = max(x for x, y in live_cells) if live_cells else 0
    min_y = min(y for x, y in live_cells) if live_cells else 0
    max_y = max(y for x, y in live_cells) if live_cells else 0
    
    pattern_w = max_x - min_x + 1
    pattern_h = max_y - min_y + 1

    if pattern_w > target_res or pattern_h > target_res:
        print(f"Chyba: RLE vzor ({pattern_w}x{pattern_h}) se do mřížky {target_res}x{target_res} nevejde!")
        return

    offset_x = (target_res - pattern_w) // 2 - min_x
    offset_y = (target_res - pattern_h) // 2 - min_y

    buffer = bytearray(target_res * target_res)
    for cx, cy in live_cells:
        nx, ny = cx + offset_x, cy + offset_y
        if 0 <= nx < target_res and 0 <= ny < target_res:
            buffer[ny * target_res + nx] = 1

    with open(output_dat, "wb") as f:
        f.write(buffer)

    print(f"Uspech: [{input_rle} -> {output_dat}] (Zapsano do stredu mrizky {target_res}x{target_res})")


# =============================================================================
# Zpracování argumentů z příkazové řádky
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Obousměrný konvertor pro Game of Life: z DAT do RLE a naopak."
    )
    subparsers = parser.add_subparsers(dest="command", required=True, help="Vyberte akci: 'export' nebo 'import'")

    # --- DAT -> RLE ---
    export_parser = subparsers.add_parser("export", help="Převede binární DAT soubor na textový RLE")
    export_parser.add_argument("input", type=str, help="Cesta ke vstupnímu DAT souboru (např. LIFE_01.DAT)")
    export_parser.add_argument("output", type=str, help="Cesta k výstupnímu RLE souboru (např. vzor.rle)")

    # --- RLE -> DAT ---
    import_parser = subparsers.add_parser("import", help="Převede textový RLE vzor do binárního DAT souboru")
    import_parser.add_argument("input", type=str, help="Cesta ke vstupnímu RLE souboru (např. vzor.rle)")
    import_parser.add_argument("output", type=str, help="Cesta k výstupnímu DAT souboru (např. LIFE_99.DAT)")
    import_parser.add_argument("--res", type=int, default=320, 
                               help="Cílové rozlišení mřížky DAT souboru (výchozí: 320)")

    args = parser.parse_args()

    if args.command == "export":
        dat_to_rle(args.input, args.output)
    elif args.command == "import":
        rle_to_dat(args.input, args.output, target_res=args.res)

if __name__ == "__main__":
    main()
