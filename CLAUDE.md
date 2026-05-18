# ColdProff Project Instructions

## Contexto del proyecto

**Objetivo**: Etiqueta adhesiva desechable con sensor de temperatura, geolocalización sin SIM/GPS y readout NFC.

**Hardware core**:
- BC660K-GL NB-IoT (QuecOpen, programable con C embebido)
- ST25DV64K (NFC + EEPROM 8KB)
- TMP117 (sensor temperatura)
- CR2032 (220mAh)
- PCB flexible (Kapton) con antenas embedded

**Geolocalización**: Sin SIM. Modo LIMSRV → AT+QENG → Cell ID (MCC/MNC/LAC/CID) → Google Geolocation API. 

**Datos a guardar en ST25DV64K** (7 días):
- Timestamp
- Temperatura (TMP117)
- MCC/MNC/LAC/Cell ID
- RSRP (signal strength)

## Reglas de codificación

### ⚠️ CRÍTICO: Cell ID es HEXADECIMAL

En Quectel, **TODOS** los identificadores de celda (LAC, Cell ID, TAC) se reportan en **hexadecimal**, incluso si solo contienen dígitos 0-9.

**Correcto**:
```c
uint32_t cid = strtol(cid_str, NULL, 16);  // base 16
uint16_t lac = strtol(lac_str, NULL, 16);
```

**Incorrecto** (causa errores > 100km):
```c
uint32_t cid = atoi(cid_str);  // ERROR: asume decimal
```

Ejemplo: `cid_str = "6936965"` → hex → 110,324,069 decimal (no 6,936,965)

### AT+QENG response structure (LTE)

```
+QENG: "servingcell","FDD",214,03,6936965,235,3050,7,5,5,8CA,18,-98,-11,-72,7,59,5
                     ↑    ↑  ↑ ↑ ↑        ↑   ↑    ↑ ↑ ↑ ↑   ↑  ↑  ↑  ↑  ↑ ↑  ↑
                     RAT  duplex MCC MNC CID PCID EARFCN ...TAC... RSRP ... ...
```

**Parseo en código**:
```c
// AT+QENG response
int mcc, mnc, cid_hex, tac_hex;
sscanf(response, "+QENG: \"servingcell\",\"FDD\",%d,%d,%x,%*d,%*d,%*d,%*d,%*d,%x",
       &mcc, &mnc, &cid_hex, &tac_hex);
// cid_hex y tac_hex ya en decimal después de sscanf con %x
```

## Firmware structure (QuecOpen)

**Main components**:
1. `src/main.c` — Startup, main loop
2. `src/modem.c` — AT command wrapper (AT+QENG, AT+QPSMS, etc)
3. `src/sensor.c` — I2C to TMP117
4. `src/nfc.c` — I2C to ST25DV64K
5. `src/geoloc.c` — Cell ID to URL encoding
6. `include/*.h` — Headers

**Build**: Quectel SDK + CMake or provided Makefile

##sever
Website to run on mobile to be launched using NFC antena that encodes in URL the temperaure data and cellId data. The website calls Geolocalization APIs to retrieve geoloclizations at different moments


## Hardware checklist

- [ ] Schematic: BC660K-GL + ST25DV64K + TMP117 + decoupling
- [ ] Antenna design: NFC spiral + NB-IoT IFA (embedded in PCB traces)
- [ ] PCB layout: Flexible Kapton, keep IFA clear of ground pour
- [ ] Gerber export for manufacturing
- [ ] BOM: €6.10–6.30/unit (no separate antenna components)

## Git workflow

- Main branch: `main` (stable releases)
- Development: `dev` or feature branches
- Firmware: stored in `firmware/quecopen/`
- Hardware: KiCAD files in `hardware/`

## API Keys (regenerate before production)

- ⚠️ Google Geolocation (exposed in chat): Regenerate
- ⚠️ HERE Positioning (exposed in chat): Regenerate
- ⚠️ Unwired Labs (exposed in chat): Regenerate

Never commit `.env` with real keys.

## Testing checklist

- [ ] AT+QENG read without SIM (LIMSRV mode)
- [ ] Cell ID parsed correctly (hex conversion)
- [ ] Google API lookup returns < 1000m accuracy
- [ ] NFC read/write via I2C
- [ ] Temperature sensor I2C communication
- [ ] PSM sleep 800nA verified
- [ ] 7-day battery life simulation

## Battery — LR936 (Sony SR936)

**Energy analysis** (7 days @ 1 reading per 3h):
- Actual consumption: 2.62 mAh total
- Safety margin: **26.7x** with LR936 (70mAh)
- Dimensions: 9.5 × 3.6mm (71% smaller than CR2032)
- Cost: €0.06 per unit (€0.12 savings vs CR2032)

**Note**: PSM sleep 800nA means 99% of time in ultra-low power mode. LR936 is overkill but provides reliability margin.

## Mechanical

- Flexible PCB adhesive label (85×55mm, tarjeta-like)
- Protective cover? (TBD)
- Thermal properties: TMP117 must sense outside temperature through label

---

**Last updated**: 2026-05-17
