# ColdProff Hardware — KiCAD Design Specification

## Schematic Overview

**Microcontroller**: Quectel BC660K-GL (NB-IoT SoC with QuecOpen)
**MCU Package**: QFN88 (14×14mm)
**Main Voltage**: 2.8V (LR936 nominal)
**Power Supply**: LDO regulator (3.3V if needed for peripherals)

## Block Diagram

```
LR936 (70mAh, 1.5V)
  ↓
[LDO: TPS70233 or similar] → 3.3V rail
  ↓
┌─────────────────────────┬──────────────┬──────────────┐
│                         │              │              │
BC660K-GL            TMP117          ST25DV64K       Reset/Debug
(NB-IoT SoC)      (Temp sensor)    (NFC+EEPROM)       UART
│                   │ I2C            │ I2C
├─ UART_TX       (0x48)            (0x50)
├─ UART_RX                              
├─ GPIO (NFC RF_ACTIVITY interrupt)
├─ SPI/I2C (system)
├─ GND
└─ VDD (1.8V-3.6V)
```

## Pinout & Connections

### BC660K-GL (QFN88 — 14×14mm, 0.5mm pitch)

**Power Pins**:
| Pin | Signal | Description |
|-----|--------|-------------|
| 1,5,9,13,17,21,25,29 | VDD | Supply 2.8-3.3V (tie together, bulk cap 100µF) |
| 2,6,10,14,18,22,26,30 | GND | Ground (tie together, star ground) |
| 24,28 | VCORE_RX | RF RX supply (2.8V via 10µF cap) |
| 3,7,11,15,19,23,27,31 | VCORE_TX | RF TX supply (2.8V via 10µF cap) |

**RF Antenna**:
| Pin | Signal | Description |
|-----|--------|-------------|
| 4,8 | ANT | NB-IoT antenna (printed IFA on PCB, 50Ω impedance) |
| 12 | GND_ANT | Antenna ground |

**UART (AT Commands)**:
| Pin | Signal | Description |
|-----|--------|-------------|
| 35 | UART_RX | RX from FTDI/USB (3.3V TTL) |
| 36 | UART_TX | TX to FTDI/USB (3.3V TTL) |

**I2C (Sensors)**:
| Pin | Signal | Description |
|-----|--------|-------------|
| 48 | I2C_SDA | I2C data line (TMP117 + ST25DV64K + pull-ups 2.2kΩ) |
| 49 | I2C_SCL | I2C clock line |

**GPIO (NFC Interrupt)**:
| Pin | Signal | Description |
|-----|--------|-------------|
| 52 | GPIO_A | RF_ACTIVITY interrupt from ST25DV64K (wakes MCU from PSM) |

**SPI/Debug** (optional for firmware upload):
| Pins | Signal | Description |
|-----|--------|-------------|
| 69-72 | SPI_CLK/MOSI/MISO | Programming SPI (if using Quectel download tool) |
| 73 | RESET_N | Active-low reset (10µF cap + 10kΩ pull-up) |

### TMP117 (6-pin SOIC/DFN)

**Address Pins**:
- A0 (pin 5): GND (fixed address 0x48)
- A1 (pin 6): GND (fixed address 0x48)

**Connections**:
| Pin | Signal | Destination |
|-----|--------|-------------|
| 1 | GND | Star ground |
| 2 | SDA | BC660K I2C_SDA (with 2.2kΩ pull-up to 3.3V) |
| 3 | SCL | BC660K I2C_SCL (with 2.2kΩ pull-up to 3.3V) |
| 4 | VDD | 3.3V supply (100nF cap to GND) |
| 5,6 | A0,A1 | GND (fixed address 0x48) |

### ST25DV64K (14-pin TSSOP or 12-pin QFN)

**I2C (E2 Interface)**:
| Pin | Signal | Connection |
|-----|--------|------------|
| 1 (E2_GND) | GND | Star ground |
| 2 (E2_SDA) | I2C SDA | BC660K I2C_SDA (shared with TMP117) |
| 3 (E2_SCL) | I2C SCL | BC660K I2C_SCL (shared with TMP117) |
| 4 (E2_VCC) | VDD | 3.3V supply (100nF cap to GND) |

**NFC Interface (RF)**:
| Pin | Signal | Connection |
|-----|--------|------------|
| 5,6 | RF_IN | Connected to phone NFC antenna (via capacitive coupling) |
| 7 (RF_VSS) | GND | Ground plane |
| 8 (RF_VCC) | 3.3V | RF supply (no extra cap, internal) |

**Interrupt**:
| Pin | Signal | Connection |
|-----|--------|------------|
| 9 (RF_ACTIVITY) | GPIO_A | BC660K GPIO_A (wakes from PSM) |

