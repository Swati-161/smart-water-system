// backend/routes.js
// REST API endpoints for the Flutter app.
// All data reads from Firestore — no MQTT needed here.

const express = require('express');
const router  = express.Router();
let db;

function init(firestore) {
    db = firestore;
}

// GET /readings/latest
// Live dashboard — returns most recent sensor values
router.get('/readings/latest', async (req, res) => {
    try {
        const doc = await db.collection('devices').doc('node_01').get();
        if (!doc.exists) return res.status(404).json({ error: 'Device not found' });
        res.json({
            data:     doc.data()?.latest,
            lastSeen: doc.data()?.lastSeen,
            status:   doc.data()?.status
        });
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// GET /readings/history?days=7
// History graphs — returns time-series readings for last N days
router.get('/readings/history', async (req, res) => {
    try {
        const days  = Math.min(parseInt(req.query.days) || 7, 30);
        const since = new Date();
        since.setDate(since.getDate() - days);

        const snap = await db.collection('devices').doc('node_01')
            .collection('readings')
            .where('timestamp', '>', since)
            .orderBy('timestamp', 'asc')
            .limit(500)
            .get();

        const readings = snap.docs.map(d => ({ id: d.id, ...d.data() }));
        res.json({ readings, count: readings.length, days });
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// GET /alerts/recent
// Alert history screen
router.get('/alerts/recent', async (req, res) => {
    try {
        const snap = await db.collection('devices').doc('node_01')
            .collection('alerts')
            .orderBy('timestamp', 'desc')
            .limit(50)
            .get();

        const alerts = snap.docs.map(d => ({ id: d.id, ...d.data() }));
        res.json({ alerts, count: alerts.length });
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// PATCH /alerts/:id/resolve
// Mark an alert as resolved from the app
router.patch('/alerts/:id/resolve', async (req, res) => {
    try {
        await db.collection('devices').doc('node_01')
            .collection('alerts').doc(req.params.id)
            .update({ resolved: true, resolvedAt: new Date() });
        res.json({ success: true });
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// GET /stats/daily
// Daily consumption bar chart — last 7 days
router.get('/stats/daily', async (req, res) => {
    try {
        const snap = await db.collection('devices').doc('node_01')
            .collection('daily_consumption')
            .orderBy('date', 'desc')
            .limit(7)
            .get();

        const days = snap.docs.map(d => d.data());
        res.json({ days });
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// GET /device/status
// Device online/offline badge in app
router.get('/device/status', async (req, res) => {
    try {
        const doc = await db.collection('devices').doc('node_01').get();
        if (!doc.exists) return res.status(404).json({ error: 'Device not found' });
        res.json({
            status:   doc.data()?.status,
            lastSeen: doc.data()?.lastSeen
        });
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// PUT /config/thresholds
// Update alert thresholds from app settings screen
router.put('/config/thresholds', async (req, res) => {
    try {
        const allowed = [
            'levelHigh', 'levelWarnLow', 'levelCritLow',
            'phWarnLow',  'phWarnHigh',  'phCritLow', 'phCritHigh',
            'turbidityWarn', 'turbidityCrit'
        ];
        const thresholds = {};
        for (const key of allowed) {
            if (req.body[key] !== undefined) {
                thresholds[key] = parseFloat(req.body[key]);
            }
        }
        if (Object.keys(thresholds).length === 0) {
            return res.status(400).json({ error: 'No valid threshold keys provided' });
        }
        await db.collection('devices').doc('node_01')
            .set({ thresholds }, { merge: true });
        res.json({ success: true, thresholds });
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

module.exports = { router, init };