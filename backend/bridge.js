// bridge.js
// Listens to MQTT broker and writes every sensor reading to Firestore.
// Run this alongside simulate.js (in a separate terminal).

const mqtt  = require('mqtt');
const admin = require('firebase-admin');
const serviceAccount = require('./serviceAccountKey.json');
const { checkAlerts } = require('./alerts.js');

// Initialise Firebase
admin.initializeApp({
    credential: admin.credential.cert(serviceAccount)
});
const db = admin.firestore();
console.log('Firebase connected');

// Connect to MQTT
const client = mqtt.connect('mqtt://broker.hivemq.com');

client.on('connect', () => {
    console.log('Bridge connected to MQTT broker');
    client.subscribe('devices/node_01/readings');
    client.subscribe('devices/node_01/status');
    console.log('Listening for sensor data...\n');
});

client.on('message', async (topic, message) => {
    try {
        const payload = JSON.parse(message.toString());

        if (topic.includes('readings')) {

            // 1. Write to readings subcollection (for history graphs)
            await db.collection('devices')
                .doc('node_01')
                .collection('readings')
                .add({
                    ...payload,
                    timestamp: admin.firestore.FieldValue.serverTimestamp()
                });

            // 2. Update latest reading (for live dashboard)
            await db.collection('devices')
                .doc('node_01')
                .set({
                    latest:   payload,
                    lastSeen: admin.firestore.FieldValue.serverTimestamp(),
                    status:   'online'
                }, { merge: true });

            // 3. Check thresholds and save alerts
            await checkAlerts('node_01', payload);      

            console.log(`[${new Date().toLocaleTimeString()}] Written to Firestore — level: ${payload.level_pct}%  pH: ${payload.ph}`);
        }

        if (topic.includes('status')) {
            await db.collection('devices')
                .doc('node_01')
                .set({
                    status:   payload.status,
                    lastSeen: admin.firestore.FieldValue.serverTimestamp()
                }, { merge: true });
            console.log(`[${new Date().toLocaleTimeString()}] Device status: ${payload.status}`);
        }

    } catch (err) {
        console.error('Error writing to Firestore:', err.message);
    }
});

client.on('error', (err) => {
    console.error('MQTT error:', err.message);
});