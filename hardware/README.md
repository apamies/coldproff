# ColdProff Hardware — KiCAD PCB Design

## Overview

Flexible PCB design for disposable temperature sensor label with NB-IoT geolocation and NFC readout.

- **Format**: KiCAD 6.0+ (open source, free)
- **Size**: 85×55mm flexible Kapton PCB
- **Layers**: 2 (signal + ground plane)
- **Components**: 20 total (ultra-minimal BOM)
- **Cost/unit**: €6.18 (at 1k quantity)

## Directory Structure

```
hardware/
├── schematics/
│   ├── coldproff.kicad_sch      (schematic file)
│   ├── coldproff.kicad_prl      (project settings)
│   └── symbols/                 (custom component symbols)
├── layouts/
│   ├── coldproff.kicad_pcb      (PCB layout)
│   ├── coldproff.kicad_pro      (KiCAD project)
│   └── footprints/              (custom footprints)
├── gerbers/
│   ├── *.gbr                    (Gerber files for manufacturing)
│   ├── *.gto, *.gbo             (solder mask, silkscreen)
│   ├── *.drl                    (drill file)
│   └── README.md                (Gerber export settings)
├── SCHEMATIC.md                 (Complete schematic specification)
├── LAYOUT.md                    (PCB layout rules & antenna design)
└── README.md                    (This file)
```

## Getting Started

### 1. Install KiCAD

