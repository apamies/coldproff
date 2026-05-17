# ColdProff — Disposable Temperature Sensor Label with Cell-Based Geolocation

Etiqueta adhesiva desechable con sensor de temperatura, geolocalización por Cell ID y readout NFC.

## Especificación del producto

- **Hardware**: BC660K-GL NB-IoT SoC con QuecOpen (programable embebido)
- **Sensor**: TMP117 ±0.1°C
- **Almacenamiento**: ST25DV64K NFC + EEPROM 8KB
- **Batería**: LR936 (70 mAh) — 26.7x margen de seguridad, 71% más pequeña
- **Duración**: 7 días de lecturas cada 3h (consumo real: 2.62 mAh)
- **Geolocalización**: Sin SIM, sin GPS — sólo Cell ID + Google/HERE API
- **PCB**: Flexible (Kapton) 85×55mm, antenas embedded (NFC + NB-IoT)
- **Costo estimado**: €5.98–6.18/unit (ahorro €0.12 por unidad con LR936)

## Estructura del repositorio

```
ColdProff/
├── firmware/              # QuecOpen C embedded
│   └── quecopen/
│       ├── src/          # Main code
│       ├── include/      # Headers
│       └── build/        # Compiled binaries
├── hardware/             # Schematic + PCB layout
│   ├── schematics/       # KiCAD .kicad_sch
│   ├── layouts/          # KiCAD .kicad_pcb
│   └── gerbers/          # Manufacturing files
├── mechanical/           # 3D enclosure, label geometry
│   ├── 3d/              # STEP/STL
│   ├── cad/             # FreeCAD/Fusion 360
│   └── renders/         # Preview images
├── docs/                 # Especificaciones, datasheets
└── README.md
```

## Flujo de trabajo

1. **Firmware QuecOpen** (C/FreeRTOS):
   - Startup: AT+QENG read cell info
   - Log: MCC/MNC/LAC/Cell ID + temperatura + RSRP
   - Sleep: PSM 800nA
   - NFC: I2C to ST25DV64K, RF_ACTIVITY interrupt

2. **Hardware**:
   - Diseño en KiCAD (esquema + PCB flexible)
   - Fabricante: PCB flexible con antenas embedded (Kapton + copper traces)
   - Soldering: BC660K-GL, ST25DV64K, TMP117 en SMD 0402-0603

3. **Mechanical**:
   - Envolvente adhesiva flexible, geometría para etiqueta estándar
   - Diseño 3D para verification
   - Tolerancias: ±0.5mm (etiqueta de un solo uso)

## Geolocalización sin SIM

Modo **LIMSRV** (Limited Service):
- EC25 pruebado: Cell ID + TAC sin SIM ✓
- Precision: Google ~337m (LTE), ~902m (WCDMA)
- Ventaja: No requiere contrato, costo €0 en licencia móvil

### Parseo crítico

**⚠️ IMPORTANTE**: Cell ID en Quectel es **HEXADECIMAL** aunque solo contenga dígitos 0-9:
```c
// Correcto
uint32_t cid = strtol(cid_str, NULL, 16);  // base 16
uint16_t lac = strtol(lac_str, NULL, 16);

// Incorrecto
uint32_t cid = atoi(cid_str);  // ERROR: asume decimal
```

## APIs de geolocalización testeadas

| Servicio | Precision (LTE) | Coverage | Costo |
|----------|-----------------|----------|-------|
| Google Geolocation API | 337m ✓ | >99% Europa | €0.004/req |
| HERE Positioning v2 | 411m ✓ | Muy alta | API key |
| Unwired Labs OpenCellID | 10km | Variable | €0.01/req |
| Combain | No encontrada | Limitada | Trial |

**Recomendación**: Google + fallback HERE

## Checklist inicial

- [ ] Firmware QuecOpen básico (AT+QENG, log, sleep)
- [ ] Schematic KiCAD (BC660K-GL, ST25DV64K, TMP117)
- [ ] PCB layout flexible (NFC coil + NB-IoT IFA embedded)
- [ ] Archivo Gerber para fabricante
- [ ] 3D model para mechanical fit check
- [ ] Protocolo de comunicación NFC

---

**Última actualización**: 2026-05-17  
**Estado**: Proyecto iniciado — estructura preparada
