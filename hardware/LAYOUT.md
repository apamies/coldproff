# ColdProff PCB Layout Guide — Rev B

**Revision**: 0.3 — 2026-05-18  
**Tool**: KiCad 10.0  
**Board**: 85 × 55 mm, 2 capas, FR4 rígida (prototipo) / Kapton FPC (producción)

---

## Cambio Rev B

Respecto a Rev A (BC660K-GL):
- El módulo BC65 **ocupa el mismo footprint LCC** que BC660K-GL (17.7×15.8mm, 61 pads LCC). Los pads del módulo no cambian — solo cambia el net assignment interno.
- Se añade **STM32L010F4P6** (TSSOP-20, 6.5×4.4mm) cerca del BC65.
- Las conexiones I2C, antenas NFC y NB-IoT no cambian.

---

## Footprints KiCad

| Componente | Footprint KiCad sugerido |
|---|---|
| BC65 | `Quectel_BC65:BC65-LCC` (custom, igual que BC660K-GL) |
| STM32L010F4P6 | `Package_SO:TSSOP-20_4.4x6.5mm_P0.65mm` |
| TMP117 | `Package_SO:SOIC-8_3.9x4.9mm_P1.27mm` |
| ST25DV64K | `Package_SO:SOIC-8_3.9x4.9mm_P1.27mm` |
| TPS61221 | `Package_TO_SOT_SMD:SOT-23-5` |
| Inductancia L1 | `Inductor_SMD:L_0402_1005Metric` |
| C1,C3-C7 | `Capacitor_SMD:C_0402_1005Metric` |
| C2 (tantalo) | `Capacitor_Tantalum_SMD:CP_EIA-3216-12_HandSoldering` |
| R1,R2,R3 | `Resistor_SMD:R_0402_1005Metric` |
| J1 (SWD) | `Connector_PinHeader_1.27mm:PinHeader_1x05_P1.27mm_Vertical` |
| B1 (LR936) | `Battery:BatteryHolder_Keystone_3034_1x9.5mm` |

---

## Stackup (2 capas)

| Capa | Función |
|---|---|
| F.Cu (Top) | Señal + componentes SMD |
| B.Cu (Bottom) | Plano GND (casi completo), señal donde necesario |

Reglas:
- Traza mínima: 0.15 mm
- Via mínima: 0.6 mm drill / 1.0 mm annular
- Clearance: 0.15 mm
- Power (3V3/GND): 0.5 mm
- I2C / UART: 0.25 mm, longitud máx 30 mm

---

## Placement — Vista top (85 × 55 mm)

