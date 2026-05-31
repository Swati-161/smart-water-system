// backend/server.js
// Main entry point for the backend.
// Runs bridge, alerts, and valve API all together.
// Start this instead of running bridge.js separately.

const express = require('express');
const mqtt    = require('mqtt');
const admin   = require('firebase-admin');
const cors    = require('cors');
const serviceAccount = require('./serviceAccountKey.json');
const { checkAlerts } = require('./alerts.js');
const valveModule     = require('./valve.js');

// ── Firebase init ────────────────────────────────────────────
admin.initializeApp({
    credential: admin.credential.cert(serviceAccount)
});
const db = admin.firestore();
console.log('[Firebase] Connected');

// ── Express API server ───────────────────────────────────────
const app = express();
app.use(cors());
app.use(express.json());
app.use('/valve', valveModule.router);

// Health check endpoint — app can ping this to verify backend is up
app.get('/health', (_, res) => res.json({ status: 'ok', time: new Date() }));

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
    console.log(`[API] Server running on port ${PORT}`);
});

// ── MQTT bridge ───────────────────────────────────────────────
const mqttClient = mqtt.connect('mqtt://broker.hivemq.com');

// Give valve module access to mqtt client
valveModule.init(mqttClient);

mqttClient.on('connect', () => {
    console.log('[MQTT] Connected to broker');
    mqttClient.subscribe('devices/node_01/readings');
    mqttClient.subscribe('devices/node_01/status');
});

mqttClient.on('message', async (topic, message) => {
    try {
        const payload = JSON.parse(message.toString());

        if (topic.includes('readings')) {
            // Write to Firestore
            await db.collection('devices').doc('node_01')
                .collection('readings').add({
                    ...payload,
                    timestamp: admin.firestore.FieldValue.serverTimestamp()
                });

            await db.collection('devices').doc('node_01').set({
                latest:   payload,
                lastSeen: admin.firestore.FieldValue.serverTimestamp(),
                status:   'online'
            }, { merge: true });

            // Check alert thresholds
            await checkAlerts('node_01', payload);

            console.log(`[Bridge] Written — level: ${payload.level_pct}%  pH: ${payload.ph}`);
        }

        if (topic.includes('status')) {
            await db.collection('devices').doc('node_01').set({
                status:   payload.status,
                lastSeen: admin.firestore.FieldValue.serverTimestamp()
            }, { merge: true });
        }

    } catch (err) {
        console.error('[Bridge] Error:', err.message);
    }
});

console.log('[Server] SmAart backend running');