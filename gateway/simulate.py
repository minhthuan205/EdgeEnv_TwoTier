import argparse, json, math, os, time
from datetime import datetime, timezone
from pathlib import Path
import paho.mqtt.client as mqtt
from dotenv import load_dotenv
load_dotenv(Path(__file__).with_name('.env'))
p=argparse.ArgumentParser(); p.add_argument('--scenario',choices=['normal','rapid','threshold','stop'],default='normal'); p.add_argument('--interval',type=float,default=3); p.add_argument('--count',type=int,default=60); args=p.parse_args()
c=mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
if os.getenv('MQTT_USER'): c.username_pw_set(os.environ['MQTT_USER'],os.getenv('MQTT_PASSWORD',''))
if os.getenv('MQTT_TLS','0')=='1': c.tls_set()
c.connect(os.getenv('MQTT_HOST','127.0.0.1'),int(os.getenv('MQTT_PORT','1883'))); c.loop_start()
try:
    for i in range(args.count):
        if args.scenario=='stop' and i>=5: print('Ngừng gửi để kiểm tra mất dữ liệu'); break
        t=29+math.sin(i/5) if args.scenario in ('normal','stop') else 29+min(i,10) if args.scenario=='rapid' else 55
        d=dict(device_id='esp32-01',timestamp=datetime.now(timezone.utc).isoformat(),temperature=t,humidity=65+math.sin(i/7),simulation=True)
        c.publish('htvt/pi4/telemetry',json.dumps(d),qos=1,retain=False).wait_for_publish(); print(d); time.sleep(args.interval)
finally: c.disconnect(); c.loop_stop()
