"""MQTT L2 gateway starter. Stores payloads, exports CSV, responds to offload.
No web/chatbot/model server is claimed by this starter.
"""
import argparse
import csv
import json
import os
from pathlib import Path
import re
import sqlite3
import time
from analysis import analyze

TOPIC = re.compile(r"^edgeenv/v1/([A-Za-z0-9_-]{1,40})/(telemetry|events|offload/request)$")


def open_db(path):
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    db = sqlite3.connect(path)
    db.execute("PRAGMA journal_mode=WAL")
    db.execute("""CREATE TABLE IF NOT EXISTS messages (
        id INTEGER PRIMARY KEY, received_unix_s REAL NOT NULL,
        node_id TEXT NOT NULL, session_id TEXT NOT NULL,
        seq INTEGER NOT NULL, topic TEXT NOT NULL, payload TEXT NOT NULL,
        UNIQUE(node_id, session_id, seq))""")
    db.commit()
    return db


def ingest(db, topic, raw):
    match = TOPIC.fullmatch(topic)
    if not match or len(raw) > 32768:
        raise ValueError("Unexpected topic or excessive payload")
    def reject_constant(value):
        raise ValueError("Invalid JSON constant: " + value)
    d = json.loads(raw, parse_constant=reject_constant)
    if not isinstance(d, dict) or d.get("schema") != 1 or d.get("node_id") != match[1]:
        raise ValueError("Invalid schema/node")
    kinds = {"telemetry": "telemetry", "events": "event", "offload/request": "offload_request"}
    if d.get("kind") != kinds[match[2]]:
        raise ValueError("Topic/kind mismatch")
    if not isinstance(d.get("session_id"), str) or not 1 <= len(d["session_id"]) <= 64:
        raise ValueError("Invalid session")
    if type(d.get("seq")) is not int or d["seq"] < 1:
        raise ValueError("Invalid sequence")
    if match[2] == "telemetry" and not isinstance(d.get("records"), list):
        raise ValueError("Missing records")
    response = None
    if match[2] == "offload/request":
        if d.get("method") != "linear_trend_baseline":
            raise ValueError("Unsupported offload method")
        rid = d.get("request_id")
        if not isinstance(rid, str) or not 1 <= len(rid) <= 100:
            raise ValueError("Invalid request ID")
        start = time.perf_counter()
        response = analyze(d.get("samples"))
        response.update(schema=1, kind="offload_response", node_id=match[1],
                        sensor_id=d.get("sensor_id"), request_id=rid,
                        processing_ms=(time.perf_counter()-start)*1000)
    db.execute("INSERT OR IGNORE INTO messages(received_unix_s,node_id,session_id,seq,topic,payload) VALUES(?,?,?,?,?,?)",
               (time.time(), match[1], d["session_id"], d["seq"], topic, json.dumps(d, ensure_ascii=False)))
    db.commit()
    return match[1], response


def export_csv(db, path):
    # Export only transmitted telemetry, never fabricate individual raw readings.
    fields = ["received_unix_s", "sample_unix_s", "node_id", "session_id", "seq", "zone_id",
              "source_ids", "aggregation", "temperature_c", "humidity_rh", "valid", "stale",
              "ai_status", "ai_probability", "over_temperature", "over_humidity", "ai_alarm"]
    with open(path, "w", newline="", encoding="utf-8-sig") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for received, payload in db.execute("SELECT received_unix_s,payload FROM messages ORDER BY id"):
            d = json.loads(payload)
            if d.get("kind") != "telemetry":
                continue
            for r in d["records"]:
                row = {k: r.get(k) for k in fields}
                row.update(received_unix_s=received, node_id=d["node_id"],
                           session_id=d["session_id"], seq=d["seq"],
                           source_ids="|".join(r.get("source_ids", [])))
                # Reconstruct measurement time only if node clock was synchronized.
                if d.get("unix_s") is not None and r.get("sample_uptime_ms") is not None:
                    age = (d["uptime_ms"]-r["sample_uptime_ms"]) % (2**32)
                    row["sample_unix_s"] = d["unix_s"]-age/1000
                writer.writerow(row)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--db", default=os.getenv("EDGEENV_DB", "./data/edgeenv.sqlite3"))
    parser.add_argument("--export", help="Export stored telemetry and exit")
    args = parser.parse_args()
    db = open_db(args.db)
    if args.export:
        export_csv(db, args.export)
        db.close()
        return
    import paho.mqtt.client as mqtt
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="edgeenv-pi4")
    if os.getenv("MQTT_USER"):
        client.username_pw_set(os.environ["MQTT_USER"], os.environ.get("MQTT_PASSWORD", ""))

    def on_connect(c, userdata, flags, reason_code, properties):
        if reason_code == 0:
            for suffix in ("telemetry", "events", "offload/request"):
                c.subscribe("edgeenv/v1/+/"+suffix, qos=1)
            print("Connected; subscribed", flush=True)
        else:
            print("Connection rejected:", reason_code, flush=True)

    def on_message(c, userdata, msg):
        try:
            node, response = ingest(db, msg.topic, msg.payload)
            if response is not None:
                # Never retain results: stale decisions must not replay on reconnect.
                c.publish(f"edgeenv/v1/{node}/offload/response", json.dumps(response), qos=1, retain=False)
        except (ValueError, TypeError, KeyError, sqlite3.Error) as exc:
            print("Message rejected:", str(exc), flush=True)

    client.on_connect = on_connect
    client.on_message = on_message
    client.reconnect_delay_set(1, 30)
    client.connect(os.getenv("MQTT_HOST", "127.0.0.1"), int(os.getenv("MQTT_PORT", "1883")), 30)
    try:
        client.loop_forever() # same thread owns SQLite
    finally:
        client.disconnect()
        db.close()


if __name__ == "__main__":
    main()
