// ****************************************************************************
//
//                 Game of Life - Optimalizováno pro RP2350 (2 jádra)
//                 EXTREME PERFORMANCE, SMART SD & USB CDC RLE EDITION
//
// ****************************************************************************

#include "../include.h"

#define SLOTS_MAX   99 
#define HASH_HIST_MAX 128

struct Tone {
    u16 freq;       
    u16 duration;   
};

const Tone Sound[] = {
    {NOTE_E5, 200}, {REST, 50},
    {NOTE_E5, 200}, {REST, 50},
    {NOTE_E5, 200}, {REST, 50},
    {NOTE_E5, 100}, {REST, 50},
    {NOTE_E5, 100}, {REST, 50},
    {NOTE_E5, 50},  {REST, 25},
    {NOTE_E5, 50},  {REST, 25},
    {NOTE_E5, 50},  {REST, 25},
    {NOTE_E5, 50},  {REST, 25},
    {NOTE_E5, 50},  {REST, 25},
    {NOTE_E5, 50},  {REST, 25},
    
    {NOTE_A4, 150}, {NOTE_B4, 150}, {NOTE_C5, 150}, {NOTE_D5, 150},
    {NOTE_E5, 250}, {NOTE_F5, 150}, {NOTE_E5, 250}, {NOTE_D5, 150},
    {NOTE_C5, 150}, {NOTE_B4, 150}, {NOTE_A4, 300}, {REST, 100},

    {NOTE_C5, 150}, {NOTE_B4, 150}, {NOTE_A4, 150}, {NOTE_GS4, 150},
    {NOTE_A4, 150}, {NOTE_B4, 150}, {NOTE_C5, 150}, {NOTE_A4, 150},
    {NOTE_GS4, 150},{NOTE_E4, 150}, {NOTE_GS4, 150},{NOTE_B4, 150},
    {NOTE_A4, 400}, {REST, 150},

    {NOTE_A4, 50}, {REST, 25}, 
    {NOTE_A4, 50}, {REST, 25}, 
    {NOTE_A4, 50}, {REST, 25},
    {NOTE_E5, 400}, {REST, 100}
};

const int SoundLen = sizeof(Sound) / sizeof(Sound[0]);

u8 BaseWave[200];

// Dynamické proměnné pro rozlišení
int MapW = 320; 
int MapH = 320;
int MapSize = 102400; 
int HistMax = 0;   
int CurrentSlot = 1;

// Proměnné pro zpožděné zadávání slotů
u32 SlotInputTime = 0;
int PendingSlotValue = -1;

// --- Barvy mřížky ---
const u16 GridColors[] = { 0x2104, 0x0000, 0x39E7 }; 
int GridColorIdx = 0;

// --- Barvy pro věk buněk (LUT tabulka) ---
u16 AgeColors[128];

// --- RLE Parser proměnné ---
int RleNum = 0;
int RleStartX = -1;
int RleX = 0;
int RleY = 0;
bool RleIgnoreLine = false;

// --- LUT TABULKY ---
int PosX[321];
int PosY[321];

// --- Stavy aplikace ---
enum {
	STATE_GAME = 0,
	STATE_MENU,
	STATE_SD_MENU,
    STATE_RES_MENU,
    STATE_RULE_MENU
};

enum {
	MENU_RESUME = 0,
    MENU_RULES,
    MENU_RESOLUTION,
	MENU_STEP_FWD,
	MENU_STEP_BWD,
	MENU_WRAP,
    MENU_COLOR_AGE, 
	MENU_UI_TOGGLE,
	MENU_CLEAR,
	MENU_SD, 
	MENU_EXIT,
	MENU_MAX
};

enum {
	SDMENU_BACK = 0,
	SDMENU_SAVE,
    SDMENU_SAVE_NEW,
	SDMENU_LOAD,
	SDMENU_SLOT_UP,
	SDMENU_SLOT_DOWN,
	SDMENU_MAX
};

enum {
	BRUSH_OFF = 0,
	BRUSH_ALIVE,  
	BRUSH_DEAD    
};

int ResMenuSel = 0;
int RuleMenuSel = 0;
 
// --- Globální proměnné ---
u8* Board = nullptr;
u8* NextBoard = nullptr;
u8* PrevBoard = nullptr;
u8** History = nullptr;    

int AllocHistMax = 0;      
int PrevCurX = -1;           
int PrevCurY = -1;           
bool ForceRedraw = true;     

int HistHead = 0;           
int HistCount = 0;          

int CurX = MapW / 2;
int CurY = MapH / 2;

int AppState = STATE_GAME;
int MenuSel = 0;
int SDMenuSel = 0;

bool IsPlaying = false;
bool WrapMode = true;       
bool ShowUI = true;         
bool ShowGrid = false;      
bool ColorAgeMode = false;  
bool AutoNudgeEnabled = false;
int NudgeCounter = 0; 
u8 BrushState = BRUSH_OFF;  

int GenDelayMs = 0;         
u32 LastGenTime = 0;        

char SDMessage[64] = "";    
u32 SDMessageTime = 0;

// --- Synchronizace pro Jádro 1 ---
volatile bool Core1_StartFlag = false;
volatile bool Core1_DoneFlag = false;

// --- Proměnné pro měření GPS ---
u32 GpsTimer = 0;         
int GpsCounter = 0;       
int LastGps = 0;          
u32 TotalGenerations = 0; 

// --- Konfigurace rozlišení (pouze dělitele 320) ---
const int ResOptions[] = {10, 16, 20, 32, 40, 64, 80, 160, 320};
const int ResHistOptions[] = {500, 500, 500, 250, 180, 64, 40, 8, 0}; 
const int ResCount = 9;
int CurrentResIndex = 8; 

// --- Konfigurace pravidel (Cellular Automata Rules) ---
struct CellularRule {
    const char* name;
    u16 birthMask;
    u16 surviveMask;
};

// Bitové masky: bit na pozici N znamená stav pro N sousedů (např. 1<<3 pro 3 sousedy)
const CellularRule Rulesets[] = {
    {"Conway (B3/S23)",       0b000001000, 0b000001100}, // B: 3, S: 2,3
    {"HighLife (B36/S23)",    0b001001000, 0b000001100}, // B: 3,6, S: 2,3
    {"Day & Night (B3678/S)", 0b111001000, 0b111111000}, // B: 3,6,7,8 S: 3,4,6,7,8
    {"Seeds (B2/S-)",         0b000000100, 0b000000000}, // B: 2, S: nic
    {"Maze (B3/S12345)",      0b000001000, 0b000111110}, // B: 3, S: 1,2,3,4,5
    {"Assimilation (B345/S)", 0b000111000, 0b001110000}  // B: 3,4,5 S: 4,5,6
};
const int RuleCount = sizeof(Rulesets) / sizeof(Rulesets[0]);

int CurrentRuleIdx = 0;
u16 ActiveBirthMask = 0b000001000;   // Výchozí Conway
u16 ActiveSurviveMask = 0b000001100; // Výchozí Conway

// --- Hashovací historie pro detekci uvíznutí ---
u32 StateHashes[HASH_HIST_MAX];
int HashIndex = 0;
int HashCount = 0;

