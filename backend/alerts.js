// backend/alerts.js
// Threshold checking and alert saving.
// Called by server.js after every sensor reading.
// db (Firestore) is passed in as a parameter — no direct Firebase init here.

const admin = require('firebase-admin');

// Default thresholds — used if nothing is saved in Firestore yet
const DEFAULT_THRESHOLDS = {
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

// Cache thresholds in memory — refreshed every 60 seconds
// so the alert engine picks up app changes quickly without
// fetching from Firestore on every single reading
let cachedThresholds = { ...DEFAULT_THRESHOLDS };
let lastFetch = 0;
const CACHE_TTL_MS = 60 * 1000; // 60 seconds

async function getThresholds(db, deviceId) {
    const now = Date.now();
    if (now - lastFetch < CACHE_TTL_MS) {
        return cachedThresholds; // use cached values
    }
    try {
        const doc = await db.collection('devices').doc(deviceId).get();
        const saved = doc.data()?.thresholds;
        if (saved && Object.keys(saved).length > 0) {
            // Merge saved values over defaults
            // so any missing keys still have sensible fallbacks
            cachedThresholds = { ...DEFAULT_THRESHOLDS, ...saved };
        }
        lastFetch = now;
        console.log('[Alerts] Thresholds refreshed from Firestore');
    } catch (err) {
        console.error('[Alerts] Could not fetch thresholds — using cached:', err.message);
    }
    return cachedThresholds;
}

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

    // Offline alert — no sensor data, just mark offline
    if (data._offline) {
        if (shouldAlert('offline')) {
            tasks.push(saveAlert(db, deviceId, 'critical', 'offline',
                data.offlineMinutes, 10,
                `Sensor node offline — no data for ${data.offlineMinutes} minutes`));
        }
        await Promise.all(tasks);
        return;
    }

    // Fetch current thresholds (cached, refreshes every 60s)
    const T = await getThresholds(db, deviceId);

    // Water level
    if (data.level_pct !== undefined && data.level_pct >= 0) {
        if (data.level_pct >= T.levelHigh) {
            if (shouldAlert('level_high')) {
                tasks.push(saveAlert(db, deviceId, 'critical', 'level',
                    data.level_pct, T.levelHigh,
                    `Tank full (${data.level_pct}%) — supply valve closed automatically`));
            }
        } else if (data.level_pct <= T.levelCritLow) {
            if (shouldAlert('level_crit_low')) {
                tasks.push(saveAlert(db, deviceId, 'critical', 'level',
                    data.level_pct, T.levelCritLow,
                    `Tank critically low (${data.level_pct}%) — refill immediately`));
            }
        } else if (data.level_pct <= T.levelWarnLow) {
            if (shouldAlert('level_warn_low')) {
                tasks.push(saveAlert(db, deviceId, 'warning', 'level',
                    data.level_pct, T.levelWarnLow,
                    `Tank level low (${data.level_pct}%) — refill soon`));
            }
        }
    }

    // pH
    if (data.ph !== undefined && data.ph >= 0) {
        if (data.ph < T.phCritLow || data.ph > T.phCritHigh) {
            if (shouldAlert('ph_critical')) {
                tasks.push(saveAlert(db, deviceId, 'critical', 'pH',
                    data.ph, `${T.phCritLow}–${T.phCritHigh}`,
                    `pH ${data.ph} is outside safe drinking range (BIS IS 10500:2012: 6.5–8.5)`));
            }
        } else if (data.ph < T.phWarnLow || data.ph > T.phWarnHigh) {
            if (shouldAlert('ph_warning')) {
                tasks.push(saveAlert(db, deviceId, 'warning', 'pH',
                    data.ph, `${T.phWarnLow}–${T.phWarnHigh}`,
                    `pH ${data.ph} approaching unsafe range — monitor closely`));
            }
        }
    }

    // Turbidity
    if (data.turbidity_ntu !== undefined && data.turbidity_ntu >= 0) {
        if (data.turbidity_ntu > T.turbidityCrit) {
            if (shouldAlert('turbidity_critical')) {
                tasks.push(saveAlert(db, deviceId, 'critical', 'turbidity',
                    data.turbidity_ntu, T.turbidityCrit,
                    `Turbidity ${data.turbidity_ntu} NTU exceeds BIS limit of 5 NTU — water unsafe`));
            }
        } else if (data.turbidity_ntu > T.turbidityWarn) {
            if (shouldAlert('turbidity_warning')) {
                tasks.push(saveAlert(db, deviceId, 'warning', 'turbidity',
                    data.turbidity_ntu, T.turbidityWarn,
                    `Turbidity ${data.turbidity_ntu} NTU — water becoming cloudy`));
            }
        }
    }

    // Leak
    if (data.leak_detected === true) {
        if (shouldAlert('leak')) {
            tasks.push(saveAlert(db, deviceId, 'critical', 'leak',
                1, 0,
                'Water leak detected at moisture sensor — check tank base and pipes immediately'));
        }
    }

    // Battery
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