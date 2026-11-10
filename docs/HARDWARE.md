# Phần cứng và lịch đo

## Hai cảm biến hiện có — giữ nguyên hai bus trong ảnh

| Thiết bị | VCC | GND | SDA | SCL | Địa chỉ | ID |
|---|---|---|---:|---:|---|---|
| SHT30 #1 | 3V3 | GND chung | 21 | 22 | 0x44 | SHT_01 |
| SHT30 #2 | 3V3 | GND chung | 18 | 19 | 0x44 | SHT_02 |
| LED đỏ | GPIO25 qua điện trở 220–330Ω, anode | cathode → GND | — | — | — | cảnh báo chung |

GPIO25 là lựa chọn trong mã mới, cần kiểm tra lại chân thực trên mạch. Nếu dùng LED module active-low, đổi `LED_ACTIVE_HIGH`.
Pull-up I²C về 3.3V; module thường có sẵn, cần kiểm tra. Dùng chung GND, dây ngắn, tránh đưa pull-up 5V vào GPIO ESP32.
Nguồn cảm biến giữ liên tục; lệnh 0x2400 thực hiện single-shot, chờ 16ms rồi đọc 6 byte, kiểm CRC cho nhiệt và ẩm.
Không gọi đo hai cảm biến cùng lúc. Ở thời gian còn lại cảm biến ở idle, heater tắt.

## Ba cảm biến: phương án ưu tiên ít linh kiện

- Bus 0 (21/22): SHT_01 tại 0x44 + SHT_03 tại 0x45.
- Bus 1 (18/19): SHT_02 tại 0x44.
- Đặt ADDR của SHT_03 theo hướng dẫn module để chọn 0x45. Không để ADDR trôi nổi.
- `SENSOR_COUNT=3` trong Config.h. SHT_03 mặc định ZONE_02, hai con đầu ZONE_01; sửa theo vị trí thật.
- Nếu module thứ ba không có cách đổi địa chỉ và cả ba cùng 0x44: dùng TCA9548A hoặc thay một module cho phép ADDR. Firmware hiện tại không có driver TCA; không bật chế độ 3 rồi đấu ba 0x44 lên hai bus.
- ESP32-WROOM trong ảnh có 2 controller I²C; hai địa chỉ là giới hạn **mỗi bus**, không phải cả hệ thống.

## 1 Hz và luân phiên

| Số cảm biến | Thời điểm bắt đầu đo tương đối trong mỗi giây | Tần số từng con |
|---|---|---|
| 2 | #1 tại 0 ms; #2 tại 500 ms | 1 Hz |
| 3 | #1 tại 0 ms; #2 tại ~333 ms; #3 tại ~666 ms | 1 Hz |

Sau khoảng 16ms hoàn thành phép đo, con vừa đo ở idle. Con còn lại không thực hiện chuyển đổi trong thời gian đó.
Đây là trạng thái **không đo**, không phải ngắt điện. Nếu yêu cầu thầy là cắt nguồn thật, cần bổ sung load switch/MOSFET, cách ly đường tín hiệu và thời gian khởi động; mã này chưa điều khiển nguồn.

Không luân phiên toàn bộ các mẫu SHT_01/SHT_02 thành một chuỗi AI: mỗi con một cửa sổ.
Đổi cảm biến hoạt động mỗi vài chục giây sẽ tạo khoảng mù và đòi hỏi chiến lược AI khác.

## Linh kiện cần bổ sung và thứ tự

1. Pi 4, nguồn phù hợp, thẻ nhớ tốt; router/AP hoặc Wi-Fi LAN sẵn có.
2. Nếu muốn 3 điểm đo: thêm SHT30 hỗ trợ ADDR; TCA9548A chỉ cần khi không thể phân địa chỉ trên hai bus hiện có.
3. INA219/thiết bị đo dòng để so sánh năng lượng; phải bố trí đo toàn node, không chỉ cảm biến. INA219 không được tích hợp vào firmware này.
4. Load switch chỉ khi phải ngắt nguồn thật. Không coi tắt kênh TCA là tắt nguồn.
5. Nếu muốn cảnh báo theo từng ID bằng phần cứng, thêm LED từng cảm biến; mã hiện dùng một LED cảnh báo chung, phân biệt ID qua MQTT/Serial.
