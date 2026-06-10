// backend/server.js
// ─────────────────────────────────────────────────────────────
// SmAart Water Management System — Backend Entry Point
// Runs: MQTT bridge + Firestore writer + alert engine +
//       offline detection + daily consumption + REST API
//
// Start with: node server.js  (or npm start)
// Simulator:  node simulate.js  (separate terminal, for testing)
// ─────────────────────────────────────────────────────────────

const express        = require('express');
const mqtt           = require('mqtt');
const admin          = require('firebase-admin');
const cors           = require('cors');
const serviceAccount = require('./serviceAccountKey.json');
const { checkAlerts } = require('./alerts.js');
const valveModule     = require('./valve.js');
const routesModule    = require('./routes.js');

// ── Firebase init ─────────────────────────────────────────────
admin.initializeApp({
    credential: admin.credential.cert(serviceAccount)
});
const db = admin.firestore();
console.log('[Firebase] Connected');

// ── Express API server ────────────────────────────────────────
const app = express();
app.use(cors());
app.use(express.json());

// Initialise route modules with db reference
valveModule.init(null); // mqttClient injected after MQTT connects
routesModule.init(db);

// Mount routes
app.use('/valve',    valveModule.router);
app.use('/readings', routesModule.router);
app.use('/alerts',   routesModule.router);
app.use('/stats',    routesModule.router);
app.use('/device',   routesModule.router);
app.use('/config',   routesModule.router);

// Health check
app.get('/health', (_, res) => res.json({
    status: 'ok',
    time: new Date(),
    uptime: Math.floor(process.uptime()) + 's'
}));

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
    console.log(`[API] Server running on port ${PORT}`);
    console.log(`[API] Endpoints: /health  /readings/latest  /readings/history`);
    console.log(`[API]            /alerts/recent  /stats/daily  /device/status`);
    console.log(`[API]            /valve/command  /config/thresholds`);
});

// ── MQTT bridge ───────────────────────────────────────────────
const mqttClient = mqtt.connect('mqtt://broker.hivemq.com');

// Inject mqttClient into valve module once connected
mqttClient.on('connect', () => {
    console.log('[MQTT] Connected to broker');
    valveModule.init(mqttClient); // now safe to use

    mqttClient.subscribe('devices/node_01/readings', (err) => {
        if (err) console.error('[MQTT] Subscribe error (readings):', err.message);
        else console.log('[MQTT] Subscribed to devices/node_01/readings');
    });
    mqttClient.subscribe('devices/node_01/status', (err) => {
        if (err) console.error('[MQTT] Subscribe error (status):', err.message);
        else console.log('[MQTT] Subscribed to devices/node_01/status');
    });
});

mqttClient.on('message', async (topic, message) => {
    try {
        const payload = JSON.parse(message.toString());

        if (topic.includes('readings')) {
            // Skip invalid readings — -1 means sensor error in firmware
            // Still write to Firestore but flag them
            const hasErrors = payload.level_pct === -1
                           || payload.ph === -1
                           || payload.turbidity_ntu === -1;

            // Write to readings subcollection (history graphs)
            await db.collection('devices').doc('node_01')
                .collection('readings').add({
                    ...payload,
                    has_errors: hasErrors,
                    timestamp: admin.firestore.FieldValue.serverTimestamp()
                });

            // Update latest reading (live dashboard)
            await db.collection('devices').doc('node_01').set({
                latest:   payload,
                lastSeen: admin.firestore.FieldValue.serverTimestamp(),
                status:   'online'
            }, { merge: true });

            // Check alert thresholds
            await checkAlerts(db, 'node_01', payload);

            console.log(`[Bridge] level: ${payload.level_pct}%`
                + `  pH: ${payload.ph}`
                + `  turbidity: ${payload.turbidity_ntu} NTU`
                + `  leak: ${payload.leak_detected}`
                + `  mock: ${payload.mock}`);
        }

        if (topic.includes('status')) {
            await db.collection('devices').doc('node_01').set({
                status:   payload.status,
                lastSeen: admin.firestore.FieldValue.serverTimestamp()
            }, { merge: true });
            console.log(`[Status] Device node_01: ${payload.status}`);
        }

    } catch (err) {
        console.error('[Bridge] Error:', err.message);
    }
});

mqttClient.on('error', (err) => {
    console.error('[MQTT] Error:', err.message);
});

mqttClient.on('offline', () => {
    console.warn('[MQTT] Client offline — will reconnect automatically');
});

// ── Offline detection ─────────────────────────────────────────
// Checks every 2 minutes if device has been silent for 10+ minutes
// If so, marks it offline in Firestore and saves an alert
const OFFLINE_THRESHOLD_MS = 10 * 60 * 1000; // 10 minutes

setInterval(async () => {
    try {
        const doc = await db.collection('devices').doc('node_01').get();
        if (!doc.exists) return;

        const data     = doc.data();
        const lastSeen = data?.lastSeen?.toDate();
        if (!lastSeen) return;

        const offlineMs = Date.now() - lastSeen.getTime();

        if (offlineMs > OFFLINE_THRESHOLD_MS && data?.status !== 'offline') {
            await db.collection('devices').doc('node_01')
                .set({ status: 'offline' }, { merge: true });

            await checkAlerts(db, 'node_01', {
                _offline: true,
                offlineMinutes: Math.floor(offlineMs / 60000)
            });

            console.log(`[Offline] node_01 marked offline — silent for ${Math.floor(offlineMs / 60000)} min`);
        }
    } catch (err) {
        console.error('[Offline] Check error:', err.message);
    }
}, 2 * 60 * 1000);

// ── Daily consumption record ──────────────────────────────────
// At midnight: saves the day's total water usage, resets counter
function scheduleMidnightReset() {
    const now      = new Date();
    const midnight = new Date();
    midnight.setHours(24, 0, 0, 0);
    const msUntilMidnight = midnight - now;

    console.log(`[Daily] Next consumption record in ${Math.floor(msUntilMidnight / 3600000)}h`);

    setTimeout(async () => {
        try {
            const doc = await db.collection('devices').doc('node_01').get();
            const totalLitres = doc.data()?.latest?.total_litres ?? 0;
            const today = new Date().toISOString().split('T')[0];

            await db.collection('devices').doc('node_01')
                .collection('daily_consumption')
                .doc(today)
                .set({
                    litres:    totalLitres,
                    date:      today,
                    timestamp: admin.firestore.FieldValue.serverTimestamp()
                });

            console.log(`[Daily] Saved ${today}: ${totalLitres}L`);
        } catch (err) {
            console.error('[Daily] Error:', err.message);
        }
        scheduleMidnightReset(); // schedule next midnight
    }, msUntilMidnight);
}

scheduleMidnightReset();

console.log('[Server] SmAart backend running — waiting for sensor data');