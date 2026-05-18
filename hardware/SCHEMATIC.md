# ColdProff Schematic Reference — Rev B

**Revision**: 0.3 — 2026-05-18  
**Arquitectura**: BC65 (AT modem) + STM32L010F4P6 (host MCU)  
**Sin SIM · Sin GPS · Sin RTC externo · Sin cristal de cuarzo**

---

## Diagrama de bloques

```
 LR936 (1.5V)
    │
 TPS61221DCKR ── +3V3 ───────────────────────────────────────┐
 (boost 3.3V)                                                 │
                ┌──────────────────────────────────────┐      │
                │ STM32L010F4P6 (TSSOP-20) — host MCU  │◄─────┘
                │                                      │
                │ PB6/PB7 ── I2C1 ──┬── TMP117 (0x48) │
                │                   └── ST25DV64K(0x50)│
                │                         │ NFC RF     │
                │ PA2/PA3 ── USART2 ─────► BC65 NB-IoT │
                │ PA0 ──── BC65_STATUS ←──┘            │
                │ PA1 ──── BC65_PWRKEY ───►             │
                │ PA13/14 ─ SWD (J1)                   │
                └──────────────────────────────────────┘
```

---

## U1 — TPS61221DCKR (Boost Converter, SOT-23-5)

Convierte 1.5V (LR936) a 3.3V regulados. Quiescent: 15 µA.

| Pin | Nombre | Conexión                         |
|-----|--------|----------------------------------|
| 1   | LX     | L1 (4.7 µH) → VOUT              |
| 2   | GND    | GND                              |
| 3   | VIN    | BATT+ ; C1 (1 µF) a GND         |
| 4   | EN     | VOUT (siempre habilitado)        |
| 5   | VOUT   | +3V3 rail ; C2 (100 µF) a GND   |

Componentes auxiliares:
- L1: 4.7 µH, DCR < 0.5 Ω (Murata LQM2HPN4R7MG0 o similar)
- C1: 1 µF / 10V X5R 0402
- C2: 100 µF / 6.3V tantalo low-ESR (Kemet T491)
- C3: 100 nF / 10V X7R 0402 (bypass adicional VOUT)

---

## U2 — STM32L010F4P6 (Host MCU, TSSOP-20)

16 KB flash, 2 KB RAM, RTC interno LSI (37 kHz, ±2%), 1.35 µA STOP mode.

| Pin | GPIO  | Función            | Descripción                        |
|-----|-------|--------------------|------------------------------------|
| 1   | PB6   | I2C1_SCL           | Pull-up R2 (2.2 kΩ) a +3V3        |
| 2   | PB7   | I2C1_SDA           | Pull-up R1 (2.2 kΩ) a +3V3        |
| 3   | PC14  | —                  | NC (LSE externo opcional)          |
| 4   | PC15  | —                  | NC                                 |
| 5   | NRST  | Reset MCU          | J1 pin 5, C_RST 100 nF a GND      |
| 6   | VDDA  | Analog supply      | +3V3 ; C 10 nF a GND              |
| 7   | PA0   | BC65_STATUS input  | Pull-up interno, HIGH=BC65 ON      |
| 8   | PA1   | BC65_PWRKEY output | Output Push-Pull, idle HIGH        |
| 9   | PA2   | USART2_TX          | → BC65 UART_RX (serie 33 Ω)       |
| 10  | PA3   | USART2_RX          | ← BC65 UART_TX                    |
| 11  | PA4   | BC65_RESET_N output| Output Push-Pull, idle HIGH        |
| 12  | VSS   | GND                |                                    |
| 13  | VDD   | +3V3               | C_bypass 100 nF 0402 a GND        |
| 14-18 | PA5-PB1 | —              | NC (reservados)                    |
| 19  | PA13  | SWDIO              | J1 pin 2                           |
| 20  | PA14  | SWDCLK             | J1 pin 4                           |

---

## U3 — BC65PB-04-STD (NB-IoT Cat-NB1, LCC 17.7 × 15.8 mm)

Módem Quectel BC65, solo comandos AT. Mismo footprint LCC que BC660K-GL.  
LIMSRV mode: lee Cell ID sin SIM vía AT+QENG. Hora vía AT+QLTS=1 (SIB16).

| Señal BC65    | Conectado a         | Descripción                      |
|---------------|---------------------|----------------------------------|
| VCC (×3)      | +3V3                | Alimentación (verificar datasheet)|
| GND (×5)      | GND                 |                                  |
| MAIN_TX       | STM32 PA3 (RX)      | Respuesta AT                     |
| MAIN_RX       | STM32 PA2 (TX)      | Comando AT                       |
| PWRKEY        | STM32 PA1           | Pulso LOW ≥650 ms → toggle ON/OFF|
| STATUS        | STM32 PA0           | HIGH = módulo encendido          |
| RESET_N       | STM32 PA4           | LOW = reset (opcional)           |
| ANT_MAIN      | J2 o trace IFA      | Antena NB-IoT                    |

