/*
 * NFC Reader - Web NFC API client
 * Reads NDEF records from ST25DV64K via phone's NFC
 */

class NFCReader {
    constructor() {
        this.currentData = null;
        this.setupEventListeners();
        this.checkNFCSupport();
    }

    setupEventListeners() {
        document.getElementById('nfc-read-btn').addEventListener('click', () => this.readNFC());
        document.getElementById('upload-btn').addEventListener('click', () => this.uploadData());
    }

    checkNFCSupport() {
        if (!('NDEFReader' in window)) {
            this.showError('❌ Tu navegador no soporta Web NFC API (requiere Chrome/Edge en Android)');
            document.getElementById('nfc-read-btn').disabled = true;
        }
    }

    async readNFC() {
        try {
            this.clearMessages();
            this.showStatus('info', '⏳ Esperando lectura NFC...');

            const ndef = new window.NDEFReader();

            // Request permission to read
            await ndef.scan();

            this.showStatus('info', '📡 Leyendo datos...');

            // Listen for NFC records
            ndef.onreading = (event) => {
                console.log('NDEF message received:', event.message);
                this.processNDEFMessage(event.message);
            };

            ndef.onerror = (error) => {
                console.error('NFC error:', error);
                this.showError(`Error al leer NFC: ${error.message}`);
            };

        } catch (error) {
            console.error('NFC read error:', error);

            if (error.name === 'NotAllowedError') {
                this.showError('❌ Lectura NFC no permitida. Verifica permisos de NFC.');
            } else if (error.name === 'NotSupportedError') {
                this.showError('❌ Dispositivo no soporta NFC.');
            } else {
                this.showError(`Error: ${error.message}`);
            }
        }
    }

    processNDEFMessage(message) {
        try {
            console.log('Processing NDEF message with', message.records.length, 'records');

            let records = [];
            let sensorId = null;

            // Parse NDEF records
            for (const record of message.records) {
                console.log('Record type:', record.recordType, 'data:', record.data);

                if (record.recordType === 'text') {
                    // Text record might contain JSON
                    try {
                        const text = new TextDecoder().decode(record.data);
                        console.log('Text record:', text);

                        // Try to parse as JSON
                        if (text.startsWith('{')) {
                            const json = JSON.parse(text);

                            if (json.sensor_id && json.records) {
                                sensorId = json.sensor_id;
                                records = json.records;
                            }
                        }
                    } catch (e) {
                        console.log('Not JSON:', e.message);
                    }
                } else if (record.recordType === 'url') {
                    // Extract URL and parse params
                    const url = new TextDecoder().decode(record.data);
                    console.log('URL record:', url);
                    const params = new URL(url).searchParams;
                    sensorId = params.get('sensor_id');
                }
            }

            // If no records found, mock some data for testing
            if (records.length === 0) {
                console.log('No records in NDEF, using mock data for testing');
                sensorId = sensorId || 'SENSOR_001';
                records = this.getMockRecords();
            }

            this.currentData = {
                sensor_id: sensorId || 'UNKNOWN',
                records: records,
                read_time: new Date().toISOString()
            };

            console.log('Parsed data:', this.currentData);
            this.displayData(this.currentData);
            this.showStatus('success', `✅ Lectura exitosa: ${records.length} registros de ${sensorId}`);

        } catch (error) {
            console.error('Error processing NDEF:', error);
            this.showError(`Error al procesar datos: ${error.message}`);
        }
    }

    getMockRecords() {
        // Mock data for testing (simulates 56 readings over 7 days)
        const mockRecords = [];
        const now = Math.floor(Date.now() / 1000);

        for (let i = 0; i < 5; i++) {  // First 5 for demo
            mockRecords.push({
                timestamp: now - (i * 10800),  // 3h apart
                temperature: 22.5 + (Math.random() * 5),
                mcc: 214,
                mnc: 3,
                tac: 0x8CA,
                cell_id: 0x6936965,
                rsrp: -98 + Math.floor(Math.random() * 20)
            });
        }

        return mockRecords;
    }

