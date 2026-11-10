# EdgeEnv Web — frontend mô phỏng

## Chạy bản web đầu tiên trên laptop

Cần Node.js 20.19+ hoặc 22.12+ và npm. Trong terminal:

```bash
cd web
npm install
npm run dev
```

Mở địa chỉ do Vite in ra (mặc định http://127.0.0.1:5173), rồi bấm **Bắt đầu**.
Server mặc định chỉ lắng nghe laptop. Để thử trên điện thoại cùng Wi-Fi, chạy
`npm run dev -- --host 0.0.0.0` và mở địa chỉ IP LAN của laptop với cổng Vite.
Giao diện responsive; chưa triển khai website lên Internet.

```bash
npm run typecheck
npm test
npm run build
```

### Cấu trúc mã

- `src/App.tsx`: dashboard tiếng Việt, điều khiển, lọc lịch sử, biểu đồ Recharts,
  thống kê và xuất CSV cùng một tập dữ liệu đang lọc.
- `src/data/provider.ts`: hợp đồng `DataProvider`, nguồn mô phỏng 1 Hz mỗi cảm biến,
  giới hạn 600 lượt mẫu/cảm biến, ngưỡng/hysteresis và hàm thống kê/CSV.
- `src/data/provider.test.ts`: kiểm thử phân biệt AI/ngưỡng, hysteresis, mất dữ liệu,
  CSV và giới hạn bộ nhớ.
- `src/style.css`: bố cục laptop/điện thoại; `src/main.tsx`: điểm vào React.
- `vite.config.ts`, `tsconfig.json`: cấu hình Vite và TypeScript strict.

Mô phỏng ban đầu tạm dừng, chưa sinh mẫu. Đổi kịch bản/số cảm biến sẽ xóa lịch sử
và tạm dừng. Tăng nhanh 30→40°C trong 20 giây; tăng chậm 30→40°C trong 300 giây.
Kịch bản mất dữ liệu ngắt SHT_02 từ giây thứ 10; mẫu thiếu có giá trị null và biểu đồ
không nối qua khoảng thiếu. Tạm dừng giữ lịch sử và ghi rõ trạng thái.
Thống kê chỉ dùng mẫu hợp lệ. CSV giữ mẫu thiếu, timestamp ISO, source_ids,
node/zone, valid/stale, cảnh báo và `simulated=true`. Dữ liệu chỉ nằm trong RAM;
tải lại trang sẽ mất dữ liệu. Không coi mỗi mẫu vượt ngưỡng là một sự kiện mới.

AI chỉ là nhãn kịch bản mô phỏng (tăng nhanh từ giây 4), không chạy model hay đưa
ra xác suất giả. Ngưỡng bật >50°C / >70%RH, giữ đến ≤49°C / ≤68%RH.
Dự báo và chatbot **Chưa tích hợp**.

### Thay bằng dữ liệu Pi sau này

Viết adapter thực hiện `DataProvider`, truyền vào `<App provider={adapter} />`.
Adapter nhận REST/WebSocket từ backend Pi, chuẩn hóa record theo `docs/PROTOCOL.md`.
Frontend hiện chỉ xử lý mẫu riêng (`aggregation=none`); cần mở rộng kiểu dữ liệu và
UI trước khi nhận bản ghi gộp, giữ source_ids, valid/stale và nguồn timestamp.
Không gán timestamp nhận thành timestamp đo nếu chưa biết thời gian đo.
Broker MQTT/cổng 1883 thuộc ESP32↔Pi; trình duyệt không kết nối MQTT trực tiếp.
Chưa có backend/API, kết nối phần cứng, key AI hoặc lời gọi dịch vụ bên ngoài.

---

# Tài liệu thiết kế ban đầu (giữ lại để tham khảo)

Web nên phục vụ từ Pi 4 trong LAN. Điện thoại/laptop xem qua trình duyệt, không cần đẩy toàn bộ dữ liệu lên cloud.
Đề xuất triển khai tiếp: FastAPI + REST/WebSocket, React + TypeScript + thư viện biểu đồ. Đây là lựa chọn thiết kế, chưa có bản triển khai.

## Các màn hình

| Màn hình | Chức năng và dữ liệu |
|---|---|
| Tổng quan | Thẻ nhiệt/ẩm từng ID hoặc zone gộp; nguồn gộp; tuổi dữ liệu; sensor lỗi; AI disabled/warming/ready; MQTT online/offline |
| Lịch sử | Chọn node/sensor/zone/thời gian, zoom, T và %RH trên trục riêng, min/max/trung bình; không nối đường qua khoảng mất dữ liệu |
| Sự kiện | Vùng vượt 50°C, vùng vượt 70%RH, vùng AI bất thường, vùng lỗi; cho phép chọn khoảng và ghi chú nhãn thủ công |
| Xuất CSV | Giữ timestamp, ID, nguồn, trạng thái valid, loại raw/gộp; chọn khoảng thời gian; không suy diễn dữ liệu chưa nhận |
| Thử nghiệm | Số byte/messages gửi, tỷ lệ gộp, RTT và processing time, missing/duplicate, energy đo thật, bảng so sánh chính sách |
| Dự báo | Đường dự báo tách khỏi lịch sử, khoảng dự báo và MAE/RMSE trên dữ liệu giữ lại; hiện “chưa đủ dữ liệu” nếu không đủ |
| Chatbot | Hỏi dữ liệu theo khoảng thời gian, công bố nguồn và truy vấn, không điều khiển LED/ngưỡng |

Vùng AI không đồng nhất vùng vượt ngưỡng. Dữ liệu gộp không được hiển thị thành hai đường raw giả.
Khi mất kết nối, đóng băng số cuối kèm nhãn dữ liệu cũ; không giữ trạng thái xanh bình thường.
Ghi chú thủ công không ghi đè đầu ra model; lưu annotation riêng để có thể dùng làm nhãn train sau.

## API dự kiến

| Method/path | Mục đích |
|---|---|
| GET /api/nodes | Trạng thái các node |
| GET /api/readings?node_id=&zone_id=&from=&to= | Lịch sử có phân trang/downsample |
| GET /api/events?from=&to= | Lỗi và cảnh báo |
| GET /api/statistics?from=&to= | Thống kê theo nguồn và loại dữ liệu |
| GET /api/export.csv?from=&to= | CSV đúng phần lọc |
| POST /api/annotations | Ghi chú đoạn đã chọn, có xác thực |
| GET /api/forecast?zone_id=&horizon_s= | Dự báo đã kiểm chứng hoặc trạng thái unavailable |
| POST /api/chat | Hỏi đáp, giới hạn tần suất và chi phí |
| WS /api/live | Đẩy dữ liệu và trạng thái mới |

## Chatbot API

Pi đọc DB → chuẩn bị công cụ `get_statistics`, `get_events`, `get_latest_readings` → gửi dữ liệu cần thiết đến nhà cung cấp LLM.
LLM chỉ được truy vấn qua hàm có tham số kiểm tra; không chạy SQL hoặc shell do LLM sinh trực tiếp.
API key nằm trong biến môi trường backend; chọn provider/model khi nhóm quyết định, không gắn cứng trong thiết kế.
Câu trả lời kèm khoảng thời gian, node, nguồn và chỉ số đã truy vấn. Nếu dữ liệu thiếu thì nói thiếu. Không tự suy đoán nguyên nhân như cháy/rò rỉ chỉ từ nhiệt và ẩm.
Giới hạn lượt gọi/token, timeout, và thông báo lỗi API để phần giám sát vẫn chạy bình thường khi chatbot lỗi.

## Lưu trữ khi mở rộng

`raw_samples`, `aggregated_readings`, `events`, `offload_requests`, `offload_results`, `annotations`, `model_versions` là các bảng mục tiêu.
Bản starter chỉ có `messages` giữ payload nguyên gốc, đủ kiểm tra luồng trước khi xây schema/UI lớn.
Muốn thống kê số sự kiện phải nhóm lần bắt đầu–kết thúc và chống đếm lặp; không đếm mỗi mẫu vượt ngưỡng thành một sự kiện mới.