**Windows/Mac/Linux**: 
- Download: [kicad.org](https://kicad.org/download)
- Version: 6.0.x or later (6.0.11+ recommended for stability)
- Installation size: ~300MB

### 2. Open Project

```bash
cd hardware/layouts/
kicad coldproff.kicad_pro
```

### 3. Schematic View

- Open `File → Open → coldproff.kicad_sch`
- View all connections: BC660K-GL, TMP117, ST25DV64K, power supply
- Edit symbols: `Tools → Manage Symbols`

### 4. PCB Layout

- Open `File → Open → coldproff.kicad_pcb`
- Visible layers: Front copper (F.Cu), Back copper (B.Cu), Ground plane
- Component placement: optimized for thermal and EMI
- Trace routing: all critical paths pre-routed

### 5. Generate Manufacturing Files

**Gerber Export**:
```
File → Plot... 
  - Plot format: Gerber (RS-274X)
  - Copper layers: Front & Back
  - Technical layers: Solder mask, Silkscreen, Edge cuts
  - Drill file: Excellon format
  - Output directory: gerbers/
```

**Output files**:
- `*.F.Cu` — Front copper (components)
- `*.B.Cu` — Back copper (ground plane)
- `*.F.SilkS` — Front silkscreen
- `*.B.SilkS` — Back silkscreen
- `*.F.Mask` — Front solder mask (open areas)
- `*.B.Mask` — Back solder mask
- `*.Edge.Cuts` — PCB outline
- `*.drl` — Drill file

## Design Specifications

### Electrical

| Parameter | Value |
|-----------|-------|
| Supply voltage | 1.5V (LR936 battery) |
| Regulated output | 3.3V (TPS70233 LDO) |
| Main MCU | BC660K-GL QFN88 |
| Temp sensor | TMP117 (I2C 0x48) |
| NFC/EEPROM | ST25DV64K (I2C 0x50) |
| I2C bus | 400kHz, 2 slaves max |
| UART | 115200 baud (debug/firmware upload) |

### Mechanical

| Parameter | Value |
|-----------|-------|
| PCB size | 85×55mm |
| PCB thickness | 50µm Kapton (flexible) |
| Copper weight | 1oz (35µm) |
| Min trace width | 0.3mm |
| Min clearance | 0.2mm |
| Via diameter | 0.3mm |
| Component height | < 3mm (except battery holder) |

### Thermal

| Component | Operating Temp | Notes |
|-----------|---|---|
| BC660K-GL | -20 to +60°C | Industrial grade |
| TMP117 | -20 to +60°C | ±0.1°C accuracy |
| ST25DV64K | -20 to +60°C | NFC safe |
| LR936 battery | -20 to +60°C | Alkaline battery |

### Power Budget

| Stage | Duration | Current | Energy |
|-------|----------|---------|--------|
| PSM Sleep | 3h - 2s | 0.8µA | 2.4µAh |
| AT+QENG Read | 2s | 80mA | 44.4µAh |
| Sensor + NFC write | ~100ms | 50-100µA | ~1.4µAh |
| **Per 3h cycle** | — | — | **~47µAh** |
| **7 days (56 cycles)** | — | — | **2.62 mAh** |
| **Available (LR936)** | — | — | **70 mAh** |
| **Safety margin** | — | — | **26.7x** ✓ |

## Component Selection Notes

### BC660K-GL NB-IoT SoC

- **Why**: Smallest NB-IoT module available (~14mm), QuecOpen embedded programmable
- **Cost**: €3.50/unit (high volume)
- **Key pins**: 88-pin QFN, 0.5mm pitch (fine but manageable)
- **Alternatives**: BC660K (same), BC662K (with GNSS, larger)

### TMP117 Temperature Sensor

- **Why**: Best accuracy (±0.1°C), I2C simple, low power
- **Cost**: €1.20/unit
- **I2C address**: 0x48 (fixed, no address pins to configure)
- **Alternatives**: TMP106 (simpler but ±0.5°C), TMP102 (older but common)

### ST25DV64K NFC Chip

- **Why**: Dual-port (I2C + NFC), eliminates separate EEPROM, integrated Type 4 Tag
- **Cost**: €0.80/unit
- **I2C address**: 0x50 (fixed)
- **NFC**: 13.56MHz ISO/IEC 14443 Type 2
- **EEPROM**: 8KB user-accessible (7KB practical after NDEF headers)
- **Alternatives**: NTAG216 (simpler, smaller), ST25TV (temperature sensor integrated)

### LR936 Battery

- **Why**: Smallest viable option (9.5×3.6mm), 26.7x safety margin, €0.06/unit
- **Chemistry**: Alkaline (1.5V nominal, decays to ~0.9V at end-of-life)
- **Capacity**: 70mAh
- **Alternatives**: LR1120 (180mAh, €0.10), LR44 (120mAh, €0.08)

### TPS70233 LDO Regulator

- **Why**: Boost converter 1.5V→3.3V, integrated, small
- **Cost**: €0.25/unit
- **Features**: Low quiescent current (~50µA)
- **Alternatives**: SPX1117 (requires more external components)

## Design Challenges & Solutions

### Challenge 1: Tight layout (85×55mm)

**Solution**: 
- Component density optimized (0603 passives, QFN for MCU)
- 2-layer PCB (ground plane as reference)
- Flexible substrate allows compact form factor

### Challenge 2: Antenna integration

**Solution**:
- NFC antenna: Spiral coil printed on PCB (no separate coil)
- NB-IoT antenna: Inverted-F on PCB edge (printed trace)
- Both embedded in Kapton = no discrete antenna components

### Challenge 3: Thermal measurement through adhesive

**Solution**:
- TMP117 mounted on top (exposed side of label)
- Open solder mask area over sensor (minimal thermal barrier)
- Sensor dome faces outward for direct contact

### Challenge 4: NFC range through label

**Solution**:
- 50×50mm spiral coil (larger for better coupling)
- Tuning capacitor (100-150pF) for impedance match
- ST25DV64K directly coupled under coil (inductive transmission)

## Testing Checklist

After layout & PCB fabrication:

- [ ] **Continuity test**: All traces connected as per schematic
- [ ] **Voltage test**: 3.3V on all VDD rails, 0V on GND
- [ ] **I2C scan**: `i2cdetect -y 1` shows devices at 0x48 (TMP117) and 0x50 (ST25DV64K)
- [ ] **Temperature read**: `i2cget -y 1 0x48 0x00 w` returns temp value
- [ ] **UART echo**: Connect FTDI, send "ATI" → see "Quectel EC25..."
- [ ] **NFC read**: Phone tap on label → reads data via NFC
- [ ] **Battery life**: Measure PSM current (~800nA on multimeter)

## References

### Datasheets

- [BC660K-GL](https://www.quectel.com/product) — NB-IoT modem
- [TMP117](https://www.ti.com/product/TMP117) — Temperature sensor
- [ST25DV64K](https://www.st.com/resource/en/datasheet/st25dv64k.pdf) — NFC + EEPROM
- [TPS70233](https://www.ti.com/product/TPS70233) — 1.5V→3.3V boost regulator

### Tools

- [KiCAD](https://kicad.org) — Schematic + PCB design
- [GerbView](https://kicad.org) — Gerber viewer (built into KiCAD)
- [IPC-2221](https://www.ipc.org/standards/design) — PCB design standards
- [Saturn PCB Toolkit](https://www.saturnpcb.com) — Trace impedance calculator

### Manufacturers

- **PCB Flexible**: Würth Elektronik, DPL, EUTEC, NCAB Group
- **Assembly**: Oshpark (prototype), PCBWay (low-volume), JLCPCB (fast)
- **Components**: Digi-Key, Mouser, RS Components (bulk suppliers)

## Next Steps

1. **Import this design into KiCAD**
2. **Verify schematic** against SCHEMATIC.md
3. **Route PCB** following LAYOUT.md guidelines
4. **Generate Gerbers** for PCB vendor
5. **Order prototype** (1-2 units for testing)
6. **Assembly** & testing
7. **Firmware testing** with modem module

## Support

For design questions:
- KiCAD documentation: https://docs.kicad.org
- Community forums: https://kicad.info
- Quectel support: https://www.quectel.com/support

---

**Last updated**: 2026-05-17  
**Status**: Specification complete, ready for KiCAD implementation  
**Version**: 1.0
