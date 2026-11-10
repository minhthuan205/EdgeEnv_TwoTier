# Mô hình Edge Impulse

Gói đã kiểm tra: `ei-nhietdo_doam-arduino-1.0.1-impulse-#3.zip`, không sao chép SDK vào gói mới.
Metadata: input 7Hz; 21 mẫu/cửa sổ; 2 trục `temperature + humidity`; hai lớp `binhthuong`, `qua_nhiet`; mô hình int8 phân loại, không có khối anomaly K-means.

## Chuyển sang 1 Hz đúng cách

1. Thu dữ liệu thực 1Hz cho từng sensor, giữ cùng thứ tự trục và đơn vị °C/%RH.
2. Có: bình thường ổn định, tăng chậm, tăng nhanh dưới 50°C, nóng ổn định trên 50°C, độ ẩm cao, nhiễu và mất mẫu.
3. Mất mẫu/lỗi là chất lượng dữ liệu; không tự biến thành nhãn nhiệt tăng nhanh.
4. Tách train/test theo phiên thu, không chia ngẫu nhiên các cửa sổ chồng lấn cùng phiên sang cả hai tập.
5. Chọn Frequency 1Hz trong impulse, thử cửa sổ 10–15 giây và ngắn hơn; điều chỉnh theo độ trễ cần thiết.
6. Train lại, đánh giá confusion matrix, recall tăng nhanh, precision và số lần báo nhầm/giờ.
7. Export Arduino library; cài vào IDE. Bật `ENABLE_EDGE_IMPULSE=1`, `SAMPLE_HZ=1`.
8. Giữ hai lớp hiện tại nếu nhóm chưa mở rộng. Khi thêm nhãn, cập nhật ánh xạ cảnh báo trong `infer()` và tiêu chí gộp. Không mặc định mọi lớp ngoài `binhthuong` đều là nguy hiểm.

Không dùng score lớp phân loại như xác suất đã được hiệu chuẩn nếu chưa kiểm chứng. Ngưỡng 0.8 là khởi đầu thử nghiệm.
Không thể kết luận model cũ bắt tăng nhanh chỉ từ nhãn `qua_nhiet`; phải xem dữ liệu train và test thực.
1Hz phù hợp làm mốc thử cho môi trường nhiệt/ẩm biến thiên theo giây. Nếu cần phát hiện thay đổi dưới 1 giây, thử 2–5Hz và train tương ứng; tăng tần số đọc không tự làm đáp ứng vật lý SHT30 nhanh hơn.
