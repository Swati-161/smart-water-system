// backend/server.js
// ─────────────────────────────────────────────────────────────
// SmAart Water Management System — Backend Entry Point
// Runs: MQTT bridge + Firestore writer + alert engine +
//       offline detection + daily consumption + REST API
//       + Firestore valve command listener
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

    // ── Firestore valve command listener ─────────────────────
    // Started inside 'connect' so mqttClient is guaranteed ready
    // Watches for command docs written by the Flutter app
    // Forwards them to MQTT → ESP32 acts on them
    // The 'executed: false' filter ensures each command fires once only
    db.collection('devices').doc('node_01')
        .collection('commands')
        .where('executed', '==', false)
        .onSnapshot(async (snap) => {
            for (const change of snap.docChanges()) {
                // Only process newly added documents
                // 'modified' and 'removed' are ignored
                if (change.type !== 'added') continue;

                const cmd    = change.doc.data();
                const action = cmd.action; // 'OPEN' or 'CLOSE'

                if (!action || !['OPEN', 'CLOSE'].includes(action)) {
                    console.warn('[Valve] Unknown action in command doc:', action);
                    // Mark as executed anyway to prevent infinite retry
                    await change.doc.ref.update({ executed: true }).catch(() => {});
                    continue;
                }

                if (!mqttClient.connected) {
                    console.warn('[Valve] MQTT not connected — command will retry on next snapshot');
                    // Do NOT mark as executed — it will retry when next snapshot fires
                    continue;
                }

                const topic   = 'devices/node_01/commands';
                const payload = JSON.stringify({ command: action });

                mqttClient.publish(topic, payload, { qos: 1 }, async (err) => {
                    if (err) {
                        console.error(`[Valve] Publish error for ${action}:`, err.message);
                        // Do not mark executed — allow retry
                        return;
                    }

                    // Mark as executed so the listener does not fire again for this doc
                    await change.doc.ref.update({
                        executed:   true,
                        executedAt: admin.firestore.FieldValue.serverTimestamp()
                    }).catch((e) => {
                        console.error('[Valve] Could not mark command executed:', e.message);
                    });

                    console.log(`[Valve] ${action} forwarded to MQTT — command doc marked executed`);
                });
            }
        }, (err) => {
            // onSnapshot error handler — fires if Firestore connection drops
            console.error('[Valve] Command listener error:', err.message);
        });

    console.log('[Valve] Firestore command listener active');
});

mqttClient.on('message', async (topic, message) => {
    try {
        const payload = JSON.parse(message.toString());

        if (topic.includes('readings')) {
            // -1 means sensor error in firmware — still write but flag it
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
const OFFLINE_THRESHOLD_MS = 10 * 60 * 1000;

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
// At midnight: saves the day's total water usage
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
        scheduleMidnightReset();
    }, msUntilMidnight);
}

scheduleMidnightReset();

console.log('[Server] SmAart backend running — waiting for sensor data');