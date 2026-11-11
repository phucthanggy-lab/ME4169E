# Motor được chọn và cấu hình firmware

Nguồn: [sản phẩm DC 385, mã kde3852 của Điện tử Khải Định trên Shopee](https://shopee.vn/product/49119859/1706976806).

Thông tin đọc được từ tên sản phẩm: điện áp 6–24 V, tốc độ ghi 2700–10800 vòng/phút, dòng ghi 0,1 A và có encoder Hall. Trang đọc được chưa cung cấp CPR/PPR, số kênh encoder hoặc sơ đồ chân. Không sử dụng 0,1 A làm dòng kẹt trục hoặc ngưỡng bảo vệ.

Người dùng xác nhận nguồn motor **12 V**, driver **L298** và clock ATmega8 **8 MHz**. Firmware chọn PWM **10 kHz**, RUN chiều thuận, duty yêu cầu 0–100%, ramp khoảng 0,5 giây; đây là lựa chọn của chương trình. Chiều quay cơ khí và RPM không được suy ra từ tên sản phẩm.

## Đấu L298 cầu A

| ATmega8 DIP-28 / nguồn | Đấu tới |
|---|---|
| PB0, chân 14 | L298 IN1 |
| PB2, chân 16 | L298 IN2 |
| PB1/OC1A, chân 15 | L298 ENA; bỏ jumper nối ENA lên 5 V nếu dùng module |
| 12 V | Nguồn công suất VS của driver |
| Nguồn logic 5 V | VSS của L298 và nguồn logic ATmega8 |
| GND | Chung GND MCU, driver, nguồn và encoder |
| L298 OUT1/OUT2 | Hai dây motor |

Thêm điện trở **10 kΩ từ ENA xuống GND**, để ENA không thả nổi lúc MCU reset/nạp. Kéo xuống IN1/IN2 nếu module chưa có. Với L298 rời cần hoàn thiện diode hồi dòng, chân sense và tụ nguồn theo mạch ứng dụng trong [datasheet ST L298](https://www.st.com/resource/en/datasheet/l298.pdf). Cách đấu chân nguồn 5 V của module phụ thuộc jumper ổn áp trên chính module; không ghép hai ngõ ra nguồn 5 V với nhau.

## Đấu encoder và hiệu chuẩn

Nguồn motor 12 V không xác định nguồn encoder. Xác định VCC/GND và mức điện áp OUT của encoder trước khi nối; không đưa tín hiệu 12 V vào PD2/PD3. Không gán màu dây theo các motor trông giống nhau.

- Encoder A/B: A → PD2/INT0, B → PD3/INT1; dùng `Firmware/build/release/firmware.hex`, giải mã x4.
- Hall chỉ một OUT: OUT → PD2/INT0, PD3 không dùng; dùng `Firmware/build/release-hall/firmware.hex`, đếm cạnh lên x1. Chế độ này chỉ đo độ lớn tốc độ, không đo chiều quay.

Khi CPR = 0, LCD hiện `C:` là số đếm. Tắt nguồn công suất motor, giữ nguồn logic MCU/encoder, nhấn rồi thả STOP để đặt hiển thị C về 0. Đánh dấu trục, quay tay đúng 10 vòng cùng chiều và ghi N count; CPR hiệu lực là N/10. Thử lại vài lượt. Với A/B, chọn chiều làm C tăng; nếu cần đổi dấu, build với `-EncoderReverse`. Mỗi chế độ x1/x4 phải hiệu chuẩn riêng.

Ví dụ **minh họa**, đo 400 count sau 10 vòng → CPR = 40. Build:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Firmware\build.ps1 -EncoderCountsPerRev 40
# Nếu là Hall một kênh, thêm -EncoderMode HallRising, với CPR đo trong chính chế độ đó.
```

Không dùng số 40 nếu chưa đo. Khi CPR hợp lệ, LCD chuyển sang RPM. Nạp HEX tương ứng bằng programmer đã chọn và verify; firmware không ghi fuse hoặc tự thay clock phần cứng.
