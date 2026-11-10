# EdgeEnv — Giám sát nhiệt/ẩm với hai tầng biên

Bản khởi đầu triển khai cho môn **Hệ thống Viễn thông**, ngày 10/11/2026.
Tầng biên L1: ESP32 + SHT30 + LED. Tầng biên L2: Raspberry Pi 4.
Cả hai tầng đều xử lý tại chỗ; chatbot gọi API ngoài là thành phần cloud tùy chọn.

## Đã có trong gói này

- Firmware Arduino mới: 2 cảm biến trên hai bus đúng sơ đồ; cấu hình mở rộng 3 cảm biến.
- Lấy mẫu 1 Hz **mỗi cảm biến**, đo tuần tự, ID riêng, CRC, bộ đệm riêng.
- Single-shot: cảm biến không đo được để ở idle, **không cắt nguồn vật lý**.
- Ngưỡng bật: T > 50°C hoặc RH > 70%RH; hysteresis nhả: T ≤49°C, RH ≤68%RH.
- Tích hợp Edge Impulse tùy chọn; không chạy nếu tần số model khác tần số lấy mẫu.
- MQTT định kỳ 5 giây; sự kiện đổi cảnh báo gửi ở lượt mạng kế tiếp, không phải thời gian thực cứng.
- Gộp một cặp cảm biến cùng zone, khi dữ liệu mới/gần nhau và AI xác nhận bình thường; tắt mặc định.
- Mẫu gateway Python thật: nhận MQTT, lưu SQLite, xuất CSV, trả phân tích tuyến tính qua MQTT.
- Tài liệu kiến trúc, giao thức, sơ đồ thư mục, lộ trình web/chatbot và kịch bản kiểm thử.

## Chưa hoàn thành / giới hạn phải biết

- Chưa biên dịch bằng toolchain ESP32/Arduino và chưa chạy trên mạch thật: môi trường hiện tại không có Arduino CLI, board và cảm biến.
- Đã chạy kiểm thử logic C++ trên máy chủ và kiểm thử gateway Python. Không xem đó là kiểm thử end-to-end.
- Chưa có mô hình được train lại ở 1 Hz; mặc định `ENABLE_EDGE_IMPULSE=0`.
- Không đóng gói lại SDK/model bên thứ ba: cài thư viện Edge Impulse từ ZIP nhóm đã gửi.
- Chưa có web hoạt động, API web, dự báo đã kiểm chứng, chatbot hay model AI mạnh trên Pi.
- Phần offload hiện tại là baseline tuyến tính để thử giao thức hai chiều; chưa chứng minh tiết kiệm điện hay tăng độ chính xác.
- PubSubClient gửi QoS 0: không bảo đảm mọi sự kiện đều đến Pi. Hàng đợi RAM 16 sự kiện, đầy bỏ sự kiện mới và tăng `event_drops`; mất điện mất hàng đợi.
- Một lần suy luận lâu có thể làm trễ lấy mẫu; có `missed_slots` và reset cửa sổ khi gián đoạn. Đo thực tế trước khi demo.
- Serial 115200 cần đọc được để thấy lỗi, mismatch và kết quả offload. Firmware không in mật khẩu.

## Mở firmware

Mở `firmware/EdgeEnvNode/EdgeEnvNode.ino` bằng Arduino IDE. Giữ cả ba file `.h` trong cùng thư mục.

1. Cài board package **esp32 by Espressif Systems** và chọn **ESP32 Dev Module** cho ESP32-WROOM trong ảnh.
2. Cài **ArduinoJson 7.x** và **PubSubClient 2.8** qua Library Manager. Driver SHT30 single-shot và CRC đã viết trực tiếp, không cần Adafruit SHT31.
3. Sửa SSID, mật khẩu Wi-Fi, IP Pi, tài khoản broker trong `Config.h`.
4. Kiểm tra LED đang ở GPIO nào. File dùng GPIO25 là giá trị đề xuất, không suy ra được chân LED từ ảnh; nếu đang ở chân khác phải đổi.
5. Giữ `SENSOR_COUNT=2`, `SAMPLE_HZ=1`, `ENABLE_EDGE_IMPULSE=0` để kiểm tra đo/ngưỡng/MQTT trước.
6. Nạp và mở Serial Monitor 115200. Hai ID phải cho dữ liệu riêng. LED sáng khi lỗi cảm biến hoặc có cảnh báo.
7. Với nhánh 3 cảm biến, đặt `SENSOR_COUNT=3` và đấu nối đúng tài liệu phần cứng.

