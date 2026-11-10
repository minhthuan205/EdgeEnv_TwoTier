# Triển khai thử trên Raspberry Pi 4

Đề xuất Raspberry Pi OS 64-bit cho Pi 4. Đây là hướng dẫn cài tại thiết bị, chưa thực thi trên Pi của nhóm.
Cho Pi/ESP32 cùng LAN; đặt DHCP reservation cho IP Pi. Cổng gateway MQTT là địa chỉ Pi, không phải địa chỉ máy laptop.

## Mosquitto

```bash
sudo apt update
sudo apt install mosquitto mosquitto-clients python3-venv
sudo mosquitto_passwd -c /etc/mosquitto/passwd esp32
sudo mosquitto_passwd /etc/mosquitto/passwd gateway
```

Hai lệnh passwd hỏi mật khẩu; đặt mật khẩu riêng và sửa config tương ứng. `-c` chỉ dùng lần đầu vì tạo lại file.
Tạo `/etc/mosquitto/conf.d/edgeenv.conf`:

```conf
listener 1883
allow_anonymous false
password_file /etc/mosquitto/passwd
acl_file /etc/mosquitto/edgeenv.acl
```

Tạo `/etc/mosquitto/edgeenv.acl`:

```conf
user esp32
topic write edgeenv/v1/NODE_01/telemetry
topic write edgeenv/v1/NODE_01/events
topic write edgeenv/v1/NODE_01/status
topic write edgeenv/v1/NODE_01/offload/request
topic read edgeenv/v1/NODE_01/offload/response

user gateway
topic read edgeenv/v1/+/telemetry
topic read edgeenv/v1/+/events
topic read edgeenv/v1/+/status
topic read edgeenv/v1/+/offload/request
topic write edgeenv/v1/+/offload/response
```

```bash
sudo chown root:mosquitto /etc/mosquitto/passwd /etc/mosquitto/edgeenv.acl
sudo chmod 640 /etc/mosquitto/passwd /etc/mosquitto/edgeenv.acl
sudo systemctl restart mosquitto
sudo systemctl status mosquitto
```

Đây là cấu hình LAN thử nghiệm, chưa có TLS. Không mở cổng 1883 ra Internet. Nếu cần truy cập từ xa, thiết kế riêng TLS/VPN và xác thực web.
Gateway hiện subscribe telemetry/events/offload, chưa lưu status; có thể dùng mosquitto_sub để xem status khi demo.

## Gateway

Làm các lệnh trong README. Chạy terminal trước để thấy lỗi, sau khi ổn mới tạo service systemd dưới user riêng, đúng WorkingDirectory và biến môi trường.
Không đưa API key chatbot vào firmware hay frontend. Chatbot chưa được tích hợp vào starter.
SQLite WAL thích hợp bước đầu; cần backup dữ liệu và retention khi chạy dài ngày, chưa có tự động xóa dữ liệu trong bản này.

## Mốc tích hợp

1. Serial đọc đủ ID trước khi bật Wi-Fi.
2. Broker nhận telemetry và events.
3. Gateway tạo database và CSV có số liệu thật.
4. Thử offload chế độ 2, thấy RTT và đúng request_id.
5. Ngắt mạng: LED/local vẫn hoạt động. Nối lại: telemetry mới xuất hiện; không giả định khôi phục mọi dữ liệu đã mất.
6. Chạy ổn ≥30 phút và đo các chỉ số trong TEST_PLAN.
