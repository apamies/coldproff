/*
 * Cell data routes - handle NFC uploads
 */

const express = require('express');
const router = express.Router();

// In-memory storage (replace with database in production)
const sensorData = {};

// POST /api/celldata - Upload data from NFC read
router.post('/', (req, res) => {
    try {
        const { sensor_id, records } = req.body;

        if (!sensor_id || !records) {
            return res.status(400).json({ error: 'Missing sensor_id or records' });
        }

        if (!Array.isArray(records)) {
            return res.status(400).json({ error: 'records must be an array' });
        }

        // Validate record format
        const validRecords = records.filter(record => {
            return record.timestamp &&
                   record.temperature !== undefined &&
                   record.mcc && record.mnc &&
                   record.tac && record.cell_id;
        });

        if (validRecords.length === 0) {
            return res.status(400).json({ error: 'No valid records' });
        }

        // Store data (in production: save to database)
        if (!sensorData[sensor_id]) {
            sensorData[sensor_id] = [];
        }

        sensorData[sensor_id].push({
            upload_time: new Date().toISOString(),
            record_count: validRecords.length,
            records: validRecords
        });

        console.log(`[celldata] Received ${validRecords.length} records from sensor ${sensor_id}`);

        res.json({
            status: 'received',
            sensor_id,
            record_count: validRecords.length,
            timestamp: new Date().toISOString()
        });

    } catch (error) {
        console.error('Error processing cell data:', error);
        res.status(500).json({ error: 'Failed to process data' });
    }
});

// GET /api/celldata/:sensor_id - Fetch sensor data
router.get('/:sensor_id', (req, res) => {
    const { sensor_id } = req.params;

    if (!sensorData[sensor_id]) {
        return res.status(404).json({ error: 'Sensor not found' });
    }

    res.json({
        sensor_id,
        uploads: sensorData[sensor_id].length,
        latest_upload: sensorData[sensor_id][sensorData[sensor_id].length - 1],
        all_data: sensorData[sensor_id]
    });
});

module.exports = router;