void ResetStuckDetection() {
    HashIndex = 0;
    HashCount = 0;
}

// ============================================================================
// Paměťové funkce
// ============================================================================

void SaveHistory();
void UndoHistory();

void FreeMemory() {
    if (Board) { free(Board); Board = nullptr; }
    if (NextBoard) { free(NextBoard); NextBoard = nullptr; }
    if (PrevBoard) { free(PrevBoard); PrevBoard = nullptr; }
    
    if (History) {
        for (int i = 0; i < AllocHistMax; i++) {
            if (History[i]) free(History[i]);
        }
        free(History);
        History = nullptr;
    }
}

void AllocateMemory() {
    FreeMemory();

    Board = (u8*)malloc(MapSize);
    NextBoard = (u8*)malloc(MapSize);
    PrevBoard = (u8*)malloc(MapSize);
    
    if (!Board || !NextBoard || !PrevBoard) {
        if (Board) free(Board);
        if (NextBoard) free(NextBoard);
        if (PrevBoard) free(PrevBoard);
        
        MapW = 10; MapH = 10; MapSize = 100; HistMax = 0; CurrentResIndex = 0;
        Board = (u8*)malloc(MapSize);
        NextBoard = (u8*)malloc(MapSize);
        PrevBoard = (u8*)malloc(MapSize);
        
        strcpy(SDMessage, "KRITICKY NEDOSTATEK RAM!");
        SDMessageTime = Time();
    }

    memset(Board, 0, MapSize);
    memset(NextBoard, 0, MapSize);
    memset(PrevBoard, 0, MapSize);

    AllocHistMax = 0;
    if (HistMax > 0) {
        History = (u8**)malloc(HistMax * sizeof(u8*));
        if (History) {
            for (int i = 0; i < HistMax; i++) {
                History[i] = (u8*)malloc(MapSize);
                if (History[i]) {
                    memset(History[i], 0, MapSize);
                    AllocHistMax++; 
                } else {
                    break; 
                }
            }
        }
    }
    HistMax = AllocHistMax; 
    HistHead = 0;
    HistCount = 0;
}

// ============================================================================
// Práce s SD kartou a soubory
// ============================================================================

void GetSaveFileName(char* buffer, int slot) {
	memset(buffer, 0, 32);
	sprintf(buffer, "/LIFE_%02d.DAT", slot);
}

int FindNextFreeSlot() {
    sFile fil; char filename[32]; DiskMount();
    for (int i = 1; i <= SLOTS_MAX; i++) {
        GetSaveFileName(filename, i);
        if (FileOpen(&fil, filename)) FileClose(&fil);
        else return i;
    }
    return SLOTS_MAX; 
}

void SaveToSD() {
	char filename[32]; GetSaveFileName(filename, CurrentSlot);
	sFile fil; u32 bw;

	DiskMount();
	if (!DiskMounted()) {
		strcpy(SDMessage, "Chyba: SD nenalezena.");
		SDMessageTime = Time(); return;
	}

	FileDelete(filename);
	if (FileCreate(&fil, filename)) {
		bw = FileWrite(&fil, Board, MapSize);
		FileClose(&fil);
		DiskFlush();
		if (bw == MapSize) {
            char msg[64]; sprintf(msg, "Ulozeno do slotu %02d", CurrentSlot);
            strcpy(SDMessage, msg);
        } else strcpy(SDMessage, "Chyba zapisu!");
	} else strcpy(SDMessage, "Nelze vytvorit soubor.");
	SDMessageTime = Time();
}

void LoadFromSD() {
	char filename[32]; GetSaveFileName(filename, CurrentSlot);
	sFile fil; 

	DiskMount();
	if (!DiskMounted()) {
		strcpy(SDMessage, "Chyba: SD nenalezena.");
		SDMessageTime = Time(); return;
	}

	if (FileOpen(&fil, filename)) {
        u32 fileSize = 0; u8 tempBuf[1024]; 
        while (true) {
            u32 br = FileRead(&fil, tempBuf, 1024);
            fileSize += br;
            if (br < 1024) break; 
        }
        FileClose(&fil);

        int detectedResIndex = -1;
        for (int i = 0; i < ResCount; i++) {
            if (fileSize == (u32)(ResOptions[i] * ResOptions[i])) {
                detectedResIndex = i; break;
            }
        }

		if (detectedResIndex != -1) {
            CurrentResIndex = detectedResIndex;
            MapW = ResOptions[detectedResIndex]; MapH = ResOptions[detectedResIndex];
            MapSize = MapW * MapH; HistMax = ResHistOptions[detectedResIndex];

            for(int i = 0; i <= MapW; i++) PosX[i] = (i * 320) / MapW;
            for(int i = 0; i <= MapH; i++) PosY[i] = (i * 320) / MapH;

            AllocateMemory(); 

            if (FileOpen(&fil, filename)) {
                if (Board) FileRead(&fil, Board, MapSize);
                FileClose(&fil);
            }

            char msg[64]; sprintf(msg, "Slot %02d nacten (%dx%d)", CurrentSlot, MapW, MapH);
            strcpy(SDMessage, msg);
			
            IsPlaying = false; CurX = MapW / 2; CurY = MapH / 2; 
            PrevCurX = -1; PrevCurY = -1;
		} else {
			strcpy(SDMessage, "Chyba: Neznamy format souboru.");
			AllocateMemory(); 
		}
	} else strcpy(SDMessage, "Soubor nenalezen.");
	SDMessageTime = Time(); ForceRedraw = true;
}

// ============================================================================
// Logika Hry (Výpočet generace)
// ============================================================================

inline u8 GetCellValid(int x, int y) {
	if (WrapMode) {
		x = (x + MapW) % MapW;
		y = (y + MapH) % MapH;
	} else {
		if (x < 0 || x >= MapW || y < 0 || y >= MapH) return 0;
	}
	return Board[y * MapW + x] & 1; 
}

void SaveHistory() {
	if (HistMax <= 0 || History == nullptr) return; 
	memcpy(History[HistHead], Board, MapSize);
	HistHead = (HistHead + 1) % HistMax;
	if (HistCount < HistMax) HistCount++;
}

void UndoHistory() {
	if (HistMax <= 0 || History == nullptr) return; 
	if (HistCount > 0) {
		HistHead = (HistHead - 1 + HistMax) % HistMax;
		memcpy(Board, History[HistHead], MapSize);
		HistCount--;
	}
}

void ClearBoard() {
	SaveHistory();
	memset(Board, 0, MapSize);
	IsPlaying = false;
	TotalGenerations = 0; 
	ForceRedraw = true;
    ResetStuckDetection();
}