**Pull-down Resistor**:
| Component | Value | Purpose |
|-----------|-------|---------|
| R_PD | 100kΩ | Pull-down on RF_ACTIVITY (ensures low when inactive) |

## Power Supply Design

### LR936 Battery (1.5V nominal, 70mAh)

**Battery Holder**: SMD vertical holder (Molex 502456 or equivalent)

```
LR936 (+) → Fuse 250mA (PTC) → LDO input
LR936 (-) → GND
```

### LDO Regulator: TPS70233 (3.3V @ 500mA)

**Purpose**: Step up from 1.5V LR936 to 3.3V for BC660K, TMP117, ST25DV64K

**Schematic**:
```
LR936 (+) ---[Fuse 250mA]--- VOUT (TPS70233) --- 3.3V Rail
  |                              |
  ├─ IN (TPS70233)          GND (TPS70233) --- GND
  └─ GND
```

**Component Values**:
| Component | Value | Package | Note |
|-----------|-------|---------|------|
| C_IN | 10µF | 0603 | Input cap (ceramic, DC bias rated) |
| C_OUT | 22µF | 0603 | Output cap (ceramic, DC bias rated) |
| R_FB | 200kΩ | 0603 | Feedback divider (if adjustable reg) |
| Fuse | 250mA PTC | 0603 | Overcurrent protection |

**Output Voltage**: 3.3V ± 2%

### Decoupling & Filtering

**BC660K-GL**:
| Location | Cap Value | Package |
|----------|-----------|---------|
| Near VDD pins | 100µF | 1206 (bulk) |
| Near VDD pins | 10µF | 0603 (ceramic, ×4) |
| Near VCORE_RX | 10µF | 0603 |
| Near VCORE_TX | 10µF | 0603 |

**TMP117**: 100nF near VDD
**ST25DV64K**: 100nF near VDD

## I2C Pull-up Resistors

```
       3.3V
        │
       [2.2kΩ]  [2.2kΩ]
        │       │
 SDA ───┴───────┤  SCL ────┴───────┤
        │       │
    (open drain)
        │       │
       GND     GND
```

**Value**: 2.2kΩ (standard for ~400kHz I2C)
**Location**: Near BC660K I2C pins

## UART Debug Interface (optional)

For firmware development/debugging:

| Signal | Pin (BC660K) | USB Adapter (FTDI) |
|--------|---------|----------|
| UART_TX (out) | 36 | RXD |
| UART_RX (in) | 35 | TXD |
| GND | GND | GND |

**Voltage**: 3.3V TTL (FTDI FT232RL)

## NFC Antenna Design

**Type**: Spiral coil printed on PCB (copper trace)

**Specifications**:
- Frequency: 13.56 MHz
- Inductance: ~4-6µH
- Q-factor: > 20
- Impedance: 50Ω (matched to ST25DV64K)

**Layout**:
```
    ┌─ Spiral coil: 5-7 turns
    │  Pista: 0.3-0.5mm
    │  Gap: 0.2mm
    │  Area: 50×50mm (optimal for 4-5cm read range)
    │
    └─ Feed point with tuning capacitor (100-150pF)
```

## NB-IoT Antenna Design

**Type**: Inverted-F Antenna (IFA) on PCB edge

**Specifications**:
- Band: 8 (900 MHz) for Orange Spain
- Electrical length: λ/4 @ 900MHz = ~83mm
- Impedance: 50Ω

**Layout**:
```
┌─────────────────────────────────┐
│                                 │
│  IFA trace:                     │
│  ┌─────────┐                    │
│  │ ~83mm   │ (radiator)         │
│  │         │                    │
│  └────┬────┘ (feed point)       │
│       │ (clearance zone)        │
│       [No copper pour under IFA]│
│                                 │
└─────────────────────────────────┘
```

## PCB Specifications

**Layer Stack** (Flexible PCB):
- Layer 1: Signal + antenna traces
- Layer 2: Ground plane (clear under IFA)
- Substrate: Polyimide (Kapton) 50µm

**Trace Width**: 
- Power: 0.5mm (for currents > 100mA)
- Signal: 0.3mm
- I2C/UART: 0.3mm

**Via Diameter**: 0.3mm (plated)

**Impedance Control**: Not required for this design (low frequency, short traces)

**Ground**: Star ground at battery negative

## Component Placement

**Critical Placement Rules**:

1. **Battery holder**: Bottom edge, easily replaceable
2. **LDO**: Near battery (minimize high-current traces)
3. **BC660K-GL**: Center, with 100µF bulk cap very close
4. **NFC coil**: Top side, 4.5×4.5cm area, clear of all other traces
5. **TMP117**: Exposed to environment (thin solder mask for thermal coupling)
6. **ST25DV64K**: Near NFC coil antenna (tight coupling)
7. **I2C pull-ups**: Near BC660K pins

