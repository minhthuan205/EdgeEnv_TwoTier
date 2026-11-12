import json, os, tempfile, time, unittest
from unittest.mock import patch
from fastapi.testclient import TestClient
from gateway import dashboard as d

class DashboardTests(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory(); self.path=self.tmp.name+'/test.db'; self.s=d.Store(self.path); self.old=d.store; d.store=self.s; self.now=time.time(); self.client=TestClient(d.app)
 def tearDown(self):
  d.store=self.old; self.s.db.close(); self.tmp.cleanup()
 def send(self,t=29,h=60,offset=0,**extra):
  body=dict(device_id='test',timestamp=d.utc(self.now+offset),temperature=t,humidity=h,simulation=True);body.update(extra);self.s.ingest('htvt/pi4/telemetry',json.dumps(body),now=self.now+offset)
 def test_rapid_threshold_missing_persistence(self):
  for i,t in enumerate([29,34,39]): self.send(t,offset=i*6)
  kinds=[r[0] for r in self.s.db.execute('SELECT kind FROM alerts WHERE end IS NULL')];self.assertIn('temp_change',kinds);self.assertNotIn('temp_high',kinds)
  self.send(55,offset=18); self.send(56,offset=21)
  self.assertEqual(self.s.db.execute("SELECT count(*) FROM alerts WHERE kind='temp_high'").fetchone()[0],1)
  self.s.tick(self.now+60);self.assertTrue(self.s.db.execute("SELECT 1 FROM alerts WHERE kind='missing'").fetchone())
  new=d.Store(self.path);self.assertEqual(new.db.execute('SELECT count(*) FROM readings').fetchone()[0],5);new.db.close()
 def test_quality_and_duplicates(self):
  self.send();self.send();self.send(offset=-10)
  self.s.ingest('x','[]');self.s.ingest('x','bad')
  self.send(t=999,offset=1)
  self.s.ingest('x',json.dumps(dict(device_id='retained',timestamp=d.utc(self.now),temperature=30,humidity=60)),True,now=self.now)
  self.assertEqual(self.s.db.execute('SELECT count(*) FROM readings WHERE fresh=1').fetchone()[0],1)
  self.assertEqual(self.s.db.execute('SELECT count(*) FROM rejected').fetchone()[0],2)
 def test_csv_filter_chat_and_seen(self):
  self.send(55);self.send(30,offset=-100)
  q=dict(device='test',mode='simulation',start=self.now-1,end=self.now+1)
  r=self.client.get('/api/csv',params=q);self.assertEqual(r.status_code,200);self.assertEqual(len(r.text.strip().splitlines()),2);self.assertTrue(r.content.startswith(b'\xef\xbb\xbf'))
  self.client.post('/api/alerts/1/seen');self.assertIsNone(self.s.db.execute('SELECT end FROM alerts WHERE id=1').fetchone()[0])
  with patch.dict(os.environ,{'OPENAI_API_KEY':''}): self.assertEqual(self.client.post('/api/chat',json={'device':'test','question':'Tình trạng?'}).status_code,503)
  with patch.dict(os.environ,{'OPENAI_API_KEY':'test'}),patch('gateway.dashboard.httpx.AsyncClient.post',side_effect=RuntimeError('offline')): self.assertEqual(self.client.post('/api/chat',json={'device':'test','question':'Tình trạng?'}).status_code,502)
 def test_no_rate_after_gap_and_rules(self):
  self.send(29);self.send(30,offset=3);self.send(40,offset=40)
  self.assertFalse(self.s.db.execute("SELECT 1 FROM alerts WHERE kind='temp_change'").fetchone())
  self.assertEqual(self.client.put('/api/settings',json={'temp_low':60,'temp_high':50}).status_code,422)
 def test_existing_protocol(self):
  r=dict(sensor_id='SHT_01',temperature_c=30,humidity_rh=60,valid=True,sample_uptime_ms=900,ai_status='disabled')
  body=dict(schema=1,kind='telemetry',node_id='NODE_01',session_id='s',seq=1,uptime_ms=1000,unix_s=None,records=[r])
  self.s.ingest('edgeenv/v1/NODE_01/telemetry',json.dumps(body));row=self.s.db.execute('SELECT * FROM readings').fetchone();self.assertEqual(row['device'],'NODE_01/SHT_01');self.assertIsNone(row['measured'])
if __name__=='__main__': unittest.main()
