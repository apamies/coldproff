# ColdProff Server — NFC + Geolocalización

Servidor Node.js + Express para lectura de sensores NFC y consulta de APIs de geolocalización (Google, HERE).

## Características

- **Web app NFC**: Interfaz HTML5 para leer etiquetas ST25DV64K desde el móvil
- **API REST**: Endpoints para geolocalización y almacenamiento de datos
- **Dual provider**: Consulta Google Geolocation + fallback HERE Positioning
- **Cache**: Evita consultas repetidas a las APIs
- **Responsive**: Mobile-first design

## Stack

- **Backend**: Node.js + Express
- **Frontend**: HTML5 + CSS3 + JavaScript
- **NFC**: Web NFC API (Chrome/Edge en Android)
- **Mapas**: Leaflet.js + OpenStreetMap
- **APIs**: Google Geolocation + HERE Positioning v2

## Setup

### 1. Instalación local

```bash
cd server
npm install
```

### 2. Configurar variables de entorno

```bash
cp .env.example .env
```

Edita `.env` y rellena:

```env
PORT=3000
GOOGLE_GEOLOCATION_KEY=tu_clave_de_google
HERE_POSITIONING_KEY=tu_clave_de_here
NODE_ENV=development
```

⚠️ **IMPORTANTE**: Nunca commits `.env` con claves reales. Las claves mostradas en el .env.example están expuestas en chat y deben regenerarse antes de producción.

### 3. Ejecutar servidor

**Desarrollo**:
```bash
npm run dev  # Con hot reload (requiere nodemon)
```

**Producción**:
```bash
npm start
```

Accede a `http://localhost:3000`

## API Endpoints

### GET `/` 
Interfaz web para leer NFC y ver datos

### POST `/api/celldata`
Subir datos de celda desde lectura NFC

**Request**:
```json
{
  "sensor_id": "SENSOR_001",
  "records": [
    {
      "timestamp": 1716990000,
      "temperature": 22.5,
      "mcc": 214,
      "mnc": 3,
      "tac": 0x8CA,
      "cell_id": 0x6936965,
      "rsrp": -98
    }
  ]
}
```

**Response**:
```json
{
  "status": "received",
  "sensor_id": "SENSOR_001",
  "record_count": 1,
  "timestamp": "2026-05-17T23:30:00Z"
}
```

### GET `/api/celldata/:sensor_id`
Obtener datos guardados de un sensor

**Response**:
```json
{
  "sensor_id": "SENSOR_001",
  "uploads": 3,
  "latest_upload": {...},
  "all_data": [...]
}
```

### POST `/api/geolocation`
Consultar geolocalización (Google → HERE fallback)

**Request**:
```json
{
  "mcc": 214,
  "mnc": 3,
  "tac": 0x8CA,
  "cid": 0x6936965
}
```

**Response**:
```json
{
  "lat": 41.2341,
  "lng": 1.2341,
  "accuracy": 337,
  "provider": "google",
  "timestamp": "2026-05-17T23:30:00Z"
}
```

### POST `/api/geolocation/google`
Consultar solo Google Geolocation API

### POST `/api/geolocation/here`
Consultar solo HERE Positioning API

### GET `/health`
Health check

## Deploy en Bluehost

### Opción 1: Node.js con Bluehost (Recommended)

Bluehost soporta Node.js en hosting compartido:

```bash
# 1. SSH a tu servidor
ssh usuario@tudominio.com

# 2. Navega a public_html
cd public_html

# 3. Clone del repo
git clone https://github.com/apamies/coldproff.git
cd coldproff/server

# 4. Instalar dependencias
npm install --production

# 5. Crear .env con claves
nano .env

# 6. Usar PM2 para mantener servidor activo
npm install -g pm2
pm2 start server.js --name "coldproff-server"
pm2 startup
pm2 save

# 7. Configurar proxy en Bluehost (cPanel)
# Addon Domains → apuntando a http://localhost:3000
```

### Opción 2: Como CGI (alternativa si Node no está disponible)

Usa PHP como proxy a Node.js:

