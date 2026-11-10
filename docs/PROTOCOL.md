# MQTT v1

Root: `edgeenv/v1/<NODE_ID>/`. NODE_ID chỉ dùng chữ, số, gạch dưới/gạch ngang, riêng cho mỗi ESP32.
Broker ở Pi; ESP32 là publisher/subscriber. SHT30 là slave I²C, không phải MQTT client.

| Suffix | Chiều | Nội dung | Retain | QoS đang dùng |
|---|---|---|---|---|
| telemetry | ESP32 → Pi | Snapshot định kỳ, records từng nguồn hoặc gộp | không | 0 |
| events | ESP32 → Pi | Thay đổi lỗi/ngưỡng/AI của một sensor | không | 0 |
| status | ESP32 → Pi | `online` / LWT `offline` | có | online 0, LWT 1 |
| offload/request | ESP32 → Pi | Cửa sổ mẫu có request_id | không | 0 |
| offload/response | Pi → ESP32 | Kết quả tham khảo | không | 1 |

PubSubClient 2.8 không cung cấp QoS1 cho publish thông thường. Subscribe QoS1 không biến nguồn publish QoS0 thành QoS1.
Nếu yêu cầu không mất cảnh báo: bổ sung application ACK + flash spool + loại trùng, hoặc chuyển client hỗ trợ QoS1. Hàng đợi RAM hiện chỉ chờ publish thành công, không phải ACK từ Pi.

## Envelope

```json
{
  "schema": 1,
  "kind": "telemetry",
  "node_id": "NODE_01",
  "session_id": "1234abcd",
  "seq": 12,
  "uptime_ms": 15000,
  "unix_s": null,
  "sample_hz": 1,
  "event_drops": 0,
  "missed_slots": 0,
  "records": [
    {
      "sensor_id": "SHT_01",
      "zone_id": "ZONE_01",
      "source_ids": ["SHT_01"],
      "aggregation": "none",
      "temperature_c": 30.2,
      "humidity_rh": 62.1,
      "valid": true,
      "stale": false,
      "sample_uptime_ms": 14016,
      "over_temperature": false,
      "over_humidity": false,
      "ai_alarm": false,
      "ai_status": "disabled",
      "ai_probability": null,
      "errors": 0
    }
  ]
}
```

`unix_s` chỉ có khi đồng bộ được thời gian qua NTP; Pi luôn lưu `received_unix_s`. Đây là thời gian nhận, không mặc định là thời gian đo.
Gateway dùng tổ hợp `(node_id, session_id, seq)` để loại trùng. Session tạo ngẫu nhiên lúc boot, không phải ID phần cứng an toàn mật mã.
`seq` dùng chung giữa các loại message; khoảng hở seq trên telemetry không nhất thiết là mất gói vì có events/offload xen vào.
Khi gộp: `sensor_id=null`, `source_ids` gồm 2 ID, `aggregation=mean`, giá trị trung bình, spread và timestamp từng nguồn; không chứa giá trị raw từng nguồn. AI probability tổng hợp để null thay vì bịa xác suất của trung bình.
`errors` là đếm lỗi tích lũy của sensor đại diện; với bản gộp không được coi đây là tổng lỗi của mọi sensor.

## Offload

Request có envelope trên với kind `offload_request`, thêm `request_id`, `sensor_id`, `method=linear_trend_baseline` và `samples`.
Mỗi row samples: `[relative_ms, temperature_c, humidity_rh]`. Relative_ms lấy từ timestamp mẫu đầu; không gộp các sensor vào cùng cửa sổ.
Response có schema, kind `offload_response`, node_id, sensor_id, request_id, label, slope_c_per_s, fit_rmse_c, processing_ms, trained_ai=false và forecast=null.
Node chỉ nhận đúng request_id đang chờ và trong 5 giây. Kết quả đến muộn/không khớp bị bỏ. Không retain request/response.
Có tối đa một yêu cầu đang chờ trên mỗi node, không có retry tác vụ cũ sau khi hết hạn.
RTT Serial bao gồm gửi–đợi–nhận; processing_ms trên Pi chỉ bao gồm phân tích, không gồm thời gian truyền, lưu DB và hàng đợi.

## Định nghĩa cảnh báo

- `over_temperature`: vượt ngưỡng tuyệt đối, hysteresis độc lập.
- `over_humidity`: vượt 70%RH, hysteresis độc lập.
- `ai_alarm`: mô hình local xác định lớp được cấu hình với score ≥0.8.
- `valid=false`/`stale=true`: lỗi chất lượng dữ liệu, không hiển thị là bình thường.
- LED hiện sáng cho bất kỳ cảnh báo hoặc lỗi sensor. Một LED không phân biệt được nguyên nhân; xem Serial/web sau này hoặc thêm LED.

Hysteresis giữ cảnh báo T từ >50°C cho đến khi ≤49°C, RH từ >70% đến ≤68%. Không đổi mức kích hoạt đang dùng.