## Hai lựa chọn mô hình

| Chế độ | SAMPLE_HZ | ENABLE_EDGE_IMPULSE | Điều kiện |
|---|---:|---:|---|
| Đo/ngưỡng/MQTT trước | 1 | 0 | Chạy được không cần thư viện model |
| Kiểm tra model nhóm gửi | 7 | 1 | Cài ZIP thư viện cũ, giữ model 7 Hz |
| Mục tiêu 1 Hz + AI | 1 | 1 | Train/export lại model 1 Hz, hai trục temperature rồi humidity |

Gói cũ có 21 mẫu, 7 Hz, 2 trục, 2 lớp `binhthuong` và `qua_nhiet`.
Không sửa macro trong SDK để giả model 7 Hz thành 1 Hz. Không lặp một mẫu 7 lần để lấp cửa sổ.
Nếu bật EI nhưng model 7 Hz tại cấu hình 1 Hz, firmware vẫn đo/ngưỡng/MQTT và gửi `model_rate_mismatch`, không đưa ra quyết định AI giả.
Gợi ý thu dữ liệu 1 Hz và thử cửa sổ 10–15 giây, bước suy luận 1 giây. Cần so sánh với cửa sổ ngắn hơn theo độ trễ và bỏ sót. Khi train lại, số mẫu lấy trực tiếp từ metadata thư viện.

Để thử gộp, sau khi AI tương thích đã chạy: xác nhận SHT_01/SHT_02 cùng khu vực rồi bật `MERGE_ENABLED=true`.
Khi AI disabled/mismatch/warming-up hoặc có báo động, firmware cố ý giữ các phép đo riêng. Mỗi chu kỳ vẫn chỉ phát một payload telemetry; payload có thể chứa nhiều records.

## Chạy gateway trên Pi 4

Chi tiết broker: `docs/DEPLOYMENT.md`.

```bash
cd gateway
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
export MQTT_HOST=127.0.0.1
export MQTT_USER=gateway
export MQTT_PASSWORD='THAY_MAT_KHAU'
python service.py
```

Xuất CSV từ cơ sở dữ liệu đang có, chạy trong terminal khác:

```bash
cd gateway
source .venv/bin/activate
python service.py --export ../telemetry.csv
```

CSV chỉ chứa bản ghi telemetry thực sự nhận được (mặc định 5 giây/lần), **không phải** toàn bộ mẫu 1 Hz dùng cho AI.
Nếu dữ liệu đã gộp thì CSV là dữ liệu gộp; không thể khôi phục chính xác từng cảm biến.
Muốn thu train: dùng chế độ đo riêng và bổ sung luồng ghi raw 1 Hz/Serial riêng trước khi train.

## Thử offload

Sau khi MQTT và gateway đã ổn: đổi `OFFLOAD_POLICY=2`, nạp lại ESP32.
Mỗi tối thiểu 15 giây thử gửi lịch sử 15 mẫu của một cảm biến theo round-robin.
Pi trả nhãn xu hướng, hệ số dốc, RMSE đường thẳng, mã yêu cầu; ESP32 in RTT lên Serial.
Mất kết quả sau 5 giây thì timeout; xử lý local tiếp tục.
`OFFLOAD_POLICY=1` chỉ gửi khi xác suất lớp cảnh báo local trong [0.4,0.8); đây là luật thử đơn giản, không phải tối ưu năng lượng.
Kết quả Pi là tham khảo; không được tắt cảnh báo tại chỗ.

## Chạy kiểm thử máy chủ

```bash
g++ -std=c++17 -Wall -Wextra -Werror tests/test_logic.cpp -o /tmp/edgeenv_test
/tmp/edgeenv_test
python -m unittest discover -s tests -p 'test_*.py' -v
```

Đọc tiếp: `docs/ARCHITECTURE.md`, `docs/HARDWARE.md`, `docs/PROTOCOL.md`, `docs/TEST_PLAN.md`, `web/README.md`.
