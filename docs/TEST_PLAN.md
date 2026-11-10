# Kế hoạch kiểm thử và số liệu bảo vệ

## Trên mạch — cần nhóm thực hiện

| Test | Thực hiện | Kỳ vọng |
|---|---|---|
| ID/bus | Làm thay đổi từng cảm biến | Đúng ID đổi, không đọc nhầm |
| 3 sensor | 0x44/0x45 bus0, 0x44 bus1 | Cả ba đọc độc lập |
| Lịch 1Hz | Ghi timestamp mỗi sensor ≥5 phút | Mỗi con ~1s/mẫu, jitter/missed slots được thống kê |
| Single-shot | Đo dòng/phân tích bus | Chỉ một con chuyển đổi mỗi lượt; không tuyên bố cắt nguồn |
| Ngưỡng | Bơm giá trị kiểm thử 50/50.1°C, 70/70.1%RH | Chỉ bật khi lớn hơn; giữ hysteresis đúng |
| Lỗi sensor | Rút một SHT30 | LED lỗi sáng, cảm biến kia vẫn đọc; MQTT valid=false |
| CRC | Mô phỏng byte hỏng | Bỏ mẫu, không đưa vào cửa sổ |
| AI mismatch | Model7Hz, firmware1Hz | model_rate_mismatch, không suy luận sai |
| AI cửa sổ riêng | Cho hai nguồn diễn biến khác | Không nối mẫu hai sensor thành một chuỗi |
| AI quan trọng | Tăng nhanh 30→40°C; 55°C ổn định; tăng chậm | Báo kết quả test model, không giả định nhãn đúng |
| Gộp | Cùng zone, bình thường gần nhau, model ready | 1 record mean giữ cả source_ids |
| Không gộp | Khác zone/có alarm/lệch RH/lỗi/AI chưa sẵn sàng | Records riêng, cảnh báo không mất |
| MQTT | Broker tắt trong khi vượt ngưỡng | Local tiếp tục; mạng không chặn đo |
| Queue đầy | Đổi nhiều trạng thái khi offline | event_drops tăng, không treo |
| Offload | Bật policy2, gateway hoạt động | Đúng request_id/sensor, RTT và kết quả |
| Timeout | Tắt worker Pi | 5 giây hết hạn, local tiếp tục |
| CSV | So payload với CSV | Giá trị/ID/gộp đúng, không tự tạo raw |
| Demo | Chạy liên tục ≥30 phút | Không reset, theo dõi RAM, lỗi, drops |

Không cần hơ cảm biến trực tiếp đến nhiệt độ cao để kiểm tra logic: có thể dùng test injection riêng với nhãn TEST. Không ghi dữ liệu mô phỏng vào dữ liệu thí nghiệm thật.

## Chỉ số cần đo

- Sampling: trung bình/p95/max khoảng cách mẫu, số mẫu lỗi và missedSlots từng nguồn.
- AI: precision/recall, confusion matrix, false alarms/giờ, thời gian phát hiện so với sự kiện được gán nhãn độc lập.
- MQTT: byte/message ứng dụng, messages/phút, mất/loại trùng, reconnect. Khi tính băng thông Wi-Fi thật cần tính cả overhead/reconnect.
- Offload: RTT p50/p95, processing time, timeout rate; tách khỏi thời gian cảnh báo local.
- Năng lượng: Joule/lượt đo hoặc Joule/kịch bản cố định, dòng trung bình và đỉnh; E≈Σ(V×I×Δt).
- So sánh công bằng: cùng dữ liệu/kịch bản, cùng thời gian, báo số lần lặp và độ biến động. Không công bố tiết kiệm trước khi đo.

Ba chính sách mục tiêu: local-only, offload-only cho tác vụ nghiên cứu, adaptive. Luôn giữ lớp ngưỡng an toàn local.
Firmware hiện chưa có chế độ offload-only thuần hoặc bộ tối ưu cost; không dùng nó để khẳng định hoàn thành cả ba chiến lược.
Đánh giá lợi ích model mạnh phải có nhãn ground truth, không dùng chính output model để chấm model.
