# ColdProff Hardware

**Status**: Rev B — BC65 + STM32L010F4P6  
**Revision**: 0.3 — 2026-05-18

## Arquitectura

Dos chips: **BC65** (módem NB-IoT, solo AT commands) + **STM32L010F4P6** (MCU host, gestiona sensores, sueño y AT commands al BC65). Antes se usaba BC660K-GL con QuecOpen (código en el módem); ahora BC65 es más barato y el STM32 hace el trabajo de aplicación.

## Componentes principales

| Componente | Función | Package | Precio (250u) |
|---|---|---|---|
| BC65PB-04-STD | NB-IoT Cat-NB1, AT modem | LCC 17.7×15.8mm | €5.19 |
| STM32L010F4P6 | Host MCU, I2C+UART+RTC | TSSOP-20 | €0.50 |
| ST25DV64KC6 | NFC + EEPROM 8KB | SO8N | €0.85 |
| TMP117MAIDRCR | Sensor temperatura ±0.1°C | SOIC-8 | €1.20 |
| TPS61221DCKR | Boost 1.5V→3.3V | SOT-23-5 | €0.35 |
| LR936 (Sony SR936) | Batería 1.5V 70mAh | 9.5×3.6mm | €0.06 |

**BOM total estimado**: ~€8.54 a 250 unidades. A 1000u+: ~€7.50.

## Archivos

```
hardware/
├── SCHEMATIC.md    — Descripción completa de conexiones por componente
├── LAYOUT.md       — Guía de PCB: placement, routing, antenas
├── README.md       — Este fichero
└── layouts/
    └── coldproff/
        ├── coldproff.kicad_pro
        ├── coldproff.kicad_sch
        └── coldproff.kicad_pcb
```

## Cambios Rev B vs Rev A

| Cambio | Rev A (BC660K-GL) | Rev B (BC65 + STM32) |
|---|---|---|
| Módem | BC660K-GL QuecOpen | BC65 Cat-NB1 (AT only) |
| MCU | Integrado en BC660K | STM32L010F4P6 separado |
| Firmware | QuecOpen FreeRTOS | HAL bare-metal |
| Precio módulo | ~€7.50 | BC65 €5.19 + STM32 €0.50 |
| Ahorro neto | — | ~€1.80/u |
| PCB footprint módulo | 17.7×15.8mm LCC | **idéntico** 17.7×15.8mm |
| Área extra | — | +~60mm² (STM32 TSSOP-20) |
| Corriente sleep | ~0.8µA (BC660K PSM) | ~4.4µA (BC65+STM32+boost) |
| Margen batería | 26× | ~3× (LR936 70mAh) |

> ⚠️ **Nota de consumo**: El boost TPS61221 (15 µA quiescent) domina el reposo. Con BC660K el PSM del módulo era solo 0.8µA; con BC65 (3µA) + STM32 (1.35µA) el total sube a ~19µA. El margen baja de 26× a ~3×. Sigue siendo válido para 7 días, pero considerar:
> - Boost con quiescent menor (e.g. TPS61099: 5µA)
> - O escalar a CR2032 (220mAh) si el footprint lo permite

## Power budget (7 días)

| Estado | I (µA) | Tiempo | Energía |
|---|---|---|---|
| Boost quiescent | 15 | 168h | 2.52mAh |
| BC65 PSM | 3 | 168h | 0.50mAh |
| STM32 STOP | 1.35 | 168h | 0.23mAh |
| BC65 activo | 25,000 | 56×30s = 0.47h | 11.7mAh |
| STM32 activo | 2,000 | 0.47h | 0.94mAh |
| **Total** | | | **~16mAh** |
| LR936 (70mAh) | | | **~4.4× margen** |

## PCB

- **Dimensiones**: 85 × 55 mm (tarjeta)
- **Material**: Kapton flexible (FPC) o FR4 rígida para prototipo
- **Capas**: 2 capas
- **BC65**: mismo footprint LCC que BC660K-GL — los pads no cambian, solo las conexiones
- **STM32**: nuevo componente TSSOP-20 (~6.5 × 4.4mm)
- **Antena NB-IoT**: IFA en traza PCB (70×15mm keep-out sin ground pour)
- **Antena NFC**: espiral 6 vueltas, 50×50mm, traza 0.35mm

## Programación

### STM32 (vía J1 SWD)
- ST-Link v2 o cualquier debugger SWD
- STM32CubeProgrammer / OpenOCD
- Header J1: VCC, SWDIO, GND, SWDCLK, NRST

### BC65 (firmware update)
- Puerto UART del BC65 accesible vía TP1/TP2 (test points)
- Herramienta: Quectel QFlash (Windows)
- El firmware de producción viene preinstalado de fábrica

## Checklist de bring-up

- [ ] Verificar 3.3V en rail (medir después de boost)
- [ ] Verificar ST25DV64K en I2C: `0xA0` ACK a 400kHz
- [ ] Verificar TMP117 en I2C: `0x90` ACK, leer reg 0x0F = 0x0117
- [ ] Verificar BC65: pulso PWRKEY, STATUS=HIGH, `ATI` responde
- [ ] Verificar `AT+CEREG?` devuelve stat=8 (LIMSRV) sin SIM
- [ ] Verificar `AT+QENG="servingcell"` devuelve CID hex válido
- [ ] Verificar RTC STM32 retiene hora en STOP mode (medir con osciloscopio)
- [ ] Verificar NFC: teléfono Android lee URL desde ST25DV64K
- [ ] Medir corriente en reposo (objetivo: <20µA)
