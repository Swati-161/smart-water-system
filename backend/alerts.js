// backend/alerts.js
// Runs alongside bridge.js.
// Every time bridge.js writes a new reading to Firestore,
// this checks all thresholds and sends FCM push notifications.
// For now it logs alerts to console — FCM integration added after
// app push notification setup is done.

const admin = require('firebase-admin');

// Thresholds — must match config.h in firmware exactly
const THRESHOLDS = {
    levelHigh:       95.0,
    levelWarnLow:    20.0,
    levelCritLow:    10.0,
    phWarnLow:       6.8,
    phWarnHigh:      8.2,
    phCritLow:       6.5,
    phCritHigh:      8.5,
    turbidityWarn:   1.0,
    turbidityCrit:   5.0,
    flowLeakLpm:     0.5,
};

// Alert deduplication — don't spam same alert repeatedly
// Stores last alert time per type
const lastAlertTime = {};
const ALERT_COOLDOWN_MS = 30 * 60 * 1000; // 30 minutes

function shouldAlert(type) {
    const now = Date.now();
    if (!lastAlertTime[type] || 
        (now - lastAlertTime[type]) > ALERT_COOLDOWN_MS) {
        lastAlertTime[type] = now;
        return true;
    }
    return false;
}

async function saveAlert(deviceId, type, parameter, value, threshold, message) {
    const db = admin.firestore();
    
    await db.collection('devices')
        .doc(deviceId)
        .collection('alerts')
        .add({
            type,           // 'critical' or 'warning'
            parameter,      // 'level', 'pH', 'turbidity', 'leak', 'offline'
            value,          // actual sensor value
            threshold,      // threshold that was breached
            message,        // human readable message
            timestamp: admin.firestore.FieldValue.serverTimestamp(),
            resolved: false
        });

    console.log(`[ALERT] ${type.toUpperCase()} — ${message}`);
}

async function checkAlerts(deviceId, data) {
    const tasks = [];

    // ── Water level ─────────────────────────────────────────
    if (data.level_pct >= THRESHOLDS.levelHigh) {
        if (shouldAlert('level_high')) {
            tasks.push(saveAlert(deviceId, 'critical', 'level',
                data.level_pct, THRESHOLDS.levelHigh,
                `Tank full (${data.level_pct}%) — supply valve closed automatically`));
        }
    } else if (data.level_pct <= THRESHOLDS.levelCritLow) {
        if (shouldAlert('level_crit_low')) {
            tasks.push(saveAlert(deviceId, 'critical', 'level',
                data.level_pct, THRESHOLDS.levelCritLow,
                `Tank critically low (${data.level_pct}%) — refill immediately`));
        }
    } else if (data.level_pct <= THRESHOLDS.levelWarnLow) {
        if (shouldAlert('level_warn_low')) {
            tasks.push(saveAlert(deviceId, 'warning', 'level',
                data.level_pct, THRESHOLDS.levelWarnLow,
                `Tank level low (${data.level_pct}%) — refill soon`));
        }
    }

    // ── pH ───────────────────────────────────────────────────
    if (data.ph < THRESHOLDS.phCritLow || data.ph > THRESHOLDS.phCritHigh) {
        if (shouldAlert('ph_critical')) {
            tasks.push(saveAlert(deviceId, 'critical', 'pH',
                data.ph, `${THRESHOLDS.phCritLow}–${THRESHOLDS.phCritHigh}`,
                `pH ${data.ph} is outside safe drinking range (6.5–8.5 BIS standard)`));
        }
    } else if (data.ph < THRESHOLDS.phWarnLow || data.ph > THRESHOLDS.phWarnHigh) {
        if (shouldAlert('ph_warning')) {
            tasks.push(saveAlert(deviceId, 'warning', 'pH',
                data.ph, `${THRESHOLDS.phWarnLow}–${THRESHOLDS.phWarnHigh}`,
                `pH ${data.ph} approaching unsafe range — monitor closely`));
        }
    }

    // ── Turbidity ────────────────────────────────────────────
    if (data.turbidity_ntu > THRESHOLDS.turbidityCrit) {
        if (shouldAlert('turbidity_critical')) {
            tasks.push(saveAlert(deviceId, 'critical', 'turbidity',
                data.turbidity_ntu, THRESHOLDS.turbidityCrit,
                `Turbidity ${data.turbidity_ntu} NTU exceeds BIS limit of 5 NTU — water unsafe`));
        }
    } else if (data.turbidity_ntu > THRESHOLDS.turbidityWarn) {
        if (shouldAlert('turbidity_warning')) {
            tasks.push(saveAlert(deviceId, 'warning', 'turbidity',
                data.turbidity_ntu, THRESHOLDS.turbidityWarn,
                `Turbidity ${data.turbidity_ntu} NTU — water becoming cloudy`));
        }
    }

    // ── Leak ─────────────────────────────────────────────────
    if (data.leak_detected) {
        if (shouldAlert('leak')) {
            tasks.push(saveAlert(deviceId, 'critical', 'leak',
                1, 0,
                'Water leak detected at moisture sensor — check pipes immediately'));
        }
    }

    await Promise.all(tasks);
}

module.exports = { checkAlerts };