import csv
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/"gateway"))
from analysis import analyze
from service import ingest, open_db, export_csv


class GatewayTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.db = open_db(Path(self.temp.name)/"test.sqlite")

    def tearDown(self):
        self.db.close()
        self.temp.cleanup()

    def base(self, kind="telemetry", seq=1):
        return dict(schema=1,kind=kind,node_id="NODE_01",session_id="boot1",seq=seq)

    def test_rise_below_threshold_vs_slow(self):
        rapid = analyze([[i*1000,30+i,60] for i in range(10)])
        slow = analyze([[i*1000,30+i*0.02,60] for i in range(10)])
        self.assertEqual(rapid["label"], "rapid_rise_candidate")
        self.assertEqual(slow["label"], "no_rapid_rise_by_baseline")
        self.assertFalse(rapid["trained_ai"])

    def test_hot_stable_not_rapid(self):
        self.assertEqual(analyze([[0,60,60],[1000,60,60],[2000,60,60]])["slope_c_per_s"], 0)

    def test_corrupt_window(self):
        for rows in ([[0,30,60],[0,31,60],[1000,32,60]],
                     [[0,30,60],[1000,float("nan"),60],[2000,32,60]]):
            with self.assertRaises(ValueError): analyze(rows)

    def test_store_dedup_export_merged_not_fake_raw(self):
        d=self.base(); d.update(unix_s=None,uptime_ms=5000,records=[dict(
            zone_id="ZONE_01",source_ids=["SHT_01","SHT_02"],aggregation="mean",
            temperature_c=30.2,humidity_rh=60,valid=True,stale=False)])
        raw=json.dumps(d).encode()
        for _ in range(2): ingest(self.db,"edgeenv/v1/NODE_01/telemetry",raw)
        self.assertEqual(self.db.execute("SELECT COUNT(*) FROM messages").fetchone()[0],1)
        path=Path(self.temp.name)/"out.csv"; export_csv(self.db,path)
        with path.open(encoding="utf-8-sig") as f: rows=list(csv.DictReader(f))
        self.assertEqual(len(rows),1)
        self.assertEqual(rows[0]["source_ids"],"SHT_01|SHT_02")
        self.assertEqual(rows[0]["sample_unix_s"],"")

    def test_request_response_correlated(self):
        d=self.base("offload_request"); d.update(request_id="req-1",sensor_id="SHT_02",
          method="linear_trend_baseline",samples=[[0,30,60],[1000,31,60],[2000,32,60]])
        node,r=ingest(self.db,"edgeenv/v1/NODE_01/offload/request",json.dumps(d).encode())
        self.assertEqual((node,r["request_id"],r["sensor_id"]),("NODE_01","req-1","SHT_02"))

    def test_topic_identity_mismatch(self):
        d=self.base(); d["records"]=[]
        with self.assertRaises(ValueError):
            ingest(self.db,"edgeenv/v1/NODE_02/telemetry",json.dumps(d).encode())


if __name__=="__main__": unittest.main()
