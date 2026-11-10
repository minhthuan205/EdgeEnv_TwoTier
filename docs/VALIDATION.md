# Kiểm tra bản mã ngày 10/11/2026

## Đã chạy và đạt

- `g++ -std=c++17 -Wall -Wextra -Werror tests/test_logic.cpp`: biên dịch sạch logic C++ độc lập, không phải toàn bộ firmware Arduino.
- Chạy binary logic: PASS ngưỡng đúng dấu >, hysteresis, millis wraparound, dữ liệu cũ, điều kiện gộp nhiệt/ẩm, CRC vector 0xBEEF→0x92, ring buffer và tách nguồn.
- Python unittest: 6/6 đạt: tăng nhanh dưới ngưỡng so với tăng chậm; nóng ổn định; timestamp trùng/NaN; lưu chống trùng và CSV dữ liệu gộp; đúng request_id/sensor trong offload; topic không khớp node bị từ chối.
- Kiểm tra cú pháp Python cho gateway và tests.

## Chưa kiểm chứng

- Build firmware với Arduino-ESP32, ArduinoJson, PubSubClient, Edge Impulse thực tế.
- Đấu nối/chân LED, địa chỉ cảm biến, điện áp pull-up, driver I²C trên board thật.
- Wi-Fi/Mosquitto/Pi end-to-end, thời gian suy luận và jitter lấy mẫu.
- Suy luận model hiện tại hoặc model mới, độ chính xác/độ trễ cảnh báo.
- Tiết kiệm năng lượng, ổn định 30 phút, so sánh local/offload/adaptive.
- Web/chatbot: chỉ là thiết kế và cấu trúc dự kiến trong bản giao này.

Không có số liệu hiệu năng, độ chính xác hoặc năng lượng được giả lập để trình bày như kết quả thực nghiệm.
