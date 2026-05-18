# ColdProff Firmware — STM32L010F4P6 + BC65

Firmware para el MCU host STM32L010F4P6 que controla el módem BC65 NB-IoT  
mediante comandos AT, lee TMP117 y ST25DV64K por I2C y gestiona el sueño.

## Arquitectura

```
STM32L010F4P6 (host MCU, QuecOpen sustituido por HAL bare-metal)
 │
 ├── USART2 (PA2/PA3) ──► BC65 NB-IoT modem (AT commands)
 │                          ├── AT+QENG → Cell ID / TAC / MCC / MNC / RSRP
 │                          ├── AT+QLTS=1 → hora de red (LIMSRV, sin SIM)
 │                          └── AT+QPSMS → PSM sleep 3h
 │
 ├── I2C1 (PB6/PB7) ──► TMP117  (0x48) — temperatura
 │                   ──► ST25DV64K (0x50) — NFC + EEPROM 8KB
 │
 ├── RTC interno (LSI 37kHz) — despertador cada 3h sin cristal externo
 │
 └── PA13/PA14 — SWD (ST-Link para programar y depurar)
```

## Asignación de pines (STM32L010F4P6, TSSOP-20)

| Pin STM32 | Función              | Descripción                          |
|-----------|----------------------|--------------------------------------|
| PB6       | I2C1_SCL             | Reloj I2C compartido (TMP117+NFC)    |
| PB7       | I2C1_SDA             | Dato I2C compartido                  |
| PA2       | USART2_TX            | → BC65 UART_RX                       |
| PA3       | USART2_RX            | ← BC65 UART_TX                       |
| PA0       | BC65_STATUS (input)  | HIGH = BC65 encendido                |
| PA1       | BC65_PWRKEY (output) | Pulso bajo ≥650 ms para ON/OFF       |
| PA4       | BC65_RESET_N (output)| Reset hardware BC65 (opcional)       |
| PA13      | SWDIO                | Programación ST-Link                 |
| PA14      | SWDCLK               | Programación ST-Link                 |
| NRST      | Reset STM32          | Header J1                            |

## Estructura de ficheros

```
firmware/stm32/
├── App/
│   ├── Inc/
│   │   ├── coldproff_log.h   # Macros LOG_I/LOG_E/LOG_D (sin coste en producción)
│   │   ├── modem.h           # BC65 driver + CellData_t struct
│   │   ├── sensor.h          # TMP117 driver
│   │   ├── nfc.h             # ST25DV64K driver
│   │   ├── geoloc.h          # URL encoding
│   │   └── ndef.h            # NDEF Type 4 Tag
│   └── Src/
│       ├── main.c            # Init HAL, RTC, loop de medición, STOP mode
│       ├── modem.c           # AT commands: QENG, QLTS, QPSMS, PWRKEY
│       ├── sensor.c          # TMP117 via HAL_I2C_Mem_Read
│       ├── nfc.c             # ST25DV64K via HAL_I2C_Master_Transmit/Receive
│       ├── geoloc.c          # JSON y URL encoding (sin deps de plataforma)
│       └── ndef.c            # NDEF TLV encoding (sin deps de plataforma)
├── Core/                     # Generado por STM32CubeMX (no incluido aquí)
│   └── Src/
│       ├── stm32l0xx_hal_msp.c
│       ├── stm32l0xx_it.c    # Añadir llamada a HAL_RTCEx_WakeUpTimerIRQHandler
│       └── system_stm32l0xx.c
└── README.md
```

## Setup con STM32CubeMX

1. **Crear proyecto** → MCU: STM32L010F4P6
2. **RCC**: LSI habilitado (reloj RTC, no necesita cristal externo)
3. **RTC**: Clock Source = LSI, WakeUp Timer habilitado con IRQ
4. **I2C1**: Fast Mode 400 kHz, PB6/PB7
5. **USART2**: Async 9600 baud (BC65 default), PA2/PA3, no DMA
6. **GPIO**:
   - PA0 → Input, Pull-Up (BC65_STATUS)
   - PA1 → Output Push-Pull, High (BC65_PWRKEY, idle=HIGH)
   - PA4 → Output Push-Pull, High (BC65_RESET_N, idle=HIGH)