**Example layout** (85×55mm):
```
Top view (component side):
┌─────────────────────────┐
│ [NFC Coil Spiral]       │
│ ┌─────────────────────┐ │
│ │                     │ │
│ │   ST25DV64K         │ │
│ │   TMP117 (thermal)  │ │
│ │                     │ │
│ └─────────────────────┘ │
│                         │
│ [BC660K in center]      │
│                         │
│ [LDO, caps, pull-ups]   │
│                         │
│ [Battery holder]        │
└─────────────────────────┘
```

## Manufacturing & Assembly

**PCB Fabrication**:
- Vendor: Flexible PCB specialist (e.g., Würth Elektronik, DPL, EUTEC)
- Thickness: 50µm Kapton + 35µm copper (1oz)
- Surface finish: ENIG (for adhesive label reliability)
- Adhesive: PSA (Pressure Sensitive Adhesive) applied post-assembly

**Assembly**:
- Method: Reflow soldering (small quantities: manual + hotplate)
- Solder: Pb-free (SAC305)
- Stencil: Laser-cut for 0.3mm traces

**Inspection**:
- Continuity test (all traces)
- I2C address scan (TMP117 @ 0x48, ST25DV64K @ 0x50)
- UART AT command test (ATI, AT+CEREG?)

## BOM (Bill of Materials)

| Reference | Component | Value/Model | Package | Qty | Unit Cost | Notes |
|-----------|-----------|-------------|---------|-----|-----------|-------|
| U1 | NB-IoT SoC | BC660K-GL | QFN88 | 1 | €3.50 | QuecOpen programmable |
| U2 | NFC + EEPROM | ST25DV64K | TSSOP14 | 1 | €0.80 | Dual-port, 8KB |
| U3 | Temp Sensor | TMP117 | SOI**C6 | 1 | €1.20 | ±0.1°C accurate |
| U4 | LDO Regulator | TPS70233 | SOT23-5 | 1 | €0.25 | 1.5V→3.3V boost |
| BAT1 | Battery | LR936 (Sony SR936) | Holder | 1 | €0.06 | 70mAh, 26.7x margin |
| C1,C2,C3,C4 | Bulk Caps | 10µF 16V | 0603 | 4 | €0.04 | Ceramic |
| C5 | Bulk Cap | 100µF 10V | 1206 | 1 | €0.08 | Ceramic |
| C6,C7 | Filtering | 100nF 16V | 0603 | 2 | €0.02 | TMP117, ST25DV64K |
| C8 | LDO Out | 22µF 10V | 0603 | 1 | €0.04 | Output filter |
| C9 | LDO In | 10µF 10V | 0603 | 1 | €0.04 | Input filter |
| C10 | NFC Tune | 100-150pF | 0603 | 1 | €0.02 | Antenna match |
| R1,R2 | I2C Pull-up | 2.2kΩ | 0603 | 2 | €0.01 | 400kHz I2C |
| R3 | RF_ACTIVITY | 100kΩ | 0603 | 1 | €0.01 | Pull-down |
| F1 | Fuse | 250mA PTC | 0603 | 1 | €0.03 | Protection |
| ANT | NFC Antenna | Printed trace | — | 1 | €0.00 | PCB embedded |
| ANT2 | NB-IoT Antenna | IFA trace | — | 1 | €0.00 | PCB embedded |
| PCB | Flexible PCB | Kapton 50µm | 85×55mm | 1 | €0.50 | ENIG finish + PSA |
| **TOTAL** | — | — | — | — | **€6.18** | Per unit |

## Design Notes

1. **No external MCU**: BC660K-GL runs QuecOpen (embedded C) — no separate microcontroller needed
2. **Dual-port NFC**: ST25DV64K can be read via NFC (phone) OR I2C (BC660K) simultaneously
3. **Antenna embedded**: No discrete antenna components — all traces on PCB (reduces BOM cost & complexity)
4. **PSM optimized**: 800nA sleep current means battery lasts 7 days easily
5. **I2C star ground**: Minimize EMI on sensor signals
6. **Thermal sensor exposed**: TMP117 measures ambient temp through solder mask opening

## Design Files to Create (KiCAD 6.0+)

- `coldproff.kicad_sch` — Schematic
- `coldproff.kicad_pcb` — PCB layout (flexible)
- `coldproff.kicad_prl` — Project settings
- `symbols/` — Custom symbols (BC660K-GL if not in library)
- `footprints/` — Custom footprints (QFN88, flexible connectors)
- `gerbers/` — Manufacturing files (exported for PCB vendor)

---

**Last updated**: 2026-05-17  
**Status**: Specification ready for KiCAD implementation
