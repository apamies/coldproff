# ColdProff Firmware — BC660K-GL QuecOpen

Firmware embebido para el BC660K-GL NB-IoT con QuecOpen (FreeRTOS).

## Características

- **AT+QENG**: Lectura de Cell ID sin SIM (modo LIMSRV)
- **I2C dual**: TMP117 sensor + ST25DV64K NFC/EEPROM
- **PSM sleep**: 800nA en sleep mode
- **Almacenamiento**: 7 días de datos (56 lecturas cada 3h)
- **NFC readout**: NDEF Type 4 Tag con geolocalización URL

## Estructura

```
firmware/quecopen/
├── src/
│   ├── main.c          # Entry point, main loop
│   ├── modem.c         # AT commands, cell reading
│   ├── sensor.c        # TMP117 I2C driver
│   ├── nfc.c           # ST25DV64K EEPROM/NFC
│   └── geoloc.c        # Geolocation encoding (pending)
├── include/
│   ├── modem.h
│   ├── sensor.h
│   ├── nfc.h
│   └── geoloc.h
├── build/              # Compiled binaries (generated)
└── README.md
```

## Setup y Compilación

### 1. Descargar SDK de Quectel

El BC660K-GL requiere el **Quectel QCC SDK** con soporte QuecOpen:

```bash
# Descargar desde Quectel (requiere cuenta):
# https://www.quectel.com/developer

# Estructura esperada:
/path/to/qcc_sdk/
├── components/
├── examples/
├── build/
└── Makefile
```

### 2. Configurar Entorno

```bash
export QCC_SDK=/path/to/qcc_sdk
export COMPILER_PATH=/path/to/arm-toolchain  # ARM GCC 9.x

cd firmware/quecopen
```

### 3. Build

Opción A: Usar Makefile de Quectel (si proporciona ejemplo):

```bash
cd build
cmake ..
make -j4
```

Opción B: Usar script de Quectel (si está disponible):

```bash
./build.sh
```

El resultado será un archivo `.elf` o `.bin` que se flashea al BC660K-GL.

## Compilación Manual (sin SDK de Quectel)

Si no tienes el SDK completo, puedes compilar el código en tu PC para verificación:

```bash
# Requiere ARM GCC
arm-none-eabi-gcc -c src/modem.c -o build/modem.o -Iinclude
arm-none-eabi-gcc -c src/sensor.c -o build/sensor.o -Iinclude
arm-none-eabi-gcc -c src/nfc.c -o build/nfc.o -Iinclude
arm-none-eabi-gcc -c src/main.c -o build/main.o -Iinclude

# Linkear (requiere linker script de Quectel)
arm-none-eabi-ld build/*.o -o build/coldproff.elf -T build/script.ld
```

## Flash al dispositivo

Con Quectel QFlash o herramienta equivalente:

```bash
# Ejemplo (ajustar según tu tool):
qflash --port COM3 --image build/coldproff.elf
```

## Testing y Debug

### Monitor UART

```bash
# Conectar a UART2 (AT commands) @ 115200 baud
picocom -b 115200 /dev/ttyUSB2

# Esperar mensajes del firmware:
# [COLDPROFF] === ColdProff FW v0.1 ===
# [MODEM] Initializing modem...
# [SENSOR] Initializing I2C for TMP117...
# etc.
```

### Comandos AT manuales (para debug)

Desde el terminal picocom:

```
ATI                         # Identificar módulo
AT+CEREG?                   # Verificar registro (LIMSRV)
AT+QENG="servingcell"       # Leer Cell ID
AT+QPSMS=1,,,18             # Entrar en PSM
```

## Parseo crítico — Cell IDs HEXADECIMALES

⚠️ **IMPORTANTE**: En Quectel, TODOS los identificadores de celda se reportan en **hexadecimal**, incluso si solo contienen dígitos 0-9.

### Ejemplo real (Cambrils, Orange España LTE)