```php
<?php
// public_html/api/proxy.php
$ch = curl_init('http://localhost:3000' . $_SERVER['REQUEST_URI']);
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
curl_setopt($ch, CURLOPT_HEADER, true);
curl_setopt($ch, CURLOPT_CUSTOMREQUEST, $_SERVER['REQUEST_METHOD']);
curl_setopt($ch, CURLOPT_POSTFIELDS, file_get_contents('php://input'));
$response = curl_exec($ch);
echo $response;
?>
```

### Configuración de variables de entorno en Bluehost

Opción A: Archivo `.env` en el servidor

```bash
# En el VPS/servidor
echo "GOOGLE_GEOLOCATION_KEY=..." > /home/usuario/coldproff/.env
chmod 600 /home/usuario/coldproff/.env
```

Opción B: Variables de entorno del sistema

```bash
export GOOGLE_GEOLOCATION_KEY=...
```

## Estructura de archivos

```
server/
├── api/
│   └── geolocation.js          # Lógica de geolocalización
├── routes/
│   ├── celldata.js             # Endpoints /api/celldata
│   └── geolocation.js          # Endpoints /api/geolocation
├── public/
│   ├── index.html              # Web app principal
│   ├── nfc-reader.js           # Cliente NFC (Web NFC API)
│   └── style.css               # Estilos
├── config/
│   └── (futuro: DB config)
├── server.js                   # Entry point Express
├── package.json
├── .env.example
└── README.md
```

## Testing local

### Mockeando lectura NFC

Abre la consola del navegador (F12) y ejecuta:

```javascript
// Simular lectura NFC
const mockMessage = {
  records: [{
    recordType: 'text',
    data: new TextEncoder().encode(JSON.stringify({
      sensor_id: 'TEST_001',
      records: [
        {
          timestamp: Math.floor(Date.now() / 1000),
          temperature: 22.5,
          mcc: 214,
          mnc: 3,
          tac: 0x8CA,
          cell_id: 0x6936965,
          rsrp: -98
        }
      ]
    }))
  }]
};

// Procesar como si fuera NFC real
const nfc = new NFCReader();
nfc.processNDEFMessage(mockMessage);
```

## Logs y debugging

Visualiza los logs del servidor:

```bash
npm run dev  # Mostrará logs en consola

# O con PM2
pm2 logs coldproff-server
```

Ejemplos de logs:

```
[celldata] Received 5 records from sensor SENSOR_001
[geolocation] Querying Google API for google_214_3_6936965
[geolocation] Cache hit: google_214_3_6936965
```

## API Key Management

### Google Geolocation

1. Ir a [Google Cloud Console](https://console.cloud.google.com/)
2. Crear proyecto "ColdProff"
3. Habilitar "Geolocation API"
4. Crear clave de API (restricción: Solo Geolocation API)
5. Copiar a `.env`

⚠️ **Costo**: ~$4.5 per 1000 requests (muy barato para uso de sensores)

### HERE Positioning

1. Ir a [HERE Developer Platform](https://developer.here.com/)
2. Crear cuenta / proyecto
3. Generar API Key
4. Copiar a `.env`

⚠️ **Costo**: FREE tier 250K requests/mes (muy bueno)

## TODO

- [ ] Integración con base de datos (MongoDB o PostgreSQL)
- [ ] Autenticación de usuarios / sensores
- [ ] Dashboard de analytics
- [ ] Exportar datos (CSV, JSON)
- [ ] Histórico de ubicaciones por sensor
- [ ] Notificaciones (email/webhook si temp fuera de rango)
- [ ] Integración con IFTTT
- [ ] Mobile app nativa (React Native / Flutter)

## Troubleshooting

### "Web NFC API not available"
- Solo funciona en Chrome/Edge en Android
- Requiere HTTPS (excepción: localhost para desarrollo)
- Verifica permisos de NFC en Android Settings

### "Geolocation not found"
- Cell ID podría no estar en base de datos (áreas rurales)
- Verifica parseo hex: `cid = parseInt(cid_hex, 16)`
- Prueba con Google + HERE fallback

### CORS errors
- Servidor ya incluye `cors` middleware
- Verifica que el cliente esté llamando a `/api/...` (misma origin)

---

**Última actualización**: 2026-05-17  
**Status**: Web app + API lista, deploy en Bluehost ready