> **⚠️ CRÍTICO**: CID y TAC en AT+QENG son **hexadecimales**.  
> Parsear con `strtoul(str, NULL, 16)`. Error típico: 6936965 hex ≠ 6936965 decimal.

---

## U4 — TMP117MAIDRCR (Temperatura, SOIC-8)

I2C addr 0x48 (ADD0=GND, ADD1=GND). HAL DevAddr = 0x90.

| Pin | Nombre | Conexión                   |
|-----|--------|----------------------------|
| 1   | SDA    | I2C1 bus                   |
| 2   | SCL    | I2C1 bus                   |
| 3   | ALERT  | NC                         |
| 4   | GND    | GND                        |
| 5   | V+     | +3V3 ; C 100 nF a GND     |
| 6   | ADD0   | GND                        |
| 7   | ADD1   | GND                        |
| 8   | NC     | NC                         |

---

## U5 — ST25DV64KC6 (NFC + EEPROM 8KB, SO8N)

I2C EEPROM 0x50 / SYS 0x57. HAL: 0xA0 / 0xAE.

| Pin | Nombre  | Conexión                        |
|-----|---------|---------------------------------|
| 1   | SDA     | I2C1 bus                        |
| 2   | SCL     | I2C1 bus                        |
| 3   | VSS     | GND                             |
| 4   | VCC     | +3V3 ; C 100 nF a GND           |
| 5   | RF_A1   | Antena NFC diferencial pin 1    |
| 6   | RF_A2   | Antena NFC diferencial pin 2    |
| 7   | VSS_RF  | GND plano RF                    |
| 8   | GPO     | NC                              |

Antena NFC: bobina espiral diferencial 6 vueltas, 50×50 mm, 0.35mm traza.  
C_tune = 120 pF entre RF_A1 y RF_A2 (ajustar para resonancia 13.56 MHz).

---

## Red I2C compartida

```
+3V3 ──┬─ R1 2.2kΩ ─── SDA ───┬── U2 PB7 (STM32)
        │                      ├── U4 pin1 (TMP117)
        └─ R2 2.2kΩ ─── SCL ───├── U4 pin2
                                ├── U5 pin1 (ST25DV64K)
                                └── U5 pin2
```

Bus I2C1 compartido: TMP117 (0x48) + ST25DV64K EEPROM (0x50) + SYS (0x57).  
Velocidad: 400 kHz (Fast Mode). No hay conflicto de direcciones.

---

## J1 — Header SWD programación STM32 (5 pines, 1.27 mm)

| Pin | Señal  | Notas                      |
|-----|--------|----------------------------|
| 1   | +3V3   | Referencia para ST-Link    |
| 2   | SWDIO  | PA13                       |
| 3   | GND    |                            |
| 4   | SWDCLK | PA14                       |
| 5   | NRST   | Reset MCU                  |

> Para programar BC65 firmware (update): usar UART test points (TP1/TP2) en PA2/PA3.

---

## B1 — Batería LR936

1.5V, 70 mAh, 9.5 × 3.6 mm. Holder SMD: Keystone 3034 o similar.

---

## BOM completa (250 unidades estimado)

| Ref | Componente        | Package   | €/u   |
|-----|-------------------|-----------|-------|
| U2  | STM32L010F4P6     | TSSOP-20  | 0.50  |
| U3  | BC65PB-04-STD     | LCC       | 5.19  |
| U4  | TMP117MAIDRCR     | SOIC-8    | 1.20  |
| U5  | ST25DV64KC6       | SO8N      | 0.85  |
| U1  | TPS61221DCKR      | SOT-23-5  | 0.35  |
| L1  | 4.7 µH 0402       | 0402      | 0.06  |
| C1  | 1 µF 10V X5R 0402 | 0402      | 0.02  |
| C2  | 100 µF tantalo    | C case    | 0.18  |
| C3-C7 | 100 nF ×5 0402  | 0402      | 0.05  |
| R1,R2 | 2.2 kΩ ×2 0402  | 0402      | 0.02  |
| R3  | 33 Ω serie UART   | 0402      | 0.01  |
| B1  | LR936             | —         | 0.06  |
| J1  | Header SWD 5p     | 1.27 mm   | 0.05  |
| **Total** |             |           | **~€8.54** |

Objectivo €6: requiere BC65 a escala ≥1000u (precio ~€3.50-4.00 directo a Quectel).
