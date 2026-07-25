-- TerraPulse SQLite schema bootstrap.
-- Clean-room equivalent of SeisComp's shipped DB bootstrap scripts.

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS schema_version (
    component TEXT PRIMARY KEY,
    version INTEGER NOT NULL,
    applied_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS structures (
    object_id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    latitude REAL,
    longitude REAL,
    description TEXT
);

CREATE TABLE IF NOT EXISTS sensors (
    object_id INTEGER NOT NULL,
    sensor_id INTEGER NOT NULL,
    model TEXT,
    location TEXT,
    status TEXT NOT NULL DEFAULT 'unknown',
    last_seen_ms INTEGER,
    PRIMARY KEY (object_id, sensor_id),
    FOREIGN KEY (object_id) REFERENCES structures(object_id)
);

CREATE TABLE IF NOT EXISTS channels (
    object_id INTEGER NOT NULL,
    sensor_id INTEGER NOT NULL,
    component INTEGER NOT NULL,
    sample_rate REAL NOT NULL,
    unit TEXT NOT NULL,
    gain REAL NOT NULL DEFAULT 1.0,
    PRIMARY KEY (object_id, sensor_id, component),
    FOREIGN KEY (object_id, sensor_id) REFERENCES sensors(object_id, sensor_id)
);

CREATE TABLE IF NOT EXISTS events (
    event_id TEXT PRIMARY KEY,
    object_id INTEGER NOT NULL,
    created_ms INTEGER NOT NULL,
    updated_ms INTEGER NOT NULL,
    type TEXT NOT NULL,
    severity TEXT NOT NULL,
    status TEXT NOT NULL,
    FOREIGN KEY (object_id) REFERENCES structures(object_id)
);

CREATE TABLE IF NOT EXISTS qc_metrics (
    object_id INTEGER NOT NULL,
    sensor_id INTEGER NOT NULL,
    metric TEXT NOT NULL,
    value REAL NOT NULL,
    time_ms INTEGER NOT NULL,
    PRIMARY KEY (object_id, sensor_id, metric, time_ms)
);

INSERT OR REPLACE INTO schema_version(component, version)
VALUES ('core', 1);
