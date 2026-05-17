# ColdProff PCB Layout Guide — Flexible Kapton

## Quick Reference — Pin Connections

### BC660K-GL (QFN88) Pinout Summary

**Essential pins for this design**:

```
BC660K-GL (QFN88 — viewed from top)

         ┌──────────────────────┐
      1  │ VDD                VDD │  88
      2  │ GND                GND │  87
      3  │ VCORE_RX      VCORE_TX │  86
...
     35  │ UART_RX         ANT    │  4
     36  │ UART_TX         GND_ANT│  12
...
     48  │ I2C_SDA         I2C_SCL│  49
     52  │ GPIO_A              ... │
...
     73  │ RESET_N             ... │
      
         └──────────────────────┘
```

**Minimal Connections**:

| Function | BC660K Pin | Connected To | Via |
|----------|-----------|--------------|-----|
| Power | VDD (1,5,9,13,17,21,25,29) | 3.3V LDO | 100µF bulk cap |
| Ground | GND (2,6,10,14,18,22,26,30) | Star GND | — |
| Antenna | ANT (4,8) | NB-IoT IFA trace | 50Ω PCB trace |
| UART RX | 35 | USB FTDI RXD | 3.3V TTL |
| UART TX | 36 | USB FTDI TXD | 3.3V TTL |
| I2C Data | 48 | TMP117 (pin 2) + ST25DV64K (pin 2) | 2.2kΩ pull-up |
| I2C Clock | 49 | TMP117 (pin 3) + ST25DV64K (pin 3) | 2.2kΩ pull-up |
| Wake IRQ | 52 | ST25DV64K RF_ACTIVITY (pin 9) | 100kΩ pull-down |
| Reset | 73 | 10µF cap + 10kΩ pull-up | GND |

### TMP117 (SOIC6 or DFN6) Pinout

```
      Pin 1 (GND) ──→ Star GND
      Pin 2 (SDA) ──→ BC660K pin 48 (shared I2C bus)
      Pin 3 (SCL) ──→ BC660K pin 49 (shared I2C bus)
      Pin 4 (VDD) ──→ 3.3V + 100nF cap
      Pin 5 (A0)  ──→ GND (fixed address 0x48)
      Pin 6 (A1)  ──→ GND (fixed address 0x48)
```

### ST25DV64K (TSSOP14) Pinout

```
I2C Interface (E2 port):
      Pin 1 (GND)      ──→ Star GND
      Pin 2 (SDA)      ──→ BC660K pin 48 (shared I2C bus)
      Pin 3 (SCL)      ──→ BC660K pin 49 (shared I2C bus)
      Pin 4 (VDD)      ──→ 3.3V + 100nF cap

NFC Interface (RF port):
      Pin 5,6 (RF_IN)  ──→ NFC antenna coil (capacitive coupling)
      Pin 7 (GND_RF)   ──→ Ground plane
      Pin 8 (VDD_RF)   ──→ 3.3V (no extra cap needed)

Interrupt:
      Pin 9 (RF_ACTIVITY) ──→ BC660K pin 52 via 100kΩ pull-down
```

### LR936 Battery Holder

```
     Battery (+) ──→ [PTC Fuse 250mA] ──→ TPS70233 VIN
     Battery (-) ──→ GND star point
```

### LDO Regulator TPS70233

```
     VIN  ──→ LR936 + fuse
     GND  ──→ Star GND
     VOUT ──→ 3.3V Rail
     
     C_IN  = 10µF (0603) from VIN to GND
     C_OUT = 22µF (0603) from VOUT to GND
     R_FB  = 200kΩ (if adjustable mode)
```

## PCB Stackup (Flexible)

```
┌─────────────────────────┐
│ Layer 1: Traces & Parts │ ← Signal layer (components soldered here)
├─────────────────────────┤
│ Layer 2: Ground Plane   │ ← Solid ground reference
├─────────────────────────┤
│ Kapton Substrate        │ 50µm thickness
└─────────────────────────┘
```

**Trace Width Guidelines**:
- Power traces (3.3V from LDO to VDD): 0.5mm
- I2C/UART: 0.3mm
- Signal/antenna: varies (see below)

## Critical Layout Rules

1. **Battery holder**: Bottom edge, accessible for replacement
2. **LDO & caps**: Immediately after battery (minimize high-current paths)
3. **BC660K-GL decoupling**:
   - 100µF bulk cap within 5mm of VDD pins
   - 10µF ceramic caps on each corner of chip
4. **I2C pull-ups**: Within 10mm of BC660K pins 48/49
5. **NFC antenna**: Center of top side, 50×50mm spiral coil, **isolated from digital traces**
6. **NB-IoT antenna (IFA)**: Edge of PCB, 83mm radiator, **clearance zone (no copper pour under it)**
7. **TMP117 sensor**: Exposed area (open solder mask) on top for thermal coupling
8. **ST25DV64K**: Close to NFC antenna for inductive coupling
9. **Ground vias**: Frequent vias to ground plane below for EMI suppression

## Antenna Designs (Copper Traces)

### NFC Antenna — Printed Spiral Coil

**Location**: Center of PCB, top layer  
**Size**: 50×50mm (for 4-5cm read range)

```
        Feed point (tune cap here)
             ↓
        ┌────X────┐
        │    │C    │
        │  ┌─┴─┐  │
        │  │   │  │  ← 5 turns (example)
        │  │  spiral │
        │  │  traces │
        │  │  0.3mm  │
        │  │  width  │
        └──┴───────┘
        
        Gap: 0.2mm between turns
        Tune capacitor: 100-150pF (parallel)
```

**Inductance target**: 4-6µH at 13.56 MHz  
**Q-factor**: > 20

### NB-IoT Antenna — Inverted-F (IFA)

**Location**: Edge of PCB, top layer  
**Band**: 8 (900 MHz, Orange Spain)

```
       Radiator arm (~83mm)
             ↑
             │
        ┌────┐
        │    │ Feed point (SMA or trace to pin 4/8)
        │    │
        │   ═══ Matching network (if needed)
        │    │
        └────┴────────────────
            ▲
       Ground plane connection
       (via shorting pin)

        Clearance zone (no copper pour):
        40×20mm rectangle under IFA
```

**Electrical length**: λ/4 @ 900MHz = 83mm  
**Impedance**: 50Ω (matched to BC660K RF port)

## Manufacturing Notes

**File format for PCB fabrication**:
- Export as **Gerber RS-274X** (not PDF/image)
- Include: Front copper, Back copper, Edge cuts, Silkscreen, Solder mask
- Drill file: Excellon format

**Vendor checklist**:
- [ ] Flexible PCB material (Kapton/PI 50µm)
- [ ] Copper weight: 1oz (35µm)
- [ ] Surface finish: ENIG (Electroless Nickel Immersion Gold)
- [ ] Min trace/space: 0.3mm
- [ ] Via diameter: 0.3mm (plated through)
- [ ] PSA (adhesive) applied to bottom layer post-assembly
- [ ] Solder mask: Open areas for TMP117 thermal coupling

**Typical Turnaround**:
- Prototype: 10-15 days
- Production (1000 units): 15-20 days
- Cost: €0.50-0.70 per unit (qty 1k)

---

**Last updated**: 2026-05-17  
**Ready for KiCAD import**
