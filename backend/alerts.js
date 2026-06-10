// backend/alerts.js
// Threshold checking and alert saving.
// Called by server.js after every sensor reading.
// db (Firestore) is passed in as a parameter — no direct Firebase init here.

const THRESHOLDS = {
    levelHigh:      95.0,
    levelWarnLow:   20.0,
    levelCritLow:   10.0,
    phWarnLow:      6.8,
    phWarnHigh:     8.2,
    phCritLow:      6.5,
    phCritHigh:     8.5,
    turbidityWarn:  1.0,
    turbidityCrit:  5.0,
};

// Deduplication — prevent same alert firing more than once per 30 minutes
const lastAlertTime = {};
const ALERT_COOLDOWN_MS = 30 * 60 * 1000;

function shouldAlert(type) {
    const now = Date.now();
    if (!lastAlertTime[type] ||
        (now - lastAlertTime[type]) > ALERT_COOLDOWN_MS) {
        lastAlertTime[type] = now;
        return true;
    }
    return false;
}

async function saveAlert(db, deviceId, type, parameter, value, threshold, message) {
    await db.collection('devices')
        .doc(deviceId)
        .collection('alerts')
        .add({
            type,        // 'critical' or 'warning'
            parameter,   // 'level', 'pH', 'turbidity', 'leak', 'offline'
            value,       // actual sensor value at time of alert
            threshold,   // threshold breached
            message,     // human-readable description
            timestamp:   require('firebase-admin').firestore.FieldValue.serverTimestamp(),
            resolved:    false
        });

    console.log(`[ALERT] ${type.toUpperCase()} [${parameter}] — ${message}`);
}

async function checkAlerts(db, deviceId, data) {
    const tasks = [];

    // ── Offline alert ─────────────────────────────────────────
    if (data._offline) {
        if (shouldAlert('offline')) {
            tasks.push(saveAlert(db, deviceId, 'critical', 'offline',
                data.offlineMinutes, 10,
                `Sensor node offline — no data for ${data.offlineMinutes} minutes`));
        }
        await Promise.all(tasks);
        return; // no point checking sensors if device is offline
    }

    // ── Water level ───────────────────────────────────────────
    if (data.level_pct !== undefined && data.level_pct >= 0) {
        if (data.level_pct >= THRESHOLDS.levelHigh) {
            if (shouldAlert('level_high')) {
                tasks.push(saveAlert(db, deviceId, 'critical', 'level',
                    data.level_pct, THRESHOLDS.levelHigh,
                    `Tank full (${data.level_pct}%) — supply valve closed automatically`));
            }
        } else if (data.level_pct <= THRESHOLDS.levelCritLow) {
            if (shouldAlert('level_crit_low')) {
                tasks.push(saveAlert(db, deviceId, 'critical', 'level',
                    data.level_pct, THRESHOLDS.levelCritLow,
                    `Tank critically low (${data.level_pct}%) — refill immediately`));
            }
        } else if (data.level_pct <= THRESHOLDS.levelWarnLow) {
            if (shouldAlert('level_warn_low')) {
                tasks.push(saveAlert(db, deviceId, 'warning', 'level',
                    data.level_pct, THRESHOLDS.levelWarnLow,
                    `Tank level low (${data.level_pct}%) — refill soon`));
            }
        }
    }

    // ── pH ────────────────────────────────────────────────────
    if (data.ph !== undefined && data.ph >= 0) {
        if (data.ph < THRESHOLDS.phCritLow || data.ph > THRESHOLDS.phCritHigh) {
            if (shouldAlert('ph_critical')) {
                tasks.push(saveAlert(db, deviceId, 'critical', 'pH',
                    data.ph, `${THRESHOLDS.phCritLow}–${THRESHOLDS.phCritHigh}`,
                    `pH ${data.ph} is outside safe drinking range (BIS IS 10500:2012: 6.5–8.5)`));
            }
        } else if (data.ph < THRESHOLDS.phWarnLow || data.ph > THRESHOLDS.phWarnHigh) {
            if (shouldAlert('ph_warning')) {
                tasks.push(saveAlert(db, deviceId, 'warning', 'pH',
                    data.ph, `${THRESHOLDS.phWarnLow}–${THRESHOLDS.phWarnHigh}`,
                    `pH ${data.ph} approaching unsafe range — monitor closely`));
            }
        }
    }

    // ── Turbidity ─────────────────────────────────────────────
    if (data.turbidity_ntu !== undefined && data.turbidity_ntu >= 0) {
        if (data.turbidity_ntu > THRESHOLDS.turbidityCrit) {
            if (shouldAlert('turbidity_critical')) {
                tasks.push(saveAlert(db, deviceId, 'critical', 'turbidity',
                    data.turbidity_ntu, THRESHOLDS.turbidityCrit,
                    `Turbidity ${data.turbidity_ntu} NTU exceeds BIS limit of 5 NTU — water unsafe`));
            }
        } else if (data.turbidity_ntu > THRESHOLDS.turbidityWarn) {
            if (shouldAlert('turbidity_warning')) {
                tasks.push(saveAlert(db, deviceId, 'warning', 'turbidity',
                    data.turbidity_ntu, THRESHOLDS.turbidityWarn,
                    `Turbidity ${data.turbidity_ntu} NTU — water becoming cloudy`));
            }
        }
    }

    // ── Leak ──────────────────────────────────────────────────
    if (data.leak_detected === true) {
        if (shouldAlert('leak')) {
            tasks.push(saveAlert(db, deviceId, 'critical', 'leak',
                1, 0,
                'Water leak detected at moisture sensor — check tank base and pipes immediately'));
        }
    }

    // ── Battery low ───────────────────────────────────────────
    if (data.battery_v !== undefined && data.battery_v > 0) {
        if (data.battery_v < 3.2) {
            if (shouldAlert('battery_critical')) {
                tasks.push(saveAlert(db, deviceId, 'critical', 'battery',
                    data.battery_v, 3.2,
                    `Battery critically low (${data.battery_v}V) — device may shut down`));
            }
        } else if (data.battery_v < 3.5) {
            if (shouldAlert('battery_warning')) {
                tasks.push(saveAlert(db, deviceId, 'warning', 'battery',
                    data.battery_v, 3.5,
                    `Battery low (${data.battery_v}V) — check solar panel charging`));
            }
        }
    }

    await Promise.all(tasks);
}

module.exports = { checkAlerts };