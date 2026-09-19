# Klávesnice TI-92 na PicoCalc Clockwork

Mapování je implementované pouze v projektu TI92 v `src/ti92.cpp`. Názvy vlevo
označují fyzické klávesy PicoCalc, názvy vpravo klávesy původní TI-92.

## Modifikátory

| PicoCalc | TI-92 |
|---|---|
| `Alt` | `2nd` |
| `Sym` | `2nd` |
| levý nebo pravý `Shift` | `Shift` |
| `Ctrl` | `Diamond` |
| `Caps Lock` | `Hand` |

## Funkční a navigační klávesy

| PicoCalc | TI-92 |
|---|---|
| `F1` až `F8` | `F1` až `F8` |
| `F9` | `Apps` |
| šipky | šipky TI-92 |
| `Esc` | `Esc` |
| `Insert` | `sin` |
| `Home` | `cos` (nejde o klávesu TI-92 Home) |
| `Page Up` | `tan` |
| `Page Down` | mocnina `^` |
| `Delete` | `Clear` |
| `End` | druhý Enter TI-92 |
| `Tab` | `Store` |
| `Backspace` | Backspace TI-92 |
| `Enter` | hlavní Enter TI-92 |
| mezerník | mezerník TI-92 |
| zpětný apostrof `` ` `` | `Mode` |

`Graph` není samostatná pevná zkratka: fyzické `F1` až `F8` jsou skutečné
softwarové klávesy TI-92 a jejich význam se mění podle právě otevřené aplikace.

V hotovém 3D grafu se pohled otáčí přímo šipkami. Krátký stisk provede jeden
krok; podržení přibližně jednu sekundu a následné uvolnění spustí souvislou
animaci. `Enter` nebo mezerník animaci zastaví.

## Čísla, písmena a základní operace

| PicoCalc | TI-92 |
|---|---|
| `0` až `9` | `0` až `9` |
| `A` až `Z` | písmena `A` až `Z` |
| `,` | čárka |
| `.` | desetinná tečka |
| `+` | sčítání |
| `-` | odčítání |
| `*` (fyzicky `Shift+8`) | násobení `×` |
| `/` | dělení `÷` |
| `=` | rovná se |
| `(` a `)` | závorky |
| `^` | mocnina |
| `_` | záporné znaménko TI-92 |

U písmen emulátor rozlišuje stav Shiftu: malé písmeno odešle běžnou písmennou
klávesu TI-92, velké písmeno k ní dočasně přidá TI-92 `Shift`.

Znaky z číselné řady `!`, `@`, `#`, `$`, `%` a `&` se překládají na TI-92
`Shift+1`, `Shift+2`, `Shift+3`, `Shift+4`, `Shift+5` a `Shift+7`.

## Znaky tvořené pomocí 2nd

Následující znaky se překládají na příslušnou klávesu TI-92 s automaticky
stisknutým `2nd`:

| PicoCalc | Odeslaná kombinace TI-92 |
|---|---|
| `:` | `2nd` + `theta` |
| `;` | `2nd` + `M` |
| `<` | `2nd` + `0` |
| `>` | `2nd` + `.` |
| `[` | `2nd` + `,` |
| `\\` | `2nd` + `=` |
| `]` | `2nd` + `÷` |
| `{` | `2nd` + `(` |
| `}` | `2nd` + `)` |

## Odmocnina a další funkce 2nd

Odmocnina na původní TI-92 je `2nd` + `×`. Na PicoCalc použijte:

**`Alt` + `Shift` + `8`** nebo **`Sym` + `Shift` + `8`**.

Emulátor tuto trojkombinaci výslovně překládá na `2nd` + `×`; fyzický Shift se
do TI-92 při této kombinaci nepřenese. Ostatní žluté funkce TI-92 lze obdobně
vyvolat podržením `Alt` nebo `Sym` spolu s klávesou, která představuje jejich
základní tlačítko.

## Ukončení emulátoru

**`Ctrl` + `Alt` + `Esc`** ukončí celý emulátor a vrátí se do loaderu.

## Dosud bez samostatné fyzické zkratky

Přímo nejsou přiřazené zejména klávesy TI-92 `ln`, `theta`, `ON` a samostatná
klávesa `Home`. Některé jejich funkce mohou být dostupné přes nabídky nebo přes
sekundární funkce již mapovaných kláves.
