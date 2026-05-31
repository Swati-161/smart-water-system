// backend/valve.js
// Simple Express REST API.
// App calls POST /valve/command → this publishes to MQTT → ESP32 receives it.

const express = require('express');
const router  = express.Router();

// mqttClient passed in from server.js
let mqttClient;

function init(client) {
    mqttClient = client;
}

router.post('/command', async (req, res) => {
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
            return res.status(500).json({ error: 'Failed to publish command' });
        }
        console.log(`[Valve] Command sent: ${action} to ${deviceId}`);
        res.json({ success: true, action: action.toUpperCase(), deviceId });
    });
});

module.exports = { router, init };