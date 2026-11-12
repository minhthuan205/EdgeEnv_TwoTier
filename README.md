# EdgeEnv – giám sát nhiệt độ và độ ẩm

Website tiếng Việt chạy trên Windows; không cần thay chương trình Pi đang chạy.

## 1. Cài và chạy trên Windows / VS Code

Cần Python 3.11+ và Node.js 20.19+ (hoặc 22+). Mở thư mục dự án bằng VS Code → Terminal → New Terminal (PowerShell). Các lệnh dưới đây chạy từ thư mục gốc dự án.

```powershell
python -m venv .venv
.venv\Scripts\python.exe -m pip install -r gateway/requirements.txt
Copy-Item gateway/.env.example gateway/.env
npm --prefix web ci
```

Không cần activate môi trường ảo nên không cần đổi ExecutionPolicy.

Terminal 1 – backend:

```powershell
.venv\Scripts\python.exe -m uvicorn gateway.dashboard:app --host 127.0.0.1 --port 8000
```

Terminal 2 – giao diện:

```powershell
npm --prefix web run dev -- --port 5173
```

Mở http://127.0.0.1:5173 trong trình duyệt, zoom 100%. Giữ hai terminal mở. Ctrl+C để dừng. Không dùng `vite preview` cho tích hợp backend vì proxy chỉ được cấu hình trong dev server. Muốn kiểm tra build: `npm --prefix web run build`.

Nếu chưa có broker, web vẫn mở với trạng thái chưa kết nối và chưa có dữ liệu, không tự sinh dữ liệu giả.

## 2. Thử toàn bộ luồng bằng MQTT mô phỏng

Có broker sẵn thì bỏ qua cài broker thử. Không chạy hai broker cùng cổng.

Broker thử tùy chọn chạy hoàn toàn trên Windows, chỉ lắng nghe 127.0.0.1; không cài dịch vụ hay sửa Pi:

```powershell
npm install --prefix .test-tools --no-audit --no-fund aedes@0.51.3
node gateway/dev_broker.cjs
```

Để terminal broker chạy, đợi thanh MQTT báo đã kết nối (backend tự thử lại tối đa khoảng 30 giây). Terminal khác:

```powershell
.venv\Scripts\python.exe gateway/simulate.py --scenario normal
.venv\Scripts\python.exe gateway/simulate.py --scenario rapid
.venv\Scripts\python.exe gateway/simulate.py --scenario threshold
.venv\Scripts\python.exe gateway/simulate.py --scenario stop
```

Chạy từng lệnh, Ctrl+C dừng trước khi đổi kịch bản. Chọn **Mô phỏng**, thiết bị **esp32-01** trên web. `rapid` tăng từ 29 lên 39°C trong khoảng 30 giây, dưới ngưỡng cao 50°C nhưng vượt ngưỡng biến thiên 8°C/60 giây. `threshold` gửi 55°C. `stop` gửi 5 mẫu rồi dừng, chờ hơn 30 giây để thấy mất dữ liệu. Có thể đổi `--interval 1 --count 12` để chạy nhanh. Muốn thử độc lập từng kịch bản, đợi cửa sổ cảnh báo cũ hết hoặc dùng bản SQLite thử riêng bằng biến DASHBOARD_DB.

Mô phỏng có trường `simulation: true`, được lưu và truy vấn riêng, không trộn số liệu thật. Broker thử không có xác thực, chỉ dùng trên loopback.

## 3. Nối broker và Pi 4 thật

Sửa `gateway/.env`, khởi động lại backend:

```ini
MQTT_HOST=192.168.1.100
MQTT_PORT=1883
MQTT_TOPICS=edgeenv/v1/+/telemetry,htvt/pi4/telemetry
MQTT_USER=
MQTT_PASSWORD=
MQTT_TLS=0
```

Đây chỉ là IP ví dụ: thay bằng IP broker thực tế. MQTT là cơ chế gửi/nhận qua topic; broker là máy trung chuyển. Backend subscribe (đăng ký nhận) topic, lưu SQLite và gửi WebSocket (kênh cập nhật liên tục) đến trình duyệt. Trình duyệt không kết nối MQTT TCP.

Code hiện có dùng `edgeenv/v1/<NODE_ID>/telemetry`, payload schema 1 có `records`. Dashboard hỗ trợ trực tiếp, mỗi sensor hiển thị bằng ID `NODE_01/SHT_01`; bản gộp dùng zone. `unix_s=null` được giữ thời gian đo trống; không giả thời gian nhận là thời gian đo. Cờ AI/ngưỡng của nguồn được hiển thị riêng ở “Kết quả tại biên”; cảnh báo dashboard luôn ghi “Backend”.

**Lưu ý tích hợp:** `gateway/service.py` hiện là subscriber lưu dữ liệu và trả offload; chưa republish telemetry tổng hợp từ Pi. Dashboard có thể nghe cùng topic trên broker hiện tại, không cần sửa Pi. Nếu muốn đúng tuyến Pi publish kết quả cuối, cần cung cấp topic và payload Pi đang publish; không thể khẳng định đã có luồng đó chỉ từ mã hiện tại. Không khởi chạy `service.py` trên Windows để thay nhiệm vụ Pi.

Ngoài giao thức hiện tại, chấp nhận định dạng đơn giản **đề xuất** trên topic `htvt/pi4/telemetry`:

```json
{"device_id":"esp32-01","timestamp":"2026-11-10T15:00:00Z","temperature":29.6,"humidity":72.4}
```

Timestamp phải có múi giờ; dữ liệu thật không thêm simulation=true. Cấu hình được danh sách topic phân cách dấu phẩy, TLS và thông tin xác thực. Kiểm tra firewall/mạng để Windows tới được broker. Không mở Internet hoặc đổi cấu hình Pi bằng các lệnh trên.

Thông tin cần cung cấp để hoàn tất phần cứng: IP/hostname và cổng broker, TLS/CA nếu có, topic publish thực tế của Pi, một payload thực (che bí mật), chu kỳ gửi, NODE_ID/sensor/zone, cách đồng bộ đồng hồ, ý nghĩa kết quả xử lý Pi và ngưỡng phù hợp thử nghiệm. Điền mật khẩu trực tiếp vào .env, không gửi trong chat hoặc commit.

## 4. Cảnh báo, chất lượng và lưu dữ liệu

Trang Cài đặt lưu quy tắc vào SQLite, áp dụng chung cho các thiết bị. Ngưỡng cao/thấp là giá trị tuyệt đối. Ngưỡng biến thiên là độ thay đổi trong cửa sổ giây, bắt được cả tăng hoặc giảm; cần ít nhất 3 mẫu và đủ khoảng thời gian tối thiểu. Mất mẫu lâu hơn gap_s sẽ xóa cửa sổ, không đánh giá biến thiên xuyên khoảng gián đoạn. Có hysteresis cho ngưỡng giá trị và clear_s chờ ổn định trước kết thúc; mỗi loại chỉ có một cảnh báo đang mở trên thiết bị/nguồn. “Đã xem” không kết thúc cảnh báo.

Mặc định: 15–50°C, 20–80%RH; biến thiên 8°C hoặc 15%RH trong 60 giây; ít nhất 10 giây dữ liệu; gap 20 giây; mất dữ liệu sau 30 giây; chờ hết điều kiện 10 giây. Đây là **ví dụ thử nghiệm, không phải tiêu chuẩn an toàn**.

SQLite mặc định ở `gateway/data/dashboard.sqlite3`, lưu tự động và còn sau khởi động lại. Mặc định giữ 90 ngày; đổi retention_days tại Cài đặt. Bản ghi cũ và cảnh báo đã kết thúc được dọn định kỳ; cảnh báo đang mở được giữ. Khi dừng backend, sao lưu cả thư mục data để tránh bỏ sót WAL. Không xóa DB nếu cần giữ lịch sử.

Truy vấn thời gian theo lúc backend nhận. Thời gian đo và nhận lưu dạng Unix UTC; web hiển thị Asia/Ho_Chi_Minh. Bộ chọn giờ tùy chỉnh dùng múi giờ máy Windows (nên đặt UTC+7). CSV UTF-8 có BOM dùng được với Excel, chứa thiết bị/khoảng thời gian/nguồn đang chọn và ghi rõ UTC, °C, %RH. Bảng phân trang 50 dòng; biểu đồ tối đa 2.000 mẫu mới nhất; thống kê tính toàn bộ khoảng chọn, bỏ mẫu lỗi. Khoảng trống biểu đồ theo gap_s.