7. **Project Manager** → generar código (HAL, C)
8. Copiar `App/Inc/*.h` y `App/Src/*.c` en el proyecto

## Compilación (STM32CubeIDE)

```
# Importar proyecto generado por CubeMX
# Añadir App/Src/ al build path
# Definir DEBUG para logging (opcional)
# Build Release para producción
```

## Programación (ST-Link v2)

Header J1 (5 pines):
```
1: VCC (3.3V)
2: SWDIO  (PA13)
3: GND
4: SWDCLK (PA14)
5: NRST
```

```bash
# Con STM32CubeProgrammer o openocd:
openocd -f interface/stlink.cfg -f target/stm32l0.cfg \
        -c "program build/coldproff.elf verify reset exit"
```

## Flujo de ejecución

```
1. Encendido → init HAL + RTC (LSI)
2. Encender BC65 (pulso PWRKEY)
3. Esperar registro LIMSRV (~5s)
4. Obtener hora de red AT+QLTS=1 → actualizar RTC interno
5. Loop (56 ciclos = 7 días):
   a. Encender BC65 (PWRKEY)
   b. AT+QENG → Cell ID/TAC/RSRP
   c. AT+QLTS=1 → timestamp (fallback: RTC interno)
   d. I2C → TMP117 temperatura
   e. I2C → ST25DV64K: guardar registro 18 bytes + actualizar NDEF
   f. AT+QPSMS=1 → BC65 entra PSM
   g. STM32 STOP mode (RTC WakeUp 10800 s = 3h, 1.35µA)
6. 7 días completados → Standby permanente
```

## Timestamps — Estrategia sin RTC externo

El STM32L010 tiene RTC interno (LSI, ±2% precisión).  
Se sincroniza desde BC65 `AT+QLTS=1` (hora de red LTE SIB16, sin SIM) en cada ciclo.

Si no hay hora disponible, el timestamp guardado es 0.  
El servidor extrapola: `T(N) = T_phone - (total - N) × 10800 s`

## Consumo energético (7 días)

| Estado               | Corriente     | Tiempo        | Energía       |
|----------------------|---------------|---------------|---------------|
| Boost converter idle | 15 µA         | 603 h         | 9.05 mAh      |
| BC65 PSM             | 3 µA          | 603 h         | 1.81 mAh      |
| STM32 STOP           | 1.35 µA       | 603 h         | 0.81 mAh      |
| BC65 activo          | ~25 mA        | 56 × 30 s     | ~11.7 mAh     |
| STM32 activo         | ~2 mA         | 56 × 30 s     | ~0.93 mAh     |
| **Total estimado**   |               |               | **~24 mAh**   |
| LR936 (70 mAh)       |               |               | **~2.9× margen** |

> Nota: El boost converter TPS61221 (15 µA quiescent) domina el consumo en reposo.  
> El BC660K-GL (800 nA PSM) era más eficiente; con BC65+STM32 el margen baja de 26× a ~3×.  
> Para recuperar margen: sustituir TPS61221 por boost con menor quiescent (e.g. TPS61221 variante 5 µA),  
> o usar CR2032 (220 mAh) si el espacio lo permite.

## TODO

- [ ] Test AT+QPSMS con BC65 real (verificar timing PSM)
- [ ] Validar RTC LSI drift en 3h (debería ser <±6 min)
- [ ] Test I2C con ST25DV64K a 400 kHz
- [ ] Test NFC readout con teléfono Android
- [ ] Optimizar boost converter (buscar alternativa <5 µA quiescent)

---
**Última actualización**: 2026-05-18  
**MCU**: STM32L010F4P6 · **Modem**: Quectel BC65 (Cat-NB1)