```
┌─────────────────────────────────────────────────────────────────────┐
│                                                                     │
│  [J1 SWD]    [B1 LR936]  [U1 TPS61221]                            │
│   5p 1.27mm   9.5×3.6mm   SOT-23-5                                 │
│                              L1  C1  C2                             │
│                                                                     │
│         ┌──────────────────┐    ┌──────────────────┐               │
│         │  U3  BC65        │    │  U4  TMP117       │               │
│         │  LCC 17.7×15.8   │    │  SOIC-8           │               │
│         │  (NB-IoT modem)  │    │                   │               │
│         │                  │    │  U5  ST25DV64K    │               │
│         └──────────────────┘    │  SO8N (NFC+EEPROM)│               │
│                                 └──────────────────┘               │
│   [U2 STM32]                                                        │
│   TSSOP-20                                                          │
│   R1,R2 (I2C pullups)   R3 (UART)                                  │
│                                                                     │
│ ══════════ IFA NB-IoT (70×15mm, parte derecha, sin GND) ══════════ │
│                                                                     │
│ ╔══════════════════════════════════════════════════════╗           │
│ ║  NFC coil spiral (6 vueltas 50×50mm, parte inferior)  ║           │
│ ╚══════════════════════════════════════════════════════╝           │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Prioridades de placement

1. **BC65** (U3): centrado-izquierda. ANT_MAIN conecta a IFA por la derecha.
2. **STM32L010** (U2): junto a BC65, arriba-izquierda. PA2/PA3 cerca del BC65.
3. **TMP117** (U4) y **ST25DV64K** (U5): lado derecho, I2C corto hacia U2.
4. **TPS61221** (U1): cerca de la batería (arriba-izquierda).
5. **NFC coil**: parte inferior del PCB, zona libre de ground pour.
6. **J1 SWD**: borde del PCB, accesible con pinzas de pogo.

---

## Routing crítico

### I2C (400 kHz)
- SDA y SCL: traza 0.25mm, paralelas, separadas ≥0.3mm
- Pull-ups R1/R2 (2.2kΩ): lo más cerca posible de U2 PB6/PB7
- Longitud total: < 25mm (sin stub)
- Evitar pasar bajo BC65 o IFA

### UART STM32 ↔ BC65
- PA2 (TX) → R3 (33Ω serie) → BC65 UART_RX
- PA3 (RX) ← BC65 UART_TX
- Traza 0.25mm, longitud < 20mm

### Power (+3V3)
- Traza 0.5mm de TPS61221 VOUT → C2 (tantalo cerca) → distribución
- Via de GND bajo C2 tantalo a plano B.Cu
- Decoupling 100nF en cada IC, lo más cerca posible de cada VDD pin

### BC65 PWRKEY y STATUS
- PWRKEY (PA1): traza 0.2mm directa, no requiere protección especial
- STATUS (PA0): ídem

---

## Antena NB-IoT (IFA — Inverted F Antenna)

- Área: 70 × 15 mm (esquina derecha del PCB)
- **Keep-out zona F.Cu**: sin componentes SMD ni copper pour en 70×15mm
- **Keep-out zona B.Cu**: sin GND pour en la proyección de la antena
- Feed point: conectar a ANT_MAIN del BC65 con traza de impedancia 50Ω (~0.9mm en FR4 2-layer sin GND bajo)
- Matching: π-network 50Ω (poblado en prototipo, ajustar con VNA)

> El BC65 ya incluye circuito de matching interno en algunos bands. Verificar con datasheet BC65 Hardware Design.

---

## Antena NFC (Espiral diferencial 13.56 MHz)

- Topología: 6 vueltas, espiral rectangular 50×50 mm
- Traza: 0.35 mm, gap 0.20 mm
- Capa: F.Cu
- Conexiones: RF_A1 y RF_A2 del ST25DV64K (diferencial)
- C_tune: 120 pF entre RF_A1 y RF_A2 (justo en los pads del ST25DV64K)
- Sin ground pour bajo la bobina NFC (ambas capas, zona 50×50mm)
- Colocar ST25DV64K en el centro de la bobina o junto a ella

**Cálculo inductancia aproximada** (6 vueltas, 50×50mm):
- L ≈ 3-4 µH → resonancia 13.56 MHz con C ≈ 33-100 pF
- Ajustar C_tune con spectrum analyzer + smartphone como sonda

---

## Reglas de diseño KiCad (Board Setup)

```
Design Rules:
  Min track width:     0.15 mm
  Min via drill:       0.6 mm
  Min via annular:     0.3 mm (pad total 1.0 mm)
  Min clearance:       0.15 mm
  Min hole to hole:    0.25 mm

Net Classes:
  Default: width=0.20mm, clearance=0.15mm
  Power:   width=0.50mm, clearance=0.20mm
  RF:      width=0.90mm (50Ω IFA feed), clearance=0.30mm
```

---

## Gerbers para fabricación

Exportar desde KiCad (Plot):
- F.Cu, B.Cu (cobre)
- F.Mask, B.Mask (soldermask)
- F.Silks, B.Silks (serigrafía)
- Edge.Cuts (contorno)
- Excellon drill file

JLCPCB / PCBWay: subir ZIP con todos los Gerbers.  
Para prototipo FR4 estándar: 1.6mm, HASL, 2oz Cu.

---

## Bring-up sequence

1. Alimentar solo desde banco (no LR936 aún): verificar 3.3V en rail
2. Medir corriente estática (sin MCU programado): debe ser < 1mA
3. Conectar ST-Link → programar STM32 con firmware debug
4. Monitor UART debug (SWO o UART) → verificar init OK
5. Verificar I2C: `sensor_init` y `nfc_init` deben retornar 0
6. Verificar BC65: `modem_power_on()` → STATUS=HIGH, ATI responde
7. Verificar LIMSRV: `AT+CEREG?` → stat=8
8. Verificar NFC: tocar con teléfono → debe abrir URL
9. Medir corriente en STOP mode (objetivo: < 20µA total)
