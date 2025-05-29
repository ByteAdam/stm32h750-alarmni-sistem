# STM32H7 Alarmni Sistem

Projekt: alarmni sistem na STM32H750 z ultrazvočnim senzorjem, LED signalizacijo, buzzerjem, tipkami, DMA UART sprejemom in Web Serial API vmesnikom.

---

## Vsebina

- [Opis projekta](#opis-projekta)  
- [Funkcionalnosti](#funkcionalnosti)  
- [Strojna oprema in povezave](#strojna-oprema-in-povezave)  
- [Gradnja in zagon](#gradnja-in-zagon)  
- [Uporaba Web aplikacije](#uporaba-web-aplikacije)  
- [Dnevnik dogodkov & izvoz CSV](#dnevnik-dogodkov--izvoz-csv)  
- [Možne nadgradnje](#možne-nadgradnje)  

---

## Opis projekta

Ta sistem kontinuirano meri razdaljo s HC-SR04 ultrazvočnim senzorjem. Če se zazna predmet bližje kot 100 cm, se sproži 60 s odštevanje z večfazno signalizacijo na trih rdečih LED-ih in buzzerju. Uporabnik lahko alarm deaktivira z 6× pritiskom na gumb 1. Če se ne deaktivira, preide v neprekinjen alarm in po minuti pošlje obvestilo alarmni agenciji.

Podatki (US, T-xx, stanja LED, pritiski gumbov) se pošiljajo prek UART3 (115200 baud) v PuTTY ali WebSerial, kjer jih Web aplikacija prikaže v realnem času. Podpira izvoz serijskega in event loga v CSV.

---

## Funkcionalnosti

- **Ultrazvočno merjenje**: HC-SR04, natančnost ~1 cm  
- **Večfazni alarmni odštevalnik**  
  - 0–5 s: počasno utripanje 3× LED, prvi beep  
  - 5–15 s: ena LED + beep pri 10 s, 15 s  
  - 15–35 s: dve LED + beep vsake 5 s  
  - 35–50 s: tri LED + beep vsake 5 s  
  - 50–60 s: hitro utripanje 3× LED + beep vsako sekundo  
- **Prestrinja TIPKE**: EXTI + debounce (50 ms)  
- **Kode za deaktivacijo**: 6× pritisk gumba 1 = pravilna koda  
- **Neprekinjen alarm**: hiter buzzer + utripanje, dokler ni defusen  
- **DMA UART RX**: neblokirajoč sprejem ukazov BTN1–BTN4  
- **Blocking UART TX**: printf preko HAL_UART_Transmit  
- **WebSerial UI**: prikaz Ready, US, T-xx, G1–G4, LED stanj, event log, barvno ozadje  
- **CSV izvoz**: serijski in event log  

---

## Strojna oprema in povezave

| Funkcija         | Arduino Dn | STM32H7 pin | Dodatno           |
|------------------|------------|-------------|-------------------|
| Ultrazvok TRIG   | —          | PC0         | 2 kΩ serijski upor|
| Ultrazvok ECHO   | —          | PF8         | neposredno input  |
| LED1 (rdeča)     | D8         | PE3         | 220 Ω na anodi    |
| LED2 (rdeča)     | D9         | PH15        | 220 Ω na anodi    |
| LED3 (rdeča)     | D10        | PB4         | 220 Ω na anodi    |
| LED4 (zelena)    | D11        | PB15        | 220 Ω na anodi    |
| Gumb 1           | D7         | PI2 (EXTI2) | pull-up, GND      |
| Gumb 2           | D6         | PE6 (EXTI6) | pull-up, GND      |
| Gumb 3           | D5         | PA8 (EXTI8) | pull-up, GND      |
| Gumb 4           | D4         | PK1 (EXTI1) | pull-up, GND      |
| Buzzer (pasiven) | D2         | PG3         | push-pull output  |
| UART3 TX/RX      | —          | PB10/PB11   | 115200 baud, 8N1  |

---

## Gradnja in zagon

1. **STM32CubeIDE**: odpri `pr1.ioc`  
2. **Configure**: GENERATE CODE (vključi EXTI, DMA1 Stream0 RX, UART3)  
3. **Compiler**: Build → **pr1.elf**  
4. **Flash**: Debug / Run na plošči STM32H750  
5. **Poveži**:  
   - **PuTTY**: COMx @ 115200 8N1  
   - **Chrome + WebSerial**: odpri `index.html`, klikni “Poveži”

---

## Uporaba Web aplikacije

- **Status**: Ready / Odštevanje / Deaktivirano  
- **Prikaz razdalje**: v cm  
- **LED**: grafični kvadratki izklop/vklop  
- **Event log**: spodnje polje z barvnimi dogodki  
- **CSV izvoz**: gumbi “Export Serial” / “Export Events”

---

## Dnevnik dogodkov & izvoz CSV

- `ALARM_TRIGGER` → alarm sprožen (rdeče utripanje)  
- `ALARM_MAIL`    → agencija obveščena (oranžno ozadje)  
- `ALARM_DEFUSED` → alarm deaktiviran (zelena)  
- `CODE_WRONG`    → napačna koda (napačna koda, števči reset)  
- **G1–G4**       → stanje pritiskov gumbov  
- **US:**         → meritve razdalje  
- **T-xx s**      → odštevanje

---

## Možne nadgradnje

- DMA za **UART TX** (non-blocking printf)  
- Kriptografska zaščita **alarm kode**  
- Integracija dodatnih **senzorjev** (temp, gibanje)  
- Oblačne storitve (MQTT, HTTP POST)  
- Mobilni UI (React Native / PWA)

---

## Licenca

Projekt je licenciran pod **MIT License** – prosto ga prilagodi in nadgradi.
