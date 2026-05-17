/*
 * Geolocation routes - query Google/HERE APIs
 */

const express = require('express');
const axios = require('axios');
const router = express.Router();

const GOOGLE_GEOLOCATION_API = 'https://www.googleapis.com/geolocation/v1/geolocate';
const HERE_POSITIONING_API = 'https://positioning.api.here.com/v2/geolocate';

// Geolocation cache (to avoid repeated queries)
const geolocationCache = {};

// Helper: Build Google Geolocation request
function buildGoogleRequest(mcc, mnc, tac, cid) {
    return {
        radioType: 'lte',
        cellTowers: [{
            mobileCountryCode: mcc,
            mobileNetworkCode: mnc,
            locationAreaCode: tac,
            cellId: cid
        }]
    };
}

// Helper: Build HERE Positioning request
function buildHereRequest(mcc, mnc, cid) {
    return {
        lte: [{
            mcc,
            mnc,
            cid
        }]
    };
}

// POST /api/geolocation/google - Query Google Geolocation API
router.post('/google', async (req, res) => {
    try {
        const { mcc, mnc, tac, cid } = req.body;

        if (!mcc || !mnc || !tac || !cid) {
            return res.status(400).json({
                error: 'Missing required fields: mcc, mnc, tac, cid'
            });
        }

        // Check cache
        const cacheKey = `google_${mcc}_${mnc}_${cid}`;
        if (geolocationCache[cacheKey]) {
            console.log(`[geolocation] Cache hit: ${cacheKey}`);
            return res.json(geolocationCache[cacheKey]);
        }

        const apiKey = process.env.GOOGLE_GEOLOCATION_KEY;
        if (!apiKey) {
            return res.status(500).json({ error: 'Google API key not configured' });
        }

        const payload = buildGoogleRequest(mcc, mnc, tac, cid);

        console.log(`[geolocation] Querying Google API for ${cacheKey}`);

        const response = await axios.post(
            `${GOOGLE_GEOLOCATION_API}?key=${apiKey}`,
            payload,
            { timeout: 5000 }
        );

        const result = {
            provider: 'google',
            lat: response.data.location.lat,
            lng: response.data.location.lng,
            accuracy: response.data.accuracy,
            timestamp: new Date().toISOString()
        };

        // Cache result
        geolocationCache[cacheKey] = result;

        res.json(result);

    } catch (error) {
        console.error('[geolocation] Google API error:', error.message);
        res.status(500).json({
            error: 'Google Geolocation API request failed',
            details: error.message
        });
    }
});

// POST /api/geolocation/here - Query HERE Positioning API
router.post('/here', async (req, res) => {
    try {
        const { mcc, mnc, cid } = req.body;

        if (!mcc || !mnc || !cid) {
            return res.status(400).json({
                error: 'Missing required fields: mcc, mnc, cid'
            });
        }

        // Check cache
        const cacheKey = `here_${mcc}_${mnc}_${cid}`;
        if (geolocationCache[cacheKey]) {
            console.log(`[geolocation] Cache hit: ${cacheKey}`);
            return res.json(geolocationCache[cacheKey]);
        }

        const apiKey = process.env.HERE_POSITIONING_KEY;
        if (!apiKey) {
            return res.status(500).json({ error: 'HERE API key not configured' });
        }

        const payload = buildHereRequest(mcc, mnc, cid);

        console.log(`[geolocation] Querying HERE API for ${cacheKey}`);

        const response = await axios.post(
            `${HERE_POSITIONING_API}?apiKey=${apiKey}`,
            payload,
            { timeout: 5000 }
        );

        const result = {
            provider: 'here',
            lat: response.data.location.lat,
            lng: response.data.location.lng,
            accuracy: response.data.accuracy,
            timestamp: new Date().toISOString()
        };

        // Cache result
        geolocationCache[cacheKey] = result;

        res.json(result);

    } catch (error) {
        console.error('[geolocation] HERE API error:', error.message);
        res.status(500).json({
            error: 'HERE Positioning API request failed',
            details: error.message
        });
    }
});

// POST /api/geolocation - Try both (Google first, fallback HERE)
router.post('/', async (req, res) => {
    try {
        const { mcc, mnc, tac, cid } = req.body;

        if (!mcc || !mnc || !cid) {
            return res.status(400).json({
                error: 'Missing required fields: mcc, mnc, cid'
            });
        }

        let result = null;
        let provider = null;

        // Try Google first
        try {
            const googlePayload = buildGoogleRequest(mcc, mnc, tac, cid);
            const googleKey = process.env.GOOGLE_GEOLOCATION_KEY;

            if (googleKey) {
                const response = await axios.post(
                    `${GOOGLE_GEOLOCATION_API}?key=${googleKey}`,
                    googlePayload,
                    { timeout: 5000 }
                );

                result = {
                    lat: response.data.location.lat,
                    lng: response.data.location.lng,
                    accuracy: response.data.accuracy
                };
                provider = 'google';
            }
        } catch (err) {
            console.log('[geolocation] Google failed, trying HERE...');
        }

        // Fallback to HERE
        if (!result) {
            try {
                const herePayload = buildHereRequest(mcc, mnc, cid);
                const hereKey = process.env.HERE_POSITIONING_KEY;

                if (hereKey) {
                    const response = await axios.post(
                        `${HERE_POSITIONING_API}?apiKey=${hereKey}`,
                        herePayload,
                        { timeout: 5000 }
                    );

                    result = {
                        lat: response.data.location.lat,
                        lng: response.data.location.lng,
                        accuracy: response.data.accuracy
                    };
                    provider = 'here';
                }
            } catch (err) {
                console.log('[geolocation] HERE also failed');
            }
        }

        if (!result) {
            return res.status(500).json({
                error: 'All geolocation providers failed'
            });
        }

        res.json({
            ...result,
            provider,
            timestamp: new Date().toISOString()
        });

    } catch (error) {
        console.error('[geolocation] Error:', error.message);
        res.status(500).json({ error: 'Geolocation request failed' });
    }
});

module.exports = router;