Giới hạn hợp lệ cấu hình bằng VALID_TEMP_MIN/MAX, VALID_HUM_MIN/MAX trong .env. Mẫu ngoài giới hạn lưu chất lượng lỗi và giá trị rỗng, không biến thành bình thường. Bản tin không đọc được ghi lý do trong bảng `rejected`. Trùng bị bỏ qua; sai thứ tự được lưu nhưng không cập nhật trạng thái sống/cảnh báo. Retained không xác nhận sống. Nếu thiết bị chưa có đồng hồ, thứ tự dựa session/seq và thời gian nhận; chưa thể phát hiện tuổi mẫu chưa đồng bộ bị buffer ở nguồn. Cần đồng bộ NTP để xác định độ mới theo thời gian đo.

## 5. Trợ lý AI

Dùng OpenAI Chat Completions qua HTTP backend, không có khóa trong frontend. Theo [tài liệu API chính thức](https://developers.openai.com/api/reference/resources/chat).

Trong gateway/.env điền OPENAI_API_KEY và OPENAI_MODEL (mặc định gpt-4.1-mini; cần tài khoản có quyền dùng model). Khởi động lại backend. Nút Trợ lý AI mở ngăn bên phải, hỏi theo thiết bị/nguồn đã chọn, **1 giờ gần nhất**, không theo khoảng lịch sử tùy chỉnh. Backend gửi thống kê, mẫu mới nhất và tối đa 10 cảnh báo, không gửi toàn bộ lịch sử; tối đa 5 lượt/phút và 600 token trả lời. Câu hỏi tối đa 1.000 ký tự. Không lưu hội thoại, không công cụ điều khiển thiết bị hoặc tự sửa ngưỡng. Thiếu khóa hiện “Chưa cấu hình AI”; API lỗi hiện lỗi riêng, không ngừng MQTT. Sử dụng API có thể phát sinh phí theo tài khoản.

## 6. Tệp chính và kiểm tra

- `web/src/App.tsx`: 4 trang, biểu đồ hai trục, CSV và ngăn chat; WebSocket cập nhật 2 giây, HTTP 3 giây dự phòng.
- `web/src/style.css`: giao diện responsive; mục tiêu tổng quan laptop 1366×768.
- `web/vite.config.ts`: proxy /api và /ws tới backend cổng 8000.
- `gateway/dashboard.py`: FastAPI, MQTT tự reconnect, SQLite, cảnh báo, CSV và AI. Chạy 1 worker để duy trì cửa sổ cảnh báo/rate limit nhất quán.
- `gateway/simulate.py`: phát dữ liệu thử qua MQTT.
- `gateway/service.py`: gateway/offload cũ trên Pi, giữ nguyên.
- `tests/test_dashboard.py`: kiểm tra dữ liệu, cảnh báo, lưu DB, CSV, AI lỗi.

```powershell
.venv\Scripts\python.exe -m unittest discover -s tests -p test_dashboard.py -v
npm --prefix web run build
```

Cấu hình cửa sổ biến thiên đang ở RAM: sau restart cần thu đủ mẫu mới để đánh giá, tránh bịa tốc độ; cảnh báo đang mở và cấu hình vẫn giữ trên đĩa. Đây là ứng dụng cục bộ, chưa có xác thực; không bind 0.0.0.0 hay công khai API. Chưa triển khai điều khiển phần cứng, email hoặc cloud.

### Kết quả kiểm chứng trong phiên triển khai

- Frontend TypeScript + Vite build thành công; backend Uvicorn khởi động và HTTP trả 200.
- Broker Aedes cục bộ nhận bộ phát MQTT; backend lưu 18 mẫu mô phỏng và WebSocket trả dữ liệu thực từ DB.
- Cảnh báo tăng nhanh, vượt ngưỡng, mất dữ liệu đã xuất hiện qua luồng MQTT thử.
- Dừng rồi khởi động lại Uvicorn: vẫn có đủ 18 mẫu. CSV trả đúng 18 dòng, BOM UTF-8.
- 5 bài kiểm thử tự động qua: giao thức cũ, trùng/sai thứ tự/retained/lỗi, khoảng gián đoạn, cảnh báo, CSV, xác nhận đã xem, thiếu khóa và lỗi API AI (mock).
- Chưa xác minh Pi/ESP32 thật hoặc gọi AI trả lời thật vì chưa có phần cứng/khóa.
- Chưa kiểm chứng trực quan ở 1366×768: công cụ trình duyệt/Computer Use không khởi tạo được do lỗi sandbox. CSS đã hướng tới kích thước này nhưng cần mở trình duyệt để xác nhận.
- Build có cảnh báo bundle hơn 500 kB; không chặn chạy cục bộ. Backend test có cảnh báo deprecation của TestClient/httpx; test vẫn chạy qua.
