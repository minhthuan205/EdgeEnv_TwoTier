# Nguồn kỹ thuật

Đối chiếu ngày 10/11/2026. Các tham số chu kỳ/gộp/score trong firmware là lựa chọn thử nghiệm của bản thiết kế, không phải khuyến nghị bắt buộc của nhà sản xuất.

- ESP32 có hai controller I²C: https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32/api-reference/peripherals/i2c.html
- Sensirion SHT3x-DIS datasheet: địa chỉ ADDR, single-shot, thời gian chuyển đổi, CRC và công thức quy đổi: https://sensirion.com/media/documents/213E6A3B/63A5A569/Datasheet_SHT3x_DIS.pdf
- Driver chính chủ Sensirion, command 0x2400: https://github.com/Sensirion/embedded-i2c-sht3x/blob/master/sht3x_i2c.h
- TI TCA9548A, switch I²C: https://www.ti.com/product/TCA9548A
- Edge Impulse impulse design, trục dữ liệu, cửa sổ và frequency: https://docs.edgeimpulse.com/studio/projects/impulse-design
- MQTT PubSubClient source/readme và giới hạn publish QoS0: https://github.com/knolleary/pubsubclient
- Eclipse Paho Python: https://eclipse.dev/paho/files/paho.mqtt.python/html/
- Eclipse Mosquitto config: https://mosquitto.org/man/mosquitto-conf-5.html

Nguồn nội bộ đã đọc: kế hoạch Hướng A và metadata trong ZIP Edge Impulse của nhóm; sơ đồ 2 bus trong ảnh người dùng.
Chưa nhận .ino cũ nên firmware này là bản tham chiếu mới, không phải bản vá đã so sánh hành vi với firmware cũ.
