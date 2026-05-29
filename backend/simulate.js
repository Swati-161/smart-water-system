// simulate.js
// Publishes fake sensor data to MQTT every 30 seconds.
// Exactly mimics what your ESP32 main.cpp would publish.
// Run this on your laptop instead of needing real hardware.

const mqtt = require('mqtt');

const client = mqtt.connect('mqtt://broker.hivemq.com');

let level     = 72.0;
let pH        = 7.1;
let turbidity = 0.8;
let litres    = 0.0;
let battery   = 3.85;

client.on('connect', () => {
    console.log('Simulator connected to MQTT broker');
    console.log('Publishing fake sensor data every 30 seconds...');
    console.log('Press Ctrl+C to stop\n');

    // Publish immediately on connect
    publish();

    // Then every 30 seconds
    setInterval(publish, 30000);
});

function publish() {
    // Simulate gradual tank drain
    level -= 0.4;
    if (level < 8.0) level = 95.0;

    // Simulate pH drift
    pH = 7.0 + (Math.random() * 0.4 - 0.2);

    // Simulate usage
    litres += 0.2;

    // Simulate battery drain
    battery -= 0.0005;
    if (battery < 3.2) battery = 4.1;

    const payload = {
        device_id:     'node_01',
        fw_version:    '1.0.0',
        mock:          true,
        level_pct:     parseFloat(level.toFixed(1)),
        ph:            parseFloat(pH.toFixed(2)),
        turbidity_ntu: parseFloat(turbidity.toFixed(1)),
        flow_lpm:      0.0,
        total_litres:  parseFloat(litres.toFixed(1)),
        leak_detected: false,
        battery_v:     parseFloat(battery.toFixed(2)),
        valve_open:    false
    };

    const topic = 'devices/node_01/readings';
    client.publish(topic, JSON.stringify(payload));
    console.log(`[${new Date().toLocaleTimeString()}] Published:`, payload);
}

client.on('error', (err) => {
    console.error('MQTT error:', err.message);
});