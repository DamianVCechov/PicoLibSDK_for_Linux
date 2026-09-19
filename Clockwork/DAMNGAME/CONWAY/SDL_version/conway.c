// ****************************************************************************
//
//                 Game of Life - SDL2 PC Port s Myší a 3x Zoomem
//                 EXTREME PERFORMANCE, SMART SD & MUTATION EDITION
//
// ****************************************************************************

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

// --- Definice kláves ---
#define NOKEY 0
#define KEY_RIGHT 1001
#define KEY_LEFT  1002
#define KEY_DOWN  1003
#define KEY_UP    1004
#define KEY_A     1005
#define KEY_B     1006
#define KEY_X     1007
#define KEY_Y     1008

// --- Barvy (RGB format pro SDL2) ---
#define COL_BLACK   0x000000
#define COL_WHITE   0xFFFFFF
#define COL_YELLOW  0xFFFF00
#define COL_GRAY    0x808080
#define COL_CYAN    0x00FFFF
#define COL_RED     0xFF0000
#define COL_BLUE    0x0000FF
#define COL_GREEN   0x00FF00
#define COL_MAGENTA 0xFF00FF
#define COL_ORANGE  0xFF8800

// --- SDL2 proměnné ---
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TTF_Font* font = NULL;
#define FONT_PATH "font.ttf" 

#define SLOTS_MAX   99 

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
const u32 GridColors[] = { 0x222222, 0x000000, 0x444444 }; 
int GridColorIdx = 0;

// --- Barvy pro věk buněk (LUT tabulka) ---
u32 AgeColors[128];

// --- LUT TABULKY ---
int PosX[321];
int PosY[321];

// --- Stavy aplikace ---
enum { STATE_GAME = 0, STATE_MENU, STATE_SD_MENU, STATE_RES_MENU };

enum {
    MENU_RESUME = 0,
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

enum { BRUSH_OFF = 0, BRUSH_ALIVE, BRUSH_DEAD };

int ResMenuSel = 0; 
 
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

// --- Proměnné pro měření GPS ---
u32 GpsTimer = 0;         
int GpsCounter = 0;       
int LastGps = 0;          
u32 TotalGenerations = 0; 

// --- Konfigurace rozlišení (pouze dělitele 320) ---
const int ResOptions[] = {10, 16, 20, 32, 40, 64, 80, 160, 320};
const int ResHistOptions[] = {100, 100, 100, 100, 100, 64, 32, 6, 0}; 
const int ResCount = 9;
int CurrentResIndex = 8; 

// ============================================================================
// Pomocné funkce a Paměť
// ============================================================================

u32 Time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (u32)(tv.tv_sec * 1000000 + tv.tv_usec);
}

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
// Práce s SD kartou (Simulace na PC)
// ============================================================================

void GetSaveFileName(char* buffer, int slot) {
    sprintf(buffer, "LIFE_%02d.DAT", slot);
}

int FindNextFreeSlot() {
    char filename[32];
    for (int i = 1; i <= SLOTS_MAX; i++) {
        GetSaveFileName(filename, i);
        FILE* f = fopen(filename, "rb");
        if (f) { fclose(f); } 
        else { return i; }
    }
    return SLOTS_MAX; 
}

void SaveToSD() {
    char filename[32]; GetSaveFileName(filename, CurrentSlot);
    FILE* f = fopen(filename, "wb");
    if (f) {
        size_t bw = fwrite(Board, 1, MapSize, f);
        fclose(f);
        if (bw == (size_t)MapSize) {
            sprintf(SDMessage, "Ulozeno do slotu %02d", CurrentSlot);
        } else {
            strcpy(SDMessage, "Chyba zapisu do souboru!");
        }
    } else {
        strcpy(SDMessage, "Nelze vytvorit soubor.");
    }
    SDMessageTime = Time();
}

void LoadFromSD() {
    char filename[32]; GetSaveFileName(filename, CurrentSlot);
    FILE* f = fopen(filename, "rb");
    
    if (f) {
        fseek(f, 0, SEEK_END);
        u32 fileSize = ftell(f);
        fseek(f, 0, SEEK_SET);

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
            fread(Board, 1, MapSize, f);
            fclose(f);

            sprintf(SDMessage, "Slot %02d nacten (%dx%d)", CurrentSlot, MapW, MapH);
            IsPlaying = false; CurX = MapW / 2; CurY = MapH / 2; 
            PrevCurX = -1; PrevCurY = -1;
        } else {
            strcpy(SDMessage, "Chyba: Neznamy format souboru.");
            fclose(f);
            AllocateMemory(); 
        }
    } else {
        strcpy(SDMessage, "Soubor nenalezen.");
    }
    SDMessageTime = Time(); ForceRedraw = true;
}

// ============================================================================
// Logika Hry
// ============================================================================

static inline u8 GetCellValid(int x, int y) {
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
                    if (neighbors == 2 || neighbors == 3) {
                        u8 age = cell >> 1;
                        if (age < 127) age++; 
                        NextBoard[idx] = (age << 1) | 1;
                    } else NextBoard[idx] = 0;
                } else {
                    if (neighbors == 3) NextBoard[idx] = 1; 
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
                        if (neighbors == 2 || neighbors == 3) {
                            u8 age = cell >> 1; if (age < 127) age++;
                            NextBoard[idx] = (age << 1) | 1;
                        } else NextBoard[idx] = 0;
                    } else {
                        if (neighbors == 3) NextBoard[idx] = 1;
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
                        if (neighbors == 2 || neighbors == 3) {
                            u8 age = cell >> 1; if (age < 127) age++;
                            NextBoard[idx] = (age << 1) | 1;
                        } else NextBoard[idx] = 0;
                    } else {
                        if (neighbors == 3) NextBoard[idx] = 1;
                        else NextBoard[idx] = 0;
                    }
                }
            }
        }
    }
    else {
        for (int y = startY; y < endY; y++) {
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
                    if (neighbors == 2 || neighbors == 3) {
                        u8 age = cell >> 1; if (age < 127) age++;
                        NextBoard[idx] = (age << 1) | 1;
                    } else NextBoard[idx] = 0;
                } else {
                    if (neighbors == 3) NextBoard[idx] = 1;
                    else NextBoard[idx] = 0;
                }
            }
        }
    }
}

void StepGeneration() {
    SaveHistory();
    CalcGeneration(0, MapH);
    memcpy(Board, NextBoard, MapSize);
    GpsCounter++; 
    TotalGenerations++;
}

// ============================================================================
// EXPERIMENTÁLNÍ FUNKCE: Mutace
// ============================================================================

bool IsBoardStuck() {
    if (HistCount < 2 || History == nullptr) return false;
    for (int i = 1; i <= HistCount; i++) {
        int idx = (HistHead - i + HistMax) % HistMax;
        if (memcmp(Board, History[idx], MapSize) == 0) return true; 
    }
    return false;
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
}

void CheckAndNudge() {
    if (IsBoardStuck()) {
        NudgeBoard(); 
        strcpy(SDMessage, "Mutace");
        SDMessageTime = Time();
    }
}

// ============================================================================
// Kreslení na displej
// ============================================================================

void DrawRect(int x, int y, int w, int h, u32 colHex) {
    SDL_SetRenderDrawColor(renderer, (colHex >> 16) & 0xFF, (colHex >> 8) & 0xFF, colHex & 0xFF, 255);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(renderer, &r);
}

void DrawText(const char* text, int x, int y, u32 colHex) {
    if (!font || !text || text[0] == '\0') return;
    SDL_Color col = {(u8)((colHex >> 16) & 0xFF), (u8)((colHex >> 8) & 0xFF), (u8)(colHex & 0xFF), 255};
    SDL_Surface* surface = TTF_RenderText_Solid(font, text, col);
    if (surface) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture) {
            SDL_Rect dst = {x, y, surface->w, surface->h};
            SDL_RenderCopy(renderer, texture, NULL, &dst);
            SDL_DestroyTexture(texture);
        }
        SDL_FreeSurface(surface);
    }
}

void DrawAll() {
    u32 gridColor = GridColors[GridColorIdx]; 
    DrawRect(0, 0, 320, 320, gridColor); // Clears the screen to prevent flickering

    int uiTopY = (ShowUI && AppState == STATE_GAME) ? (320 - 50) : 320; 
    int gapX = (MapW < 320) ? 1 : 0;
    int gapY = (MapH < 320) ? 1 : 0;

    // Vykreslení matice buněk
    for (int y = 0; y < MapH; y++) {
        int y_pos = PosY[y];
        if (y_pos >= uiTopY) continue; 
        
        int h_pos = PosY[y + 1] - y_pos;
        if (y_pos + h_pos > uiTopY) h_pos = uiTopY - y_pos; 
        
        for (int x = 0; x < MapW; x++) {
            int idx = y * MapW + x;
            u8 cell = Board[idx];
            
            if (cell & 1) { // Je-li buňka živá
                int x_pos = PosX[x];
                int w_pos = PosX[x + 1] - x_pos;

                u32 color = ColorAgeMode ? AgeColors[cell >> 1] : COL_WHITE;
                
                int draw_w = (w_pos > gapX) ? w_pos - gapX : 1;
                int draw_h = (h_pos > gapY) ? h_pos - gapY : 1;
                
                DrawRect(x_pos, y_pos, draw_w, draw_h, color); 
            }
        }
    }

    // Kurzor na klávesnici
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
    }

    // Spodní Panel
    if (ShowUI && AppState == STATE_GAME) {
        u32 displayGens = IsPlaying ? ((TotalGenerations / 100) * 100) : TotalGenerations;

        DrawRect(0, 320 - 50, 320, 50, COL_GRAY); 
        DrawText(IsPlaying ? "ZIJE" : "PAUZA", 5, 274, COL_WHITE);
        
        char spdTxt[32];
        if (GenDelayMs == 0) strcpy(spdTxt, "Rych:MAX");
        else sprintf(spdTxt, "Rych:%dms", GenDelayMs);
        DrawText(spdTxt, 65, 274, COL_CYAN); 
        
        if (BrushState == BRUSH_ALIVE) DrawText("St:ZIVA", 160, 274, COL_WHITE);
        else if (BrushState == BRUSH_DEAD) DrawText("St:MRTVA", 160, 274, COL_BLACK);
        else DrawText("St:VYP", 160, 274, 0x333333);

        char slotUi[16];
        sprintf(slotUi, "SD:%02d", CurrentSlot);
        DrawText(slotUi, 260, 274, COL_YELLOW);

        char genTxt[64];
        sprintf(genTxt, "Generace: %u  GPS: %d", displayGens, LastGps);
        DrawText(genTxt, 5, 290, COL_YELLOW); 
        
        if (AutoNudgeEnabled) DrawText("Mutace:ZAP", 210, 290, COL_GREEN);
        else DrawText("Mutace:VYP", 210, 290, 0x444444); 

        DrawText("Spc P +/- R U M . N B I", 5, 306, COL_BLACK);
    }

    // Notifikace z SD
    if (SDMessage[0] != 0) {
        if (Time() - SDMessageTime < 2000000) {
            int textWidth = strlen(SDMessage) * 8; 
            DrawRect((320 - textWidth) / 2 - 4, 10, textWidth + 8, 20, COL_RED);
            DrawText(SDMessage, (320 - textWidth) / 2, 12, COL_WHITE);
        } else {
            SDMessage[0] = 0; 
        }
    }

    // Menu
    if (AppState == STATE_MENU) {
        int mw = 240; int mh = 230;
        int mx = (320 - mw) / 2; int my = (320 - mh) / 2;
        
        DrawRect(mx, my, mw, mh, COL_BLUE);
        DrawRect(mx+2, my+2, mw-4, mh-4, COL_BLACK);
        DrawText("--- HLAVNI MENU ---", mx + 40, my + 10, COL_YELLOW);

        const char* menuTexts[MENU_MAX] = {
            "Navrat do hry", "Zmena rozliseni", "Krok vpred", "Krok vzad (Undo)",
            WrapMode ? "Okraje: NEKONECNE" : "Okraje: PEVNE",
            ColorAgeMode ? "Obarvovat dle veku: ZAP" : "Obarvovat dle veku: VYP",
            ShowUI ? "Skryt UI panel" : "Zobrazit UI panel",
            "Vymazat plochu", "Ulozit / Nacist na SD", "Ukoncit"
        };

        for (int i = 0; i < MENU_MAX; i++) {
            u32 tColor = (i == MenuSel) ? COL_BLACK : COL_WHITE;
            u32 bColor = (i == MenuSel) ? COL_YELLOW : COL_BLACK;
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
            u32 tColor = (i == SDMenuSel) ? COL_BLACK : COL_WHITE;
            u32 bColor = (i == SDMenuSel) ? COL_YELLOW : COL_BLACK;
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
            u32 tColor = (i == ResMenuSel) ? COL_BLACK : COL_WHITE;
            u32 bColor = (i == ResMenuSel) ? COL_YELLOW : COL_BLACK;
            DrawRect(mx + 10, y_pos - 2, mw - 20, 16, bColor);
            DrawText(resText, mx + 15, y_pos + 1, tColor);
        }
    }

    SDL_RenderPresent(renderer); 
}

void RandomizeBoard(int density) {
    SaveHistory(); 
    for (int i = 0; i < MapSize; i++) {
        Board[i] = ((rand() % 100) < density) ? 1 : 0; 
    }
    TotalGenerations = 0; 
    IsPlaying = false;    
    ForceRedraw = true;   
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

// Invertace herní plochy zachovávající věk u buněk, co se stávají mrtvými
void InvertBoard() {
    SaveHistory(); 
    for (int i = 0; i < MapSize; i++) {
        Board[i] ^= 1; 
    }
    IsPlaying = false;    
}

int GetCellXFromMouse(int mx) {
    for(int i=0; i<MapW; i++) if(mx >= PosX[i] && mx < PosX[i+1]) return i;
    return MapW - 1;
}
int GetCellYFromMouse(int my) {
    for(int i=0; i<MapH; i++) if(my >= PosY[i] && my < PosY[i+1]) return i;
    return MapH - 1;
}

// ============================================================================
// Hlavní program
// ============================================================================

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;
    if (TTF_Init() == -1) return 1;

    window = SDL_CreateWindow("Game of Life - PC Nástroj (Mutace & Věk)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 960, 960, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderSetLogicalSize(renderer, 320, 320);

    font = TTF_OpenFont(FONT_PATH, 12);

    // Inicializace barev věku pro PC (přepis 16bit palety na 24bitovou)
    for (int i = 0; i < 128; i++) {
        if (i == 0) AgeColors[i] = COL_WHITE;
        else if (i < 10) AgeColors[i] = COL_CYAN; 
        else if (i < 30) AgeColors[i] = COL_GREEN; 
        else if (i < 60) AgeColors[i] = COL_YELLOW; 
        else if (i < 100) AgeColors[i] = COL_ORANGE; 
        else AgeColors[i] = COL_RED; 
    }

    for(int i = 0; i <= MapW; i++) PosX[i] = (i * 320) / MapW;
    for(int i = 0; i <= MapH; i++) PosY[i] = (i * 320) / MapH;
    AllocateMemory();

    bool quit = false;
    SDL_Event e;

    while (!quit) {
        u32 ch = NOKEY;
        
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) quit = true;
            else if (e.type == SDL_KEYDOWN) {
                switch(e.key.keysym.sym) {
                    case SDLK_RIGHT: ch = KEY_RIGHT; break;
                    case SDLK_LEFT:  ch = KEY_LEFT; break;
                    case SDLK_DOWN:  ch = KEY_DOWN; break;
                    case SDLK_UP:    ch = KEY_UP; break;
                    case SDLK_SPACE: ch = ' '; break;
                    case SDLK_RETURN: ch = '\n'; break;
                    case SDLK_ESCAPE: ch = 27; break;
                    case SDLK_PLUS: case SDLK_KP_PLUS: case SDLK_EQUALS: ch = '+'; break;
                    case SDLK_MINUS: case SDLK_KP_MINUS: ch = '-'; break;
                    case SDLK_PERIOD: ch = '.'; break;
                    case SDLK_RIGHTBRACKET: ch = ']'; break;
                    default:
                        if (e.key.keysym.sym >= '0' && e.key.keysym.sym <= '9') ch = e.key.keysym.sym;
                        else if (e.key.keysym.sym >= 'a' && e.key.keysym.sym <= 'z') ch = e.key.keysym.sym;
                        break;
                }
            }
            // Ovládání Myší (Připisování funguje s věkem 0)
            else if (AppState == STATE_GAME && (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEMOTION)) {
                if (e.type == SDL_MOUSEBUTTONDOWN || (e.type == SDL_MOUSEMOTION && e.motion.state != 0)) {
                    int mx = e.button.x; 
                    int my = e.button.y;
                    int uiTopY = (ShowUI) ? (320 - 50) : 320;
                    
                    if (my < uiTopY) { 
                        int cx = GetCellXFromMouse(mx);
                        int cy = GetCellYFromMouse(my);
                        CurX = cx; 
                        CurY = cy;

                        if (e.button.button == SDL_BUTTON_LEFT || (e.motion.state & SDL_BUTTON_LMASK)) {
                            Board[cy * MapW + cx] = 1; 
                        } 
                        else if (e.button.button == SDL_BUTTON_RIGHT || (e.motion.state & SDL_BUTTON_RMASK)) {
                            Board[cy * MapW + cx] = 0; 
                        }
                    }
                }
            }
        }
        
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

                if (ch == KEY_RIGHT || ch == 'd') { CurX++; if (CurX >= MapW) CurX = WrapMode ? 0 : MapW - 1; moved = true; }
                else if (ch == KEY_LEFT || ch == 'a') { CurX--; if (CurX < 0) CurX = WrapMode ? MapW - 1 : 0; moved = true; }
                else if (ch == KEY_DOWN || ch == 's') { CurY++; if (CurY >= MapH) CurY = WrapMode ? 0 : MapH - 1; moved = true; }
                else if (ch == KEY_UP || ch == 'w') { CurY--; if (CurY < 0) CurY = WrapMode ? MapH - 1 : 0; moved = true; }
                
                if (ch == 'q' || ch == '\t') { BrushState++; if (BrushState > BRUSH_DEAD) BrushState = BRUSH_OFF; }
                
                if (ch == KEY_A || ch == ' ') {
                    if (Board[CurY * MapW + CurX] & 1) Board[CurY * MapW + CurX] = 0;
                    else Board[CurY * MapW + CurX] = 1; 
                }

                if (moved) {
                    if (BrushState == BRUSH_ALIVE) Board[CurY * MapW + CurX] = 1; 
                    else if (BrushState == BRUSH_DEAD) Board[CurY * MapW + CurX] = 0; 
                }

                if (ch == KEY_B || ch == 'p' || ch == '\n' || ch == '\r') IsPlaying = !IsPlaying;
                else if (ch == 'r') StepGeneration();
                else if (ch == 'g') TotalGenerations = 0;
                else if (ch == 'u') UndoHistory();
                else if (ch == 'x') ClearBoard();
                else if (ch == KEY_X || ch == 'm') { IsPlaying = false; AppState = STATE_MENU; MenuSel = 0; }
                else if (ch == ']') { LoadFromSD(); AppState = STATE_GAME; }
                else if (ch == '+') { 
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
                else if (ch == 'b') { GridColorIdx++; if (GridColorIdx > 2) GridColorIdx = 0; ForceRedraw = true; }
                else if (ch == 'y') RandomizeBoard(15); 
                else if (ch == 'n') { AutoNudgeEnabled = !AutoNudgeEnabled; ForceRedraw = true; }
                else if (ch == 'i') InvertBoard(); 

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
                if (ch == KEY_DOWN || ch == 's') { MenuSel++; if (MenuSel >= MENU_MAX) MenuSel = 0; }
                else if (ch == KEY_UP || ch == 'w') { MenuSel--; if (MenuSel < 0) MenuSel = MENU_MAX - 1; }
                else if (ch == KEY_B || ch == 27) { AppState = STATE_GAME; ForceRedraw = true; }
                else if (ch == KEY_A || ch == ' ' || ch == '\n' || ch == '\r') { 
                    switch(MenuSel) {
                        case MENU_RESUME: AppState = STATE_GAME; break;
                        case MENU_RESOLUTION: AppState = STATE_RES_MENU; ResMenuSel = CurrentResIndex; break;
                        case MENU_STEP_FWD: StepGeneration(); AppState = STATE_GAME; break;
                        case MENU_STEP_BWD: UndoHistory(); AppState = STATE_GAME; break;
                        case MENU_WRAP: WrapMode = !WrapMode; break; 
                        case MENU_COLOR_AGE: ColorAgeMode = !ColorAgeMode; ForceRedraw = true; break;
                        case MENU_UI_TOGGLE: ShowUI = !ShowUI; ForceRedraw = true; break; 
                        case MENU_CLEAR: ClearBoard(); AppState = STATE_GAME; break;
                        case MENU_SD: AppState = STATE_SD_MENU; SDMenuSel = 0; break;
                        case MENU_EXIT: quit = true; break;
                    }
                }
            }
        }
        else if (AppState == STATE_SD_MENU) {
            if (ch != NOKEY) {
                if (ch == KEY_DOWN || ch == 's') { SDMenuSel++; if (SDMenuSel >= SDMENU_MAX) SDMenuSel = 0; }
                else if (ch == KEY_UP || ch == 'w') { SDMenuSel--; if (SDMenuSel < 0) SDMenuSel = SDMENU_MAX - 1; }
                else if (ch == KEY_RIGHT || ch == 'd') { CurrentSlot += 1; if (CurrentSlot > SLOTS_MAX) CurrentSlot = SLOTS_MAX; }
                else if (ch == KEY_LEFT || ch == 'a') { CurrentSlot -= 1; if (CurrentSlot < 1) CurrentSlot = 1; }
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
                if (ch == KEY_UP || ch == 'w') { ResMenuSel--; if (ResMenuSel < 0) ResMenuSel = ResCount - 1; }
                else if (ch == KEY_DOWN || ch == 's') { ResMenuSel++; if (ResMenuSel >= ResCount) ResMenuSel = 0; }
                else if (ch == KEY_B || ch == 27) AppState = STATE_MENU;
                else if (ch == KEY_A || ch == ' ' || ch == '\n' || ch == '\r') { ChangeResolution(ResMenuSel); AppState = STATE_GAME; }
            }
        }
        
        static int lastAppState = STATE_GAME;
        if (AppState != lastAppState) { ForceRedraw = true; lastAppState = AppState; }
        
        DrawAll();

        if (Time() - GpsTimer >= 1000000) { LastGps = GpsCounter; GpsCounter = 0; GpsTimer = Time(); }
        SDL_Delay(5); 
    }

    if (font) TTF_CloseFont(font);
    FreeMemory();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