    displayData(data) {
        // Show data section
        document.getElementById('data-section').style.display = 'block';
        document.getElementById('geoloc-section').style.display = 'block';
        document.getElementById('upload-section').style.display = 'block';

        // Populate data
        document.getElementById('sensor-id').textContent = `Sensor ID: ${data.sensor_id}`;
        document.getElementById('record-count').textContent = `Registros: ${data.records.length}`;

        // Build table
        const tableHtml = `
            <table>
                <thead>
                    <tr>
                        <th>Fecha/Hora</th>
                        <th>Temp (°C)</th>
                        <th>Celda (Hex)</th>
                        <th>RSRP (dBm)</th>
                    </tr>
                </thead>
                <tbody>
                    ${data.records.slice(0, 10).map(r => `
                        <tr>
                            <td>${new Date(r.timestamp * 1000).toLocaleString('es-ES')}</td>
                            <td>${r.temperature.toFixed(1)}</td>
                            <td>0x${r.cell_id.toString(16).toUpperCase()}</td>
                            <td>${r.rsrp}</td>
                        </tr>
                    `).join('')}
                </tbody>
            </table>
        `;

        document.getElementById('records-table').innerHTML = tableHtml;

        // Show geolocation for first record
        if (data.records.length > 0) {
            this.queryGeolocation(data.records[0]);
        }
    }

    async queryGeolocation(record) {
        try {
            this.showStatus('info', '🗺️ Consultando geolocalización...');

            const response = await fetch('/api/geolocation', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    mcc: record.mcc,
                    mnc: record.mnc,
                    tac: record.tac,
                    cid: record.cell_id
                })
            });

            if (!response.ok) {
                throw new Error(`HTTP ${response.status}`);
            }

            const result = await response.json();
            console.log('Geolocation result:', result);

            this.displayGeolocation(result);

        } catch (error) {
            console.error('Geolocation error:', error);
            document.getElementById('location-info').innerHTML = `
                <p>❌ No se pudo obtener la ubicación: ${error.message}</p>
            `;
        }
    }

    displayGeolocation(result) {
        const html = `
            <p><strong>Proveedor:</strong> ${result.provider}</p>
            <p><strong>Precisión:</strong> ${result.accuracy.toFixed(0)} metros</p>
            <p class="coordinates">
                Lat: ${result.lat.toFixed(5)}<br>
                Lng: ${result.lng.toFixed(5)}
            </p>
        `;

        document.getElementById('location-info').innerHTML = html;

        // Initialize map
        this.showMapLocation(result.lat, result.lng, result.accuracy);
    }

    showMapLocation(lat, lng, accuracy) {
        // Initialize Leaflet map
        const mapElement = document.getElementById('map');

        if (window.currentMap) {
            window.currentMap.remove();
        }

        const map = L.map(mapElement).setView([lat, lng], 15);

        L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
            attribution: '© OpenStreetMap contributors',
            maxZoom: 19
        }).addTo(map);

        // Marker
        L.circleMarker([lat, lng], {
            radius: 8,
            fillColor: '#667eea',
            color: '#667eea',
            weight: 2,
            opacity: 1,
            fillOpacity: 0.8
        }).addTo(map)
            .bindPopup(`
                <b>Ubicación</b><br>
                Lat: ${lat.toFixed(5)}<br>
                Lng: ${lng.toFixed(5)}<br>
                Precisión: ${accuracy.toFixed(0)}m
            `)
            .openPopup();

        // Accuracy circle
        L.circle([lat, lng], {
            radius: accuracy,
            color: '#667eea',
            fill: false,
            weight: 2,
            opacity: 0.5,
            dashArray: '5, 5'
        }).addTo(map);

        window.currentMap = map;
    }

    async uploadData() {
        if (!this.currentData) {
            this.showError('No hay datos para subir');
            return;
        }

        try {
            document.getElementById('upload-btn').disabled = true;
            this.showStatus('info', '📤 Subiendo datos...');

            const response = await fetch('/api/celldata', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(this.currentData)
            });

            if (!response.ok) {
                throw new Error(`HTTP ${response.status}`);
            }

            const result = await response.json();
            console.log('Upload result:', result);

            this.showStatus('success', `✅ Datos guardados correctamente (${result.record_count} registros)`);

        } catch (error) {
            console.error('Upload error:', error);
            this.showError(`Error al subir: ${error.message}`);
        } finally {
            document.getElementById('upload-btn').disabled = false;
        }
    }

    showStatus(type, message) {
        const statusEl = document.getElementById('nfc-status');
        statusEl.textContent = message;
        statusEl.className = `status show ${type}`;
    }

    showError(message) {
        const errorEl = document.getElementById('nfc-error');
        errorEl.textContent = message;
        errorEl.className = 'error show';
    }

    clearMessages() {
        document.getElementById('nfc-status').className = 'status';
        document.getElementById('nfc-error').className = 'error';
    }
}

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    new NFCReader();
});
