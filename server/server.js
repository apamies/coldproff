/*
 * ColdProff Server - NFC + Geolocation API
 * Node.js + Express
 *
 * Endpoints:
 * GET  /                          - NFC reader web app
 * POST /api/celldata              - Upload cell data from NFC
 * POST /api/geolocation           - Get geolocation for cell ID
 * GET  /api/sensors/:sensor_id    - Fetch sensor data history
 */

const express = require('express');
const cors = require('cors');
const bodyParser = require('body-parser');
require('dotenv').config();

const app = express();
const PORT = process.env.PORT || 3000;

// Middleware
app.use(cors());
app.use(bodyParser.json({ limit: '10mb' }));
app.use(bodyParser.urlencoded({ limit: '10mb', extended: true }));
app.use(express.static('public'));

// Routes
const cellDataRoutes = require('./routes/celldata');
const geolocationRoutes = require('./routes/geolocation');

// API routes
app.use('/api/celldata', cellDataRoutes);
app.use('/api/geolocation', geolocationRoutes);

// Serve index.html for root
app.get('/', (req, res) => {
    res.sendFile(__dirname + '/public/index.html');
});

// Health check
app.get('/health', (req, res) => {
    res.json({ status: 'ok', timestamp: new Date().toISOString() });
});

// 404 handler
app.use((req, res) => {
    res.status(404).json({ error: 'Endpoint not found' });
});

// Error handler
app.use((err, req, res, next) => {
    console.error('Error:', err);
    res.status(500).json({ error: 'Internal server error' });
});

// Start server
app.listen(PORT, () => {
    console.log(`\n=== ColdProff Server ===`);
    console.log(`Listening on port ${PORT}`);
    console.log(`Open http://localhost:${PORT}/`);
    console.log(`NFC reader ready\n`);
});
