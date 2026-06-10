// backend/valve.js
// REST API for valve control.
// App calls POST /valve/command → publishes MQTT → ESP32 acts.

const express = require('express');
const router  = express.Router();

let mqttClient;

function init(client) {
    mqttClient = client;
}

router.post('/command', async (req, res) => {
    // Guard: MQTT must be connected before sending commands
    if (!mqttClient || !mqttClient.connected) {
        return res.status(503).json({
            error: 'MQTT not connected — cannot send valve command. Retry in a few seconds.'
        });
    }

    const { action, deviceId = 'node_01' } = req.body;

    if (!action || !['OPEN', 'CLOSE'].includes(action.toUpperCase())) {
        return res.status(400).json({
            error: 'Invalid action. Must be OPEN or CLOSE'
        });
    }

    const topic   = `devices/${deviceId}/commands`;
    const payload = JSON.stringify({ command: action.toUpperCase() });

    mqttClient.publish(topic, payload, { qos: 1 }, (err) => {
        if (err) {
            console.error('[Valve] Publish error:', err.message);
            return res.status(500).json({ error: 'Failed to publish command' });
        }
        console.log(`[Valve] ${action.toUpperCase()} sent to ${deviceId}`);
        res.json({ success: true, action: action.toUpperCase(), deviceId });
    });
});

module.exports = { router, init };