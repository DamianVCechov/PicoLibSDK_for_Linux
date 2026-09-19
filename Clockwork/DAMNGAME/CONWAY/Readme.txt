Conway's Game of Life – Optimalizováno pro RP2350
Vysoce výkonná implementace celulárního automatu "Hra života" navržená pro mikrokontrolér RP2350.
Tento port využívá obě procesorová jádra pro maximální výkon (Generations Per Second - GPS) a přináší pokročilé funkce, 
jako je přímé streamování RLE vzorů přes USB, ukládání na SD kartu a automatická ochrana proti zamrznutí simulace tzv. mutace.


🌟 Hlavní funkce
Dual-Core Processing: Extrémní výkon díky optimalizaci SPI komunikace s displejem a rozdělení výpočtu mřížky mezi obě jádra RP2350.
Dynamické rozlišení: Možnost přepínat rozlišení plochy od 10x10 až po 320x320 pixelů.
Paměť historie (Undo) se dynamicky přizpůsobuje zvolenému rozlišení (až 100 kroků zpět u menších ploch).

USB CDC RLE Parser: Podpora příjmu RLE souborů (standardní formát pro Game of Life vzory) v reálném čase přes virtuální sériový port.

Chytrá SD karta: Podpora uložení a načtení stavu plochy až do 99 nezávislých slotů (LIFE_XX.DAT).

Barevné odlišení věku: Volitelný režim obarvování buněk podle jejich stáří
(bílá [0. gen] -> azurová [1. gen] -> zelená [10. gen] -> žlutá [30. gen] -> oranžová [60. gen] -> červená [100. gen]).

Auto-Nudge (Mutace): Inteligentní detekce uvíznutí simulace (např. oscilátory s krátkou periodou nebo statické bloky).
Pokud se plocha zasekne, systém automaticky "mutuje" náhodnou část plochy.

Nekonečné okraje (Wrap Mode): Mřížka funguje jako toroidní plocha (přechod přes okraj obrazovky na druhou stranu).
Lze přepnout na pevné stěny.

🎮 Ovládání
Systém podporuje hardwarová tlačítka konzole PicoCalc ClockWork i standardní klávesnicové vstupy.

    Akce	            Klávesnice / Tlačítko	                                 Popis
Pohyb kurzoru	          Šipky, W, A, S, D	                        Pohyb po hrací ploše nebo v menu.
Kreslení / Smazání	       Mezerník, F1	                       Přepne stav buňky pod kurzorem (živá/mrtvá).
Play / Pauza	           P, Enter, F2	                             Spustí nebo pozastaví simulaci.
Režim štětce	                  Q	                  Přepíná módy štětce (Vypnuto -> Kreslit živé -> Kreslit mrtvé).
Krok vpřed	                      R	                      Manuálně posune simulaci o 1 generaci (pouze v Pauze).
Krok vzad (Undo)	              U	                                Vrátí simulaci o jeden krok zpět.
Hlavní Menu	                    M, F3	                                Otevře konfigurační menu.
Zrychlit / Zpomalit	            + / -	                            Upravuje zpoždění mezi generacemi.
Vymazat plochu	                  x	                                        Smaže celou plochu.
Náhodná plocha	                  Y	                                Zaplní plochu náhodně s hustotou 15%.
Změna Gridu	                      B	                               Přepíná barvu mřížky (černá, šedá, světle šedá).
Zobrazit / Skrýt UI	          . (tečka)                                   Skryje spodní stavový řádek.
Výběr SD slotu	                0 - 9	                     Postupné stisknutí vybere slot 1-99 pro uložení/načtení.
Rychlý Load	                      ]	                                Okamžitě načte stav ze zvoleného SD slotu.
Přepínač Mutace	                  N	                        Zapne/Vypne automatické mutace při zamrznutí (Auto-Nudge).
Ukončit (Bootloader)	          F4	                                      Ukončí program
￼
￼
🔌 Import přes USB (RLE formát)
Aplikace se po připojení k PC hlásí jako virtuální COM port (USB CDC).
Můžete do něj odeslat jakýkoliv standardní .rle soubor pouhým překopírováním nebo přes terminál.

Příklad odeslání z Linuxu: cat glider_gun.rle > /dev/ttyACM0

Vzor se začne vykreslovat na aktuální pozici vašeho kurzoru (chová se jako psací stroj).

---------------------------------------------------------------------------------------------------------------------------------

Conway's Game of Life – RP2350 Optimized Edition
A high-performance implementation of the "Game of Life" cellular automaton designed for the RP2350 microcontroller.
This port utilizes both CPU cores to maximize performance (Generations Per Second - GPS) and introduces advanced 
features such as real-time RLE pattern streaming via USB, SD card saving, and automatic stuck-simulation mutation.

🌟 Key Features
Dual-Core Processing: Extreme processing speed by splitting the grid calculation between both RP2350 cores.

Dynamic Resolution: Change the grid resolution on the fly from 10x10 up to 320x320 pixels. 
The Undo history dynamically scales based on the chosen resolution (up to 100 steps for smaller grids).

USB CDC RLE Parser: Stream standard .rle pattern files directly via the virtual serial port.
The patterns are decoded and placed in real-time.

Smart SD Card Management: Save and load your grid states into 99 independent slots (LIFE_XX.DAT).

Cell Age Coloring: Optional mode that colors cells based on how long they have been alive 
(White -> Cyan -> Green -> Yellow -> Orange -> Red).

Auto-Nudge (Mutation): Intelligent detection of stuck grids (e.g., short-period oscillators or still lifes). 
If the board stops evolving, the system automatically injects a random mutation.

Wrap Mode: The grid acts as a toroidal surface (moving past the edge brings you to the opposite side). Can be toggled to fixed/hard walls.

🎮 Controls
The system supports both hardware console buttons and standard keyboard inputs.

Action	                Keyboard / Button	                                Description
Move Cursor	            Arrows, W, A, S, D	                    Move around the grid or navigate menus.
Toggle Cell	               Space, F1	            Flips the state of the cell under the cursor (Alive/Dead).
Play / Pause	          P, Enter, F2	                            Starts or pauses the simulation.
Brush Mode	                   Q	            Toggles continuous drawing modes (Off -> Draw Alive -> Draw Dead).
Step Forward	               R                  Manually advance the simulation by 1 generation (while paused).
Step Backward (Undo)	       U                                   Revert to the previous generation.
Main Menu	                 M, F3	                                Opens the configuration menu.
Speed Up / Slow Down	     + / -	                        Adjusts the delay time between generations.
Clear Board	                   x                                    	Wipes the entire grid.
Randomize	                   Y	                            Fills the board with a 15% random density.
Toggle Grid Color	           B                                    	Cycles background colors.
Toggle UI	              . (period)	                          Shows or hides the bottom status bar.
Select SD Slot	             0 - 9	                Type numbers sequentially to select slots 1-99 for save/load.
Quick Load	                   ]	               Instantly loads the state from the currently selected SD slot.
Toggle Mutation	               N                   	  Enables/Disables the Auto-Nudge feature for stuck boards.
Exit (Bootloader)	          F4	                            Resets the device into bootloader mode.
￼
￼
🔌 USB Pattern Import (RLE format)
When connected to a PC, the device registers as a virtual COM port (USB CDC). You can send any standard .rle file directly to the device.

Example from a Linux terminal: cat glider_gun.rle > /dev/ttyACM0

The pattern will begin drawing exactly where your cursor is currently located on the screen.