void CalcGeneration(int startY, int endY) {
    if (MapW == 320 && WrapMode) {
        for (int y = startY; y < endY; y++) {
            int y_up_ofs = ((y == 0) ? 319 : y - 1) * 320;
            int y_ofs    = y * 320;
            int y_dn_ofs = ((y == 319) ? 0 : y + 1) * 320;

            for (int x = 0; x < 320; x++) {
                int x_lt = (x == 0) ? 319 : x - 1;
                int x_rt = (x == 319) ? 0 : x + 1;

                int neighbors = 
                    (Board[y_up_ofs + x_lt] & 1) + (Board[y_up_ofs + x] & 1) + (Board[y_up_ofs + x_rt] & 1) +
                    (Board[y_ofs + x_lt] & 1)    +                             (Board[y_ofs + x_rt] & 1) +
                    (Board[y_dn_ofs + x_lt] & 1) + (Board[y_dn_ofs + x] & 1) + (Board[y_dn_ofs + x_rt] & 1);

                int idx = y_ofs + x;
                u8 cell = Board[idx];
                if (cell & 1) {
                    if (ActiveSurviveMask & (1 << neighbors)) {
                        u8 age = cell >> 1; if (age < 127) age++;
                        NextBoard[idx] = (age << 1) | 1;
                    } else NextBoard[idx] = 0;
                } else {
                    if (ActiveBirthMask & (1 << neighbors)) NextBoard[idx] = 1;
                    else NextBoard[idx] = 0;
                }
            }
        }
    } 
    else if (WrapMode) {
        bool isPowerOfTwo = (MapW & (MapW - 1)) == 0;
        if (isPowerOfTwo) {
            int mask = MapW - 1; 
            for (int y = startY; y < endY; y++) {
                int y_up = (y - 1 + MapH) & mask;
                int y_dn = (y + 1) & mask;
                int ofs_up = y_up * MapW;
                int ofs_md = y * MapW;
                int ofs_dn = y_dn * MapW;

                for (int x = 0; x < MapW; x++) {
                    int x_lt = (x - 1 + MapW) & mask;
                    int x_rt = (x + 1) & mask;

                    int neighbors = 
                        (Board[ofs_up + x_lt] & 1) + (Board[ofs_up + x] & 1) + (Board[ofs_up + x_rt] & 1) +
                        (Board[ofs_md + x_lt] & 1) +                           (Board[ofs_md + x_rt] & 1) +
                        (Board[ofs_dn + x_lt] & 1) + (Board[ofs_dn + x] & 1) + (Board[ofs_dn + x_rt] & 1);

                    int idx = ofs_md + x;
                    u8 cell = Board[idx];
                    if (cell & 1) {
                        if (ActiveSurviveMask & (1 << neighbors)) {
                            u8 age = cell >> 1; if (age < 127) age++;
                            NextBoard[idx] = (age << 1) | 1;
                        } else NextBoard[idx] = 0;
                    } else {
                        if (ActiveBirthMask & (1 << neighbors)) NextBoard[idx] = 1;
                        else NextBoard[idx] = 0;
                    }
                }
            }
        } else {
            for (int y = startY; y < endY; y++) {
                int y_up = (y - 1 + MapH) % MapH;
                int y_dn = (y + 1) % MapH;
                int ofs_up = y_up * MapW;
                int ofs_md = y * MapW;
                int ofs_dn = y_dn * MapW;

                for (int x = 0; x < MapW; x++) {
                    int x_lt = (x - 1 + MapW) % MapW;
                    int x_rt = (x + 1) % MapW;

                    int neighbors = 
                        (Board[ofs_up + x_lt] & 1) + (Board[ofs_up + x] & 1) + (Board[ofs_up + x_rt] & 1) +
                        (Board[ofs_md + x_lt] & 1) +                           (Board[ofs_md + x_rt] & 1) +
                        (Board[ofs_dn + x_lt] & 1) + (Board[ofs_dn + x] & 1) + (Board[ofs_dn + x_rt] & 1);

                    int idx = ofs_md + x;
                    u8 cell = Board[idx];
                    if (cell & 1) {
                        if (ActiveSurviveMask & (1 << neighbors)) {
                            u8 age = cell >> 1; if (age < 127) age++;
                            NextBoard[idx] = (age << 1) | 1;
                        } else NextBoard[idx] = 0;
                    } else {
                        if (ActiveBirthMask & (1 << neighbors)) NextBoard[idx] = 1;
                        else NextBoard[idx] = 0;
                    }
                }
            }
        }
    }
    else {
        for (int y = startY; y < endY; y++) {
            if (y == 0 || y == MapH - 1) {
                for (int x = 0; x < MapW; x++) {
                    int neighbors = 0;
                    neighbors += GetCellValid(x - 1, y - 1);
                    neighbors += GetCellValid(x,     y - 1);
                    neighbors += GetCellValid(x + 1, y - 1);
                    neighbors += GetCellValid(x - 1, y);
                    neighbors += GetCellValid(x + 1, y);
                    neighbors += GetCellValid(x - 1, y + 1);
                    neighbors += GetCellValid(x,     y + 1);
                    neighbors += GetCellValid(x + 1, y + 1);

                    int idx = y * MapW + x;
                    u8 cell = Board[idx];
                    if (cell & 1) {
                        if (ActiveSurviveMask & (1 << neighbors)) {
                            u8 age = cell >> 1; if (age < 127) age++;
                            NextBoard[idx] = (age << 1) | 1;
                        } else NextBoard[idx] = 0;
                    } else {
                        if (ActiveBirthMask & (1 << neighbors)) NextBoard[idx] = 1;
                        else NextBoard[idx] = 0;
                    }
                }
            } 
            else {
                int ofs_up = (y - 1) * MapW;
                int ofs_md = y * MapW;
                int ofs_dn = (y + 1) * MapW;
                int neighbors_l = 0;
                neighbors_l += GetCellValid(-1, y - 1);
                neighbors_l += GetCellValid( 0, y - 1);
                neighbors_l += GetCellValid( 1, y - 1);
                neighbors_l += GetCellValid(-1, y);
                neighbors_l += GetCellValid( 1, y);
                neighbors_l += GetCellValid(-1, y + 1);
                neighbors_l += GetCellValid( 0, y + 1);
                neighbors_l += GetCellValid( 1, y + 1);

                u8 cell_l = Board[ofs_md];
                if (cell_l & 1) {
                    if (neighbors_l == 2 || neighbors_l == 3) {
                        u8 age = cell_l >> 1; if (age < 127) age++;
                        NextBoard[ofs_md] = (age << 1) | 1;
                    } else NextBoard[ofs_md] = 0;
                } else {
                    if (neighbors_l == 3) NextBoard[ofs_md] = 1;
                    else NextBoard[ofs_md] = 0;
                }

                for (int x = 1; x < MapW - 1; x++) {
                    int neighbors = 
                        (Board[ofs_up + x - 1] & 1) + (Board[ofs_up + x] & 1) + (Board[ofs_up + x + 1] & 1) +
                        (Board[ofs_md + x - 1] & 1) +                           (Board[ofs_md + x + 1] & 1) +
                        (Board[ofs_dn + x - 1] & 1) + (Board[ofs_dn + x] & 1) + (Board[ofs_dn + x + 1] & 1);

                    int idx = ofs_md + x;
                    u8 cell = Board[idx];
                    if (cell & 1) {
                        if (ActiveSurviveMask & (1 << neighbors)) {
                            u8 age = cell >> 1; if (age < 127) age++;
                            NextBoard[idx] = (age << 1) | 1;
                        } else NextBoard[idx] = 0;
                    } else {
                        if (ActiveBirthMask & (1 << neighbors)) NextBoard[idx] = 1;
                        else NextBoard[idx] = 0;
                    }
                }

                int x_r = MapW - 1;
                int neighbors_r = 0;
                neighbors_r += GetCellValid(x_r - 1, y - 1);
                neighbors_r += GetCellValid(x_r,     y - 1);
                neighbors_r += GetCellValid(x_r + 1, y - 1);
                neighbors_r += GetCellValid(x_r - 1, y);
                neighbors_r += GetCellValid(x_r + 1, y);
                neighbors_r += GetCellValid(x_r - 1, y + 1);
                neighbors_r += GetCellValid(x_r,     y + 1);
                neighbors_r += GetCellValid(x_r + 1, y + 1);

                int idx_r = ofs_md + x_r;
                u8 cell_r = Board[idx_r];
                if (cell_r & 1) {
                    if (neighbors_r == 2 || neighbors_r == 3) {
                        u8 age = cell_r >> 1; if (age < 127) age++;
                        NextBoard[idx_r] = (age << 1) | 1;
                    } else NextBoard[idx_r] = 0;
                } else {
                    if (neighbors_r == 3) NextBoard[idx_r] = 1;
                    else NextBoard[idx_r] = 0;
                }
            }
        }
    }
}

void Core1_Main() {
	while (true) {
		while (!Core1_StartFlag) { __wfe(); } 
		Core1_StartFlag = false;
		CalcGeneration(MapH / 2, MapH);
		__dmb(); 
		Core1_DoneFlag = true;
		__sev(); 
	}
}

void StepGeneration() {
	SaveHistory();
	Core1_DoneFlag = false;
	__dmb();
	Core1_StartFlag = true;
	__sev();
	CalcGeneration(0, MapH / 2);
	while (!Core1_DoneFlag) { __wfe(); }

	memcpy(Board, NextBoard, MapSize);
	GpsCounter++; 
    TotalGenerations++;
}

// ============================================================================
// Kreslení na displej
// ============================================================================

void DrawAll() {
	u16 gridColor = GridColors[GridColorIdx]; 
	int uiTopY = (ShowUI && AppState == STATE_GAME) ? (320 - 50) : 320; 
	bool wasForced = ForceRedraw;
    int gapX = (MapW < 320) ? 1 : 0;
    int gapY = (MapH < 320) ? 1 : 0;

	if (!IsPlaying || AppState == STATE_MENU || AppState == STATE_SD_MENU) {
		if ((PrevCurX != CurX || PrevCurY != CurY) && PrevCurX >= 0) {
			int cx = PosX[PrevCurX];
			int cy = PosY[PrevCurY];
			if (cy < uiTopY) {
				int cw = PosX[PrevCurX + 1] - cx;
				int ch = PosY[PrevCurY + 1] - cy;
                
				DrawRect(cx, cy, cw, 1, gridColor);
                if (cy + ch - 1 < uiTopY) DrawRect(cx, cy + ch - 1, cw, 1, gridColor);
                int draw_ch = ch;
                if (cy + draw_ch > uiTopY) draw_ch = uiTopY - cy;
				
                DrawRect(cx, cy, 1, draw_ch, gridColor);
                DrawRect(cx + cw - 1, cy, 1, draw_ch, gridColor);
			}
			PrevBoard[PrevCurY * MapW + PrevCurX] = 0xFF; 
		}
	} else {
		if (PrevCurX >= 0) {
			ForceRedraw = true; wasForced = true;
			PrevCurX = -1; PrevCurY = -1;
		}
	}

	if (wasForced) {
		DrawRect(0, 0, 320, 320, gridColor);
		for (int y = 0; y < MapH; y++) {
			int y_pos = PosY[y];
			if (y_pos >= uiTopY) continue; 
			int h_pos = PosY[y + 1] - y_pos;
			if (y_pos + h_pos > uiTopY) h_pos = uiTopY - y_pos; 

			for (int x = 0; x < MapW; x++) {
				int x_pos = PosX[x];
				int w_pos = PosX[x + 1] - x_pos;
                
                u8 cell = Board[y * MapW + x];
				u16 color = (cell & 1) ? (ColorAgeMode ? AgeColors[cell >> 1] : COL_WHITE) : COL_BLACK;
                
                int draw_w = w_pos - gapX;
                int draw_h = h_pos - gapY;
                if (draw_w > 0 && draw_h > 0) DrawRect(x_pos, y_pos, draw_w, draw_h, color);
			}
		}
		memcpy(PrevBoard, Board, MapSize); 
		ForceRedraw = false;
	} else {
        if (MapW == 320) {
            for (int y = 0; y < 320; y++) {
                if (y >= uiTopY) continue;
                int row_ofs = y * 320;
                for (int x = 0; x < 320; x++) {
                    int idx = row_ofs + x;
                    if (Board[idx] != PrevBoard[idx]) {
                        u8 cell = Board[idx];
                        u16 color = (cell & 1) ? (ColorAgeMode ? AgeColors[cell >> 1] : COL_WHITE) : COL_BLACK;
                        
                        FrameBuf[row_ofs + x] = color;
                        DispStartImg(x, x, y, y); 
                        DispSendImg2(color);
                        DispStopImg();

                        PrevBoard[idx] = Board[idx];
                    }
                }
            }
        } 
        else {
            for (int y = 0; y < MapH; y++) {
                int y_pos = PosY[y];
                if (y_pos >= uiTopY) continue;
                int base_h_pos = PosY[y + 1] - y_pos;
                if (y_pos + base_h_pos > uiTopY) base_h_pos = uiTopY - y_pos; 

                for (int x = 0; x < MapW; x++) {
                    int idx = y * MapW + x;
                    if (Board[idx] != PrevBoard[idx]) {
                        int x_pos = PosX[x];
                        int w_pos = PosX[x + 1] - x_pos;
                        
                        u8 cell = Board[idx];
                        u16 color = (cell & 1) ? (ColorAgeMode ? AgeColors[cell >> 1] : COL_WHITE) : COL_BLACK;
                        
                        int draw_w = w_pos - gapX;
                        int draw_h = base_h_pos - gapY;

                        if (draw_w > 0 && draw_h > 0) {
                            DispStartImg(x_pos, x_pos + draw_w, y_pos, y_pos + draw_h);
                            int pixels = draw_w * draw_h;
                            for (int i = 0; i < pixels; i++) DispSendImg2(color); 
                            DispStopImg();
                            
                            for (int iy = 0; iy < draw_h; iy++) {
                                u16* fb = &FrameBuf[x_pos + (y_pos + iy) * WIDTH];
                                for (int ix = 0; ix < draw_w; ix++) {
                                    fb[ix] = color;
                                }
                            }
                        }
                        PrevBoard[idx] = Board[idx]; 
                    }
                }
            }
        }
	}

	if (!IsPlaying || AppState == STATE_MENU || AppState == STATE_SD_MENU) {
		int cx = PosX[CurX];
		int cy = PosY[CurY];
		if (cy < uiTopY) { 
			int cw = PosX[CurX + 1] - cx;
			int ch = PosY[CurY + 1] - cy;
			DrawRect(cx, cy, cw, 1, COL_YELLOW);
            if (cy + ch - 1 < uiTopY) DrawRect(cx, cy + ch - 1, cw, 1, COL_YELLOW);
            int draw_ch = ch;
            if (cy + draw_ch > uiTopY) draw_ch = uiTopY - cy;
			DrawRect(cx, cy, 1, draw_ch, COL_YELLOW);
			DrawRect(cx + cw - 1, cy, 1, draw_ch, COL_YELLOW);
		}
		PrevCurX = CurX;
		PrevCurY = CurY;
	}

	if (ShowUI && AppState == STATE_GAME) {
		static bool PrevIsPlaying = false;
		static u8 PrevBrushState = 255; 
		static int PrevGenDelay = -1;
		static int PrevGps = -1; 
		static u32 PrevTotalGens = 0xFFFFFFFF; 
        static int PrevSlot = -1; 
		
		u32 displayGens = IsPlaying ? ((TotalGenerations / 100) * 100) : TotalGenerations;

		if (wasForced || PrevIsPlaying != IsPlaying || PrevBrushState != BrushState || 
		    PrevGenDelay != GenDelayMs || PrevGps != LastGps || PrevTotalGens != displayGens || PrevSlot != CurrentSlot) {
			
			DrawRect(0, 320 - 50, 320, 50, COL_GRAY); 
			DrawText(IsPlaying ? "ZIJE" : "PAUZA", 5, 274, COL_WHITE);
			
			char spdTxt[32];
			if (GenDelayMs == 0) strcpy(spdTxt, "Rych:MAX");
			else sprintf(spdTxt, "Rych:%dms", GenDelayMs);
			DrawText(spdTxt, 65, 274, COL_CYAN); 
			
			if (BrushState == BRUSH_ALIVE) DrawText("St:ZIVA", 160, 274, COL_WHITE);
			else if (BrushState == BRUSH_DEAD) DrawText("St:MRTVA", 160, 274, COL_BLACK);
			else DrawText("St:VYP", 160, 274, 0x8410);

            char slotUi[16];
            sprintf(slotUi, "SD:%02d", CurrentSlot);
            DrawText(slotUi, 260, 274, COL_YELLOW);

			char genTxt[64];
			sprintf(genTxt, "Generace: %u  GPS: %d", displayGens, LastGps);
			DrawText(genTxt, 5, 290, COL_YELLOW); 
            if (AutoNudgeEnabled) DrawText("Mutace:ZAP", 210, 290, COL_GREEN);
            else DrawText("Mutace:VYP", 210, 290, 0x8410); 

			DrawText("Spc P +/- R U M . N ] B I", 5, 306, COL_BLACK);
			
			PrevIsPlaying = IsPlaying;
			PrevBrushState = BrushState;
			PrevGenDelay = GenDelayMs;
			PrevGps = LastGps;
			PrevTotalGens = displayGens; 
            PrevSlot = CurrentSlot;
		}
	}

	if (SDMessage[0] != 0) {
		if (Time() - SDMessageTime < 2000000) {
			int textWidth = strlen(SDMessage) * 8; 
			DrawRect((320 - textWidth) / 2 - 4, 10, textWidth + 8, 20, COL_RED);
			DrawText(SDMessage, (320 - textWidth) / 2, 12, COL_WHITE);
		} else {
			SDMessage[0] = 0; ForceRedraw = true; 
		}
	}

	if (AppState == STATE_MENU) {
		int mw = 240; int mh = 250;
		int mx = (320 - mw) / 2; int my = (320 - mh) / 2;
		
		DrawRect(mx, my, mw, mh, COL_BLUE);
		DrawRect(mx+2, my+2, mw-4, mh-4, COL_BLACK);
		DrawText("--- HLAVNI MENU ---", mx + 40, my + 10, COL_YELLOW);

		const char* menuTexts[MENU_MAX] = {
			"Navrat do hry", "Zmena pravidla", "Zmena rozliseni", "Krok vpred", "Krok vzad (Undo)",
			WrapMode ? "Okraje: NEKONECNE" : "Okraje: PEVNE",
            ColorAgeMode ? "Obarvovat dle veku: ZAP" : "Obarvovat dle veku: VYP",
			ShowUI ? "Skryt UI panel" : "Zobrazit UI panel",
			"Vymazat plochu", "Ulozit / Nacist na SD", "Ukoncit"
		};

		for (int i = 0; i < MENU_MAX; i++) {
			u16 tColor = (i == MenuSel) ? COL_BLACK : COL_WHITE;
			u16 bColor = (i == MenuSel) ? COL_YELLOW : COL_BLACK;
			DrawRect(mx + 10, my + 35 + i * 18 + 1, mw - 20, 16, bColor);
			DrawText(menuTexts[i], mx + 15, my + 35 + i * 18 + 4, tColor);
		}
	}
	else if (AppState == STATE_SD_MENU) {
		int mw = 240; int mh = 170; 
		int mx = (320 - mw) / 2; int my = (320 - mh) / 2;
		
		DrawRect(mx, my, mw, mh, COL_GREEN);
		DrawRect(mx+2, my+2, mw-4, mh-4, COL_BLACK);
		DrawText("--- SD KARTA ---", mx + 50, my + 10, COL_YELLOW);

		char slotText[32];
		sprintf(slotText, "Aktivni slot: %02d", CurrentSlot);

		const char* sdMenuTexts[SDMENU_MAX] = {
			"Zpet do menu", "Ulozit (Aktualni slot)", "Ulozit jako NOVY slot", 
            "Nacist stav (Load)", "Dalsi slot ->", "<- Predchozi slot"
		};

		DrawText(slotText, mx + 55, my + 35, COL_CYAN);
		for (int i = 0; i < SDMENU_MAX; i++) {
			u16 tColor = (i == SDMenuSel) ? COL_BLACK : COL_WHITE;
			u16 bColor = (i == SDMenuSel) ? COL_YELLOW : COL_BLACK;
			DrawRect(mx + 10, my + 55 + i * 18 + 1, mw - 20, 16, bColor);
			DrawText(sdMenuTexts[i], mx + 15, my + 55 + i * 18 + 4, tColor);
		}
	}
    else if (AppState == STATE_RES_MENU) {
        int mw = 220; int mh = 280; 
        int mx = (320 - mw) / 2; int my = (320 - mh) / 2;
        
        DrawRect(mx, my, mw, mh, COL_MAGENTA);
        DrawRect(mx+2, my+2, mw-4, mh-4, COL_BLACK);
        DrawText("--- ZMENA ROZLISENI ---", mx + 15, my + 10, COL_YELLOW);

		for (int i = 0; i < ResCount; i++) {
			int y_pos = my + 35 + (i * 25);
			char resText[64];
			sprintf(resText, "%d x %d (Hist:%d)", ResOptions[i], ResOptions[i], ResHistOptions[i]);
            u16 tColor = (i == ResMenuSel) ? COL_BLACK : COL_WHITE;
			u16 bColor = (i == ResMenuSel) ? COL_YELLOW : COL_BLACK;
            DrawRect(mx + 10, y_pos - 2, mw - 20, 16, bColor);
            DrawText(resText, mx + 15, y_pos + 1, tColor);
		}
	}
    else if (AppState == STATE_RULE_MENU) {
        int mw = 220; int mh = 220; 
        int mx = (320 - mw) / 2; int my = (320 - mh) / 2;
        
        DrawRect(mx, my, mw, mh, 0x03E0); // Tmavě zelená
        DrawRect(mx+2, my+2, mw-4, mh-4, COL_BLACK);
        DrawText("--- ZMENA PRAVIDEL ---", mx + 20, my + 10, COL_YELLOW);

		for (int i = 0; i < RuleCount; i++) {
			int y_pos = my + 35 + (i * 25);
            u16 tColor = (i == RuleMenuSel) ? COL_BLACK : COL_WHITE;
			u16 bColor = (i == RuleMenuSel) ? COL_YELLOW : COL_BLACK;
            DrawRect(mx + 10, y_pos - 2, mw - 20, 16, bColor);
            DrawText(Rulesets[i].name, mx + 15, y_pos + 1, tColor);
		}
	}
	DispUpdate(); 
}

void RandomizeBoard(int density) {
	SaveHistory(); 
	for (int i = 0; i < MapSize; i++) {
		Board[i] = ((rand() % 100) < density) ? 1 : 0; 
	}
	TotalGenerations = 0; 
	IsPlaying = false;    
	ForceRedraw = true;   
    ResetStuckDetection();
}

void InvertBoard() {
	SaveHistory();
	for (int i = 0; i < MapSize; i++) {
		Board[i] = (Board[i] & 1) ? 0 : 1; 
	}
	ForceRedraw = true;
    ResetStuckDetection();
}

void ChangeResolution(int newIndex) {
	if (newIndex < 0 || newIndex >= ResCount) return;
	CurrentResIndex = newIndex;
	MapW = ResOptions[newIndex]; MapH = ResOptions[newIndex];
	MapSize = MapW * MapH; HistMax = ResHistOptions[newIndex];

    for(int i = 0; i <= MapW; i++) PosX[i] = (i * 320) / MapW;
    for(int i = 0; i <= MapH; i++) PosY[i] = (i * 320) / MapH;

	AllocateMemory(); 

	CurX = MapW / 2; CurY = MapH / 2;
	PrevCurX = -1; PrevCurY = -1;

	TotalGenerations = 0; IsPlaying = false; ForceRedraw = true; 
}

// ============================================================================
// EXPERIMENTÁLNÍ FUNKCE: Detekce uvíznutí a Mutace
// ============================================================================

bool IsBoardStuck() {
    u32 currentHash = 5381;
    for (int i = 0; i < MapSize; i++) {
        currentHash = ((currentHash << 5) + currentHash) + (Board[i] & 1);
    }

    bool isStuck = false;
    for (int i = 0; i < HashCount; i++) {
        if (StateHashes[i] == currentHash) {
            isStuck = true;
            break;
        }
    }

    StateHashes[HashIndex] = currentHash;
    HashIndex = (HashIndex + 1) % HASH_HIST_MAX;
    if (HashCount < HASH_HIST_MAX) HashCount++;

    return isStuck;
}

void InjectCell(int cx, int cy, int dx, int dy) {
    int x = cx + dx;
    int y = cy + dy;
    
    if (WrapMode) {
        x = (x % MapW + MapW) % MapW;
        y = (y % MapH + MapH) % MapH;
    } else {
        if (x < 0 || x >= MapW || y < 0 || y >= MapH) return;
    }
    
    Board[y * MapW + x] = 1; 
}

void NudgeBoard() {
    int cx = rand() % MapW;
    int cy = rand() % MapH;
    int mutationType = rand() % 3; 

    if (mutationType == 0) {
        InjectCell(cx, cy,  0, -1);
        InjectCell(cx, cy,  1,  0);
        InjectCell(cx, cy, -1,  1);
        InjectCell(cx, cy,  0,  1);
        InjectCell(cx, cy,  1,  1);
    } 
    else if (mutationType == 1) {
        InjectCell(cx, cy,  0, -1);
        InjectCell(cx, cy,  1, -1);
        InjectCell(cx, cy, -1,  0);
        InjectCell(cx, cy,  0,  0);
        InjectCell(cx, cy,  0,  1);
    } 
    else {
        for (int dy = -1; dy <= 2; dy++) {
            for (int dx = -1; dx <= 2; dx++) {
                if (rand() % 100 < 60) { 
                    InjectCell(cx, cy, dx, dy);
                }
            }
        }
    }
    ForceRedraw = true; 
    ResetStuckDetection();
}

void CheckAndNudge() {
    if (IsBoardStuck()) {
        NudgeBoard(); 
        strcpy(SDMessage, "Mutace");
        SDMessageTime = Time();
    }
}

// ============================================================================
// Hlavní program
// ============================================================================

int main() {
	DeviceInit();

    // NASTAVENÍ USB CDC (Virtuální COM port)
    UsbDevInit(&UsbDevCdcSetupDesc); 

    for (int i = 0; i < 128; i++) {
        if (i == 0) AgeColors[i] = COL_WHITE;
        else if (i < 10) AgeColors[i] = 0x07FF; 
        else if (i < 30) AgeColors[i] = 0x07E0; 
        else if (i < 60) AgeColors[i] = 0xFFE0; 
        else if (i < 100) AgeColors[i] = 0xFD20; 
        else AgeColors[i] = 0xF800; 
    }
    
    for(int i = 0; i <= MapW; i++) PosX[i] = (i * 320) / MapW;
    for(int i = 0; i <= MapH; i++) PosY[i] = (i * 320) / MapH;

    AllocateMemory();

	VregSetVoltage(VREG_VOLTAGE_1_30);
	FlashSetClkDiv(6);
	SPI_Baudrate(1, 150000000);
   	ClockPllSysFreq(300000);

	memset(Board, 0, MapSize);
	multicore_launch_core1(Core1_Main);


    for(int i = 0; i < 200; i++) {
        if (i < 100) {
            BaseWave[i] = 28 + (i * 2);           // Stoupá z 28 na 226
        } else {
            BaseWave[i] = 226 - ((i - 100) * 2);  // Klesá zpět na 28
        }
    }

    // ---------------------------------------------------------
    DrawImgRle(intro, intro_Pal, 0, 0, 320, 320);
    DispUpdate(); 

    bool introSkipped = false;

    PlaySoundChan(0, BaseWave, 200, True, 1.0f, 0.0f, SNDFORM_PCM, 0);

    for (int i = 0; i < SoundLen; i++) {
        if (Sound[i].freq > 0) {
            float speedMultiplier = (float)Sound[i].freq / 110.25f;
            
            SpeedSoundChan(0, speedMultiplier);
            VolumeSoundChan(0, 0.5f);
        } else {
            VolumeSoundChan(0, 0.0f);
        }
        
        u32 startTime = Time();
        while ((Time() - startTime) < (Sound[i].duration * 1000)) { 
            u8 ch = KeyGet();
            if (ch == '\n' || ch == '\r' || ch == KEY_A) {
                introSkipped = true;
                break;
            }
        }
        
        if (introSkipped) break; 
    }

    StopSoundChan(0);

    while (!introSkipped) {
        u8 ch = KeyGet();
        if (ch == '\n' || ch == '\r' || ch == KEY_A) {
            break; 
        }
        else if (ch == KEY_Y) ResetToBootLoader();
    }

    ForceRedraw = true;
    // ---------------------------------------------------------

	while (true) {
        
        // --- USB RLE PŘÍJEM (CDC UART stream) ---
        char c;
        while ((c = UsbDevCdcReadChar()) != 0) { 
            
            // Zachytili jsme úplný začátek nového patternu (první znak)
            if (RleStartX == -1) {
                RleStartX = CurX; 
                RleX = CurX; 
                RleY = CurY;
                RleNum = 0;
                RleIgnoreLine = false;
            }

            // Ignorování hlaviček a komentářů (v RLE začínají znakem '#', 'x' nebo 'X')
            if (c == '#' || c == 'x' || c == 'X') {
                RleIgnoreLine = true;
            }
            
            // Na konci řádku se ignorování vypne a čteme dál normálně
            if (c == '\n' || c == '\r') {
                RleIgnoreLine = false;
                continue;
            }

            // Pokud jsme v komentáři, zbytek znaků přeskočíme
            if (RleIgnoreLine) continue;

            // Samotné dekódování RLE
            if (c >= '0' && c <= '9') {
                RleNum = RleNum * 10 + (c - '0');
            } 
            else if (c == 'b' || c == 'o' || c == 'B' || c == 'O') {
                int count = RleNum > 0 ? RleNum : 1;
                for (int n = 0; n < count; n++) {
                    int drawX = RleX;
                    int drawY = RleY;

                    // Ošetření okrajů obrazovky (Wrapování), aby se pattern neřízl
                    if (WrapMode) {
                        drawX = (drawX % MapW + MapW) % MapW;
                        drawY = (drawY % MapH + MapH) % MapH;
                    }
                    
                    if (drawX >= 0 && drawX < MapW && drawY >= 0 && drawY < MapH) {
                        Board[drawY * MapW + drawX] = (c == 'o' || c == 'O') ? 1 : 0;
                    }
                    RleX++;
                }
                RleNum = 0;
            } 
            else if (c == '$') {
                int count = RleNum > 0 ? RleNum : 1;
                RleY += count;
                RleX = RleStartX; // Návrat na počáteční X řádku (jako psací stroj)
                RleNum = 0;
            } 
            else if (c == '!') {
                RleNum = 0;
                RleStartX = -1; // Reset pro možnost načíst ihned další RLE soubor
                ForceRedraw = true;
                strcpy(SDMessage, "USB: Pattern prijat");
                SDMessageTime = Time();
            }
        }

        u8 ch = KeyGet();
		
		if (AppState == STATE_GAME) {

            if (PendingSlotValue != -1 && (Time() - SlotInputTime) >= 2000000) {
                if (PendingSlotValue > 0) { 
                    CurrentSlot = PendingSlotValue;
                    char msg[32]; sprintf(msg, "Zvolen slot %02d", CurrentSlot);
                    strcpy(SDMessage, msg); SDMessageTime = Time(); ForceRedraw = true;
                }
                PendingSlotValue = -1;
            }

			if (ch != NOKEY) {
                bool moved = false;

				if (ch >= '0' && ch <= '9') {
					if (PendingSlotValue == -1) {
                        PendingSlotValue = ch - '0'; SlotInputTime = Time();
                        char msg[32]; sprintf(msg, "Slot: %d_", PendingSlotValue);
                        strcpy(SDMessage, msg); SDMessageTime = Time();
                    } else {
                        PendingSlotValue = PendingSlotValue * 10 + (ch - '0');
                        if (PendingSlotValue > SLOTS_MAX) PendingSlotValue = SLOTS_MAX;
                        if (PendingSlotValue < 1) PendingSlotValue = 1;
                        CurrentSlot = PendingSlotValue; PendingSlotValue = -1;
                        
                        char msg[32]; sprintf(msg, "Zvolen slot %02d", CurrentSlot);
                        strcpy(SDMessage, msg); SDMessageTime = Time(); ForceRedraw = true;
                    }
                    goto skip_keys; 
				}
                else if (PendingSlotValue != -1) {
                    if (PendingSlotValue > 0) {
                        CurrentSlot = PendingSlotValue;
                        char msg[32]; sprintf(msg, "Zvolen slot %02d", CurrentSlot);
                        strcpy(SDMessage, msg); SDMessageTime = Time(); ForceRedraw = true;
                    }
                    PendingSlotValue = -1;
                }

				if (ch == KEY_RIGHT || ch == 'd' || ch == 'D') { CurX++; if (CurX >= MapW) CurX = WrapMode ? 0 : MapW - 1; moved = true; }
				else if (ch == KEY_LEFT || ch == 'a' || ch == 'A') { CurX--; if (CurX < 0) CurX = WrapMode ? MapW - 1 : 0; moved = true; }
				else if (ch == KEY_DOWN || ch == 's' || ch == 'S') { CurY++; if (CurY >= MapH) CurY = WrapMode ? 0 : MapH - 1; moved = true; }
				else if (ch == KEY_UP || ch == 'w' || ch == 'W') { CurY--; if (CurY < 0) CurY = WrapMode ? MapH - 1 : 0; moved = true; }
				
				if (ch == 'q' || ch == 'Q' || ch == '\t') { BrushState++; if (BrushState > BRUSH_DEAD) BrushState = BRUSH_OFF; }
				
                if (ch == KEY_A || ch == ' ') {
                    if (Board[CurY * MapW + CurX] & 1) Board[CurY * MapW + CurX] = 0;
                    else Board[CurY * MapW + CurX] = 1; 
                }

				if (moved) {
					if (BrushState == BRUSH_ALIVE) Board[CurY * MapW + CurX] = 1; 
					else if (BrushState == BRUSH_DEAD) Board[CurY * MapW + CurX] = 0; 
				}

				if (ch == KEY_B || ch == 'p' || ch == 'P' || ch == '\n' || ch == '\r') IsPlaying = !IsPlaying;
				else if (ch == 'r' || ch == 'R') StepGeneration();
				else if (ch == 'g' || ch == 'G') TotalGenerations = 0;
				else if (ch == 'u' || ch == 'U') UndoHistory();
				else if (ch == 'x' || ch == 'X') ClearBoard();
				else if (ch == KEY_X || ch == 'm' || ch == 'M' || ch == 0xB1) { IsPlaying = false; AppState = STATE_MENU; MenuSel = 0; }
                else if (ch == ']') { LoadFromSD(); AppState = STATE_GAME; }
				else if (ch == '+' || ch == '=') { 
                    if (GenDelayMs < 20) GenDelayMs -= 1; else if (GenDelayMs < 50) GenDelayMs -= 2; 
                    else if (GenDelayMs < 100) GenDelayMs -= 10; else if (GenDelayMs < 500) GenDelayMs -= 50; else GenDelayMs -= 100;
                    if (GenDelayMs < 0) GenDelayMs = 0;
				}
				else if (ch == '-') {
                    if (GenDelayMs < 20) GenDelayMs += 1; else if (GenDelayMs < 50) GenDelayMs += 2; 
                    else if (GenDelayMs < 100) GenDelayMs += 10; else if (GenDelayMs < 500) GenDelayMs += 50; else GenDelayMs += 100;
					if (GenDelayMs > 2000) GenDelayMs = 2000; 
				}
                else if (ch == '.') { ShowUI = !ShowUI; ForceRedraw = true; }
                else if (ch == 'b' || ch == 'B') { GridColorIdx++; if (GridColorIdx > 2) GridColorIdx = 0; ForceRedraw = true; }
				else if (ch == 'y' || ch == 'Y') RandomizeBoard(15); 
                else if (ch == 'i' || ch == 'I') InvertBoard();
                else if (ch == 'n' || ch == 'N') { AutoNudgeEnabled = !AutoNudgeEnabled; ForceRedraw = true; }
				else if (ch == KEY_Y) { ClockPllSysFreq(150000); FlashSetClkDiv(4); VregSetVoltage(VREG_VOLTAGE_1_10); ResetToBootLoader(); }

                skip_keys:; 
			}

			if (IsPlaying) {
				u32 currentTime = Time(); 
				if (GenDelayMs == 0 || (currentTime - LastGenTime) >= ((u32)GenDelayMs * 1000)) {
					StepGeneration(); LastGenTime = currentTime;
                    if (AutoNudgeEnabled) {
                        NudgeCounter++;
                        if (NudgeCounter >= 30) { 
                            NudgeCounter = 0; CheckAndNudge();
                        }
                    }
				}
			}
		} 
		else if (AppState == STATE_MENU) {
			if (ch != NOKEY) {
				if (ch == KEY_DOWN || ch == 's' || ch == 'S') { MenuSel++; if (MenuSel >= MENU_MAX) MenuSel = 0; }
				else if (ch == KEY_UP || ch == 'w' || ch == 'W') { MenuSel--; if (MenuSel < 0) MenuSel = MENU_MAX - 1; }
				else if (ch == KEY_B || ch == 0xB1) { AppState = STATE_GAME; ForceRedraw = true; }
				else if (ch == KEY_A || ch == ' ' || ch == '\n' || ch == '\r') { 
					switch(MenuSel) {
						case MENU_RESUME: AppState = STATE_GAME; break;
                        case MENU_RULES: AppState = STATE_RULE_MENU; RuleMenuSel = CurrentRuleIdx; break;
						case MENU_RESOLUTION: AppState = STATE_RES_MENU; ResMenuSel = CurrentResIndex; break;
						case MENU_STEP_FWD: StepGeneration(); AppState = STATE_GAME; break;
						case MENU_STEP_BWD: UndoHistory(); AppState = STATE_GAME; break;
						case MENU_WRAP: WrapMode = !WrapMode; break; 
                        case MENU_COLOR_AGE: ColorAgeMode = !ColorAgeMode; ForceRedraw = true; break;
						case MENU_UI_TOGGLE: ShowUI = !ShowUI; ForceRedraw = true; break; 
						case MENU_CLEAR: ClearBoard(); AppState = STATE_GAME; break;
						case MENU_SD: AppState = STATE_SD_MENU; SDMenuSel = 0; break;
						case MENU_EXIT: ResetToBootLoader(); break;
					}
				}
			}
		}
		else if (AppState == STATE_SD_MENU) {
			if (ch != NOKEY) {
				if (ch == KEY_DOWN || ch == 's' || ch == 'S') { SDMenuSel++; if (SDMenuSel >= SDMENU_MAX) SDMenuSel = 0; }
				else if (ch == KEY_UP || ch == 'w' || ch == 'W') { SDMenuSel--; if (SDMenuSel < 0) SDMenuSel = SDMENU_MAX - 1; }
                else if (ch == KEY_RIGHT || ch == 'd' || ch == 'D') { CurrentSlot += 1; if (CurrentSlot > SLOTS_MAX) CurrentSlot = SLOTS_MAX; }
                else if (ch == KEY_LEFT || ch == 'a' || ch == 'A') { CurrentSlot -= 1; if (CurrentSlot < 1) CurrentSlot = 1; }
				else if (ch == KEY_B || ch == 27) AppState = STATE_MENU;
				else if (ch == KEY_A || ch == ' ' || ch == '\n' || ch == '\r') {
					switch(SDMenuSel) {
						case SDMENU_BACK: AppState = STATE_MENU; break;
						case SDMENU_SAVE: SaveToSD(); AppState = STATE_GAME; break;
                        case SDMENU_SAVE_NEW: CurrentSlot = FindNextFreeSlot(); SaveToSD(); AppState = STATE_GAME; break;
						case SDMENU_LOAD: LoadFromSD(); AppState = STATE_GAME; break;
						case SDMENU_SLOT_UP: CurrentSlot++; if (CurrentSlot > SLOTS_MAX) CurrentSlot = 1; break;
						case SDMENU_SLOT_DOWN: CurrentSlot--; if (CurrentSlot < 1) CurrentSlot = SLOTS_MAX; break;
					}
				}
			}
		} 
		else if (AppState == STATE_RES_MENU) {
			if (ch != 0) {
				if (ch == KEY_UP || ch == 'w' || ch == 'W') { ResMenuSel--; if (ResMenuSel < 0) ResMenuSel = ResCount - 1; }
				else if (ch == KEY_DOWN || ch == 's' || ch == 'S') { ResMenuSel++; if (ResMenuSel >= ResCount) ResMenuSel = 0; }
				else if (ch == KEY_B || ch == 27) AppState = STATE_MENU;
				else if (ch == KEY_A || ch == ' ' || ch == '\n' || ch == '\r') { ChangeResolution(ResMenuSel); AppState = STATE_GAME; }
			}
		}
        else if (AppState == STATE_RULE_MENU) {
			if (ch != 0) {
				if (ch == KEY_UP || ch == 'w' || ch == 'W') { RuleMenuSel--; if (RuleMenuSel < 0) RuleMenuSel = RuleCount - 1; }
				else if (ch == KEY_DOWN || ch == 's' || ch == 'S') { RuleMenuSel++; if (RuleMenuSel >= RuleCount) RuleMenuSel = 0; }
				else if (ch == KEY_B || ch == 27) AppState = STATE_MENU;
				else if (ch == KEY_A || ch == ' ' || ch == '\n' || ch == '\r') { 
                    CurrentRuleIdx = RuleMenuSel;
                    ActiveBirthMask = Rulesets[CurrentRuleIdx].birthMask;
                    ActiveSurviveMask = Rulesets[CurrentRuleIdx].surviveMask;
                    AppState = STATE_GAME; 
                }
			}
		}
		
		static int lastAppState = STATE_GAME;
		if (AppState != lastAppState) { ForceRedraw = true; lastAppState = AppState; }
		
		DrawAll();

		if (Time() - GpsTimer >= 1000000) { LastGps = GpsCounter; GpsCounter = 0; GpsTimer = Time(); }
	}
	return 0;
}