```
AT+QENG="servingcell"
+QENG: "servingcell","FDD",214,03,6936965,235,3050,7,5,5,8CA,18,-98,-11,-72,7,59,5

Parseo:
- MCC = 214 (decimal) → España
- MNC = 3 (decimal) → Orange
- CID = "6936965" (hex string) → strtol(cid, NULL, 16) = 0x6936965 = 110324069 decimal
- TAC = "8CA" (hex string) → strtol(tac, NULL, 16) = 0x8CA = 2250 decimal
```

**Google Geolocation con valores correctos**:
```json
{
  "radioType": "lte",
  "cellTowers": [{
    "mobileCountryCode": 214,
    "mobileNetworkCode": 3,
    "locationAreaCode": 2250,
    "cellId": 110324069
  }]
}

Respuesta:
{
  "location": {"lat": 41.2341, "lng": 1.2341},
  "accuracy": 337
}
```

**ERROR COMÚN** (si se parsea como decimal):
```
cellId = 6936965 (decimal) → No encontrado en Google DB → Error
```

## Estructura de datos en EEPROM (ST25DV64K)

Cada lectura ocupa **18 bytes**:

```
Byte 0-3:   Timestamp (uint32_t, little-endian)
Byte 4-5:   Temperatura raw (int16_t)
Byte 6-7:   MCC (uint16_t)
Byte 8-9:   MNC (uint16_t)
Byte 10-11: TAC (uint16_t, hex)
Byte 12-15: Cell ID (uint32_t, hex)
Byte 16-17: RSRP (int16_t) + flags

Total: 18 bytes/record × 56 lecturas = 1008 bytes ~ 8KB disponibles
```

## Flujo de ejecución

1. **Startup** (en `main.c`):
   - Inicializar I2C (TMP117 + ST25DV64K)
   - Inicializar UART (AT commands)
   - Verificar registro sin SIM (LIMSRV mode)

2. **Loop principal** (cada 3 horas):
   - Leer `AT+QENG="servingcell"` → MCC/MNC/LAC/CID/RSRP
   - Leer TMP117 → temperatura
   - Escribir ambos a ST25DV64K via I2C
   - Entrar en PSM sleep (800nA) por ~3h
   - Despertar y repetir

3. **Lectura NFC**:
   - Teléfono toca ST25DV64K (NFC)
   - Lee NDEF Type 4 Tag con datos + URL de geolocalización
   - App procesa URL: `https://example.com/api/geoloc?cid=110324069&tac=2250&...`

## APIs de geolocalización

### Google Geolocation API

```c
POST https://www.googleapis.com/geolocation/v1/geolocate?key=YOUR_KEY

Request:
{
  "radioType": "lte",
  "cellTowers": [{
    "mobileCountryCode": mcc,
    "mobileNetworkCode": mnc,
    "locationAreaCode": tac,
    "cellId": cell_id
  }]
}

Response:
{
  "location": {"lat": 41.2341, "lng": 1.2341},
  "accuracy": 337
}
```

**Precisión en Cambrils**: ~337m (LTE), ~902m (WCDMA)

### HERE Positioning API v2

```c
POST https://positioning.api.here.com/v2/geolocate?apiKey=YOUR_KEY

Request (LTE):
{
  "lte": [{
    "mcc": mcc,
    "mnc": mnc,
    "cid": cell_id
  }]
}

Response:
{
  "location": {"lat": 41.2341, "lng": 1.2341},
  "accuracy": 411
}
```

## TODO

- [ ] Implementar `geoloc.c` (codificación JSON/URL)
- [ ] Integración de timestamp real (usar RTC o QTIME)
- [ ] Manejo de errores de I2C (reintentos)
- [ ] Configuración de antena NB-IoT (tuning)
- [ ] NDEF Type 4 Tag encoding para NFC
- [ ] Tests unitarios
- [ ] Optimización de consumo (verificar PSM 800nA)

## Referencias

- [BC660K-GL Datasheet](https://www.quectel.com/product)
- [TMP117 Datasheet](https://www.ti.com/product/TMP117)
- [ST25DV64K NDEF Type 4 Tag](https://www.st.com/resource/en/datasheet/st25dv64k.pdf)
- Quectel QuecOpen Documentation (incluida en QCC SDK)

---

**Última actualización**: 2026-05-17
**Estado**: Base structure ready, needs SDK integration for compilation
