# Motor đã mua: GA25-370, 12 V, 280 RPM

Người dùng xác nhận đã mua bản **280 RPM** tại [Shopee — shop Điện tử Đức Huy](https://shopee.vn/product/1292561517/43151505475). Ảnh bìa dùng chung cho nhiều tốc độ, không dùng nhãn trong ảnh bìa để thay đổi biến thể người dùng đã mua.

[Trang sản phẩm của cùng shop](https://dientuduchuy.com/products/jga25-370-12v-dong-co-dc-giam-toc-12vdc-60rpm-co-encoder-hoi-tiep-toc-do-vi-tri) công bố motor 12 V, encoder A/B cấp 3,3–5 V, đĩa encoder 11 xung và tỉ số truyền khoảng 21,3:1 cho bản 280 RPM. Đây là thông số tham khảo của người bán, chưa phải kết quả đo trên motor đã nhận. Trang chưa xác nhận dòng kẹt trục của đúng biến thể này.

## Cấu hình firmware

- ATmega8, clock thực cần khớp **8 MHz**; không ghi fuse tự động.
- Nguồn motor **12 V**, nguồn encoder **5 V** từ Header3.
- Encoder A/B: PD2/INT0 và PD3/INT1, giải mã **x4**; chọn `Firmware/build/release/firmware.hex`.
- L298 **cầu B**, motor ở **OUT3/OUT4**, theo cách đấu người dùng đã chọn.
- PWM vẫn là **Timer1 OC1A/PB1**, 10 kHz. Kênh A của Timer1 độc lập với tên cầu B của L298; không đổi sang OC1B/PB2 vì PB2 đang dùng chọn chiều.
- Chiết áp đặt duty 0–100%; ramp khoảng 0,5 giây. RUN chạy thuận theo quy ước dây, STOP ưu tiên; không có PID hay tự bù tải.
- **280 RPM là tốc độ danh định không tải**, không phải giá trị chương trình tự ép lên LCD. Tốc độ thực phụ thuộc tải, nguồn, sụt áp driver và duty.

## Nối sáu dây motor với PCB và L298

Số 1–6 dưới đây theo hình phân chân encoder của shop: đối chiếu đúng mặt nhìn của giắc/board motor trước khi nối. Không suy ra pin chỉ từ màu dây hoặc từ phía nhìn ngược của giắc.

| Chân theo hình shop | Chức năng | Nối đến |
|---|---|---|
| 1 | M1, phần động cơ | L298 OUT3 |
| 2 | GND encoder | Header3 chân 2, GND chung |
| 3 | Encoder A | Header4 chân 1 → PD2/INT0, DIP 4 |
| 4 | Encoder B | Header4 chân 2 → PD3/INT1, DIP 5 |
| 5 | VCC encoder | Header3 chân 1, +5 V |
| 6 | M2, phần động cơ | L298 OUT4 |

Header4 chỉ có A/B. R7/R8 kéo tín hiệu lên +5 V qua 10 kΩ, không thay thế cặp VCC/GND cấp điện cho encoder.

| PCB / nguồn | L298 cầu B |
|---|---|
| Header5 chân 1, PB1/OC1A, DIP 15 | ENB; tháo jumper ENB nối lên 5 V |
| Header5 chân 2 | GND driver |
| Header6 chân 1, PB0, DIP 14 | IN3 |
| Header6 chân 2, PB2, DIP 16 | IN4 |
| Header2 +12 V/GND | VS/GND, khi đường đồng/đầu nối chịu đủ dòng motor; có thể chia nguồn trực tiếp từ adapter |

Thêm kéo xuống **ENB 10 kΩ** để cầu tắt khi MCU reset/nạp. IN3/IN4 cần có mức xác định khi reset. GND MCU, encoder, driver và nguồn nối chung. Cách cấp logic 5 V của module phụ thuộc jumper ổn áp; không nối song song đầu ra ổn áp trên module với đầu ra 5 V PCB. Với L298 rời, cần diode hồi dòng, chân sense và tụ nguồn theo [datasheet ST](https://www.st.com/resource/en/datasheet/l298.pdf).

Các GPIO MCU giống bản dùng cầu A trước đây. Dùng cầu B chỉ đổi đầu nối bên L298 từ ENA/IN1/IN2/OUT1/OUT2 sang ENB/IN3/IN4/OUT3/OUT4; không có thanh ghi ATmega8 nào chọn cầu A/B của module ngoài.

## CPR và hiển thị RPM

Release mặc định dùng `CFG_ENCODER_PPR=11` và tỉ số `213/10` của shop để tính CPR danh nghĩa. Với 11 chu kỳ trên một kênh/một vòng trục motor, x4 cho 44 count/vòng trục motor; hộp số danh nghĩa 21,3:1 cho **937,2 count/vòng trục ra**. Code giữ chính xác hệ số danh nghĩa dưới dạng `9372/10`, không làm tròn CPR về 937. Đây chưa phải CPR được đo trên motor đã nhận.

HEX `Firmware/build/release/firmware.hex` hiện **`RPM~:`**, đơn vị vòng/phút, tính từ xung encoder thu được trong từng cửa sổ khoảng 250 ms. Dấu `~` cho biết CPR lấy từ thông số danh nghĩa. Không gán cố định 280 lên LCD: tốc độ hiển thị thay đổi theo chuyển động, kể cả quay trớn sau STOP; không có xung thì về 0. Duty 50% không đảm bảo tốc độ bằng một nửa 280 RPM.

Khi cần đối chiếu CPR, build riêng chế độ đếm rồi nạp HEX tương ứng:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Firmware\build.ps1 -EncoderCountsPerRev 0 -OutputDirectory Firmware/build/release-counts
```

LCD của bản này hiện `C:`. Đo tổng N count sau R vòng trục ra và dùng C=N/R; không suy ra CPR bằng cách giả định motor luôn quay 280 RPM. Quy trình:

1. Kiểm tra nguồn/đấu dây, giữ motor dừng. Nhấn rồi thả STOP để C về 0.
2. Nếu hộp số quay tay nhẹ được, ngắt nguồn motor, giữ logic/encoder, quay đúng số vòng cùng chiều và ghi count. Không cưỡng bức hộp số khó quay. Có thể dùng đánh dấu trục/video để ghi count tại cùng vị trí sau nhiều vòng khi chạy chậm; tránh cộng thêm vòng quay trớn ngoài khoảng đo.
3. Lặp lại để kiểm tra. Nếu C giảm theo chiều đang chọn, kiểm tra A/B và quy ước chiều; tùy chọn `-EncoderReverse` đổi dấu mà không đổi chân.
4. Tham số build hiện nhận CPR nguyên. Nếu N/R có phần lẻ, tăng số vòng đo để đánh giá sai số; có thể làm tròn về CPR nguyên gần nhất và ghi rõ sai số, không gọi đó là tỉ số truyền chính xác.

Ví dụ **chỉ khi phép đo thực cho C=937**:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Firmware\build.ps1 -EncoderCountsPerRev 937
```

Số 937 trong lệnh không phải giá trị đã xác nhận cho motor của người dùng. Tham số CPR nguyên khác 0 thay hệ số danh nghĩa và chuyển nhãn thành `RPM:`. Không truyền tham số CPR thì Release trở lại hệ số danh nghĩa và nhãn `RPM~:`. Mỗi lần đổi cấu hình phải build và nạp lại HEX; kiểm tra manifest để chọn đúng file.

`release-hall` vẫn được giữ cho thử nghiệm chỉ một kênh A/cạnh lên x1: CPR danh nghĩa **234,3 = 2343/10**, không dùng 937,2 của x4. Bỏ ngõ B và hiệu chuẩn riêng. Với motor đã mua có A/B, dùng bản `release` x4.

## Phạm vi đã kiểm tra

Phần mềm được kiểm tra bằng AVR-GCC và thực thi instruction AVR với mô hình thanh ghi. Thử 4.374 count/s trong lúc RUN cho LCD khoảng 280 vòng/phút với CPR danh nghĩa 937,2; khi hết xung, màn hình về 0. Có thêm các bài thử 12.000 count/s, CPR phân số và chế độ đếm. Đây là kiểm tra bằng tín hiệu mô hình, không phải phép đo tốc độ, clock, dòng hoặc nhiễu trên bo thật. Chưa nạp hoặc thử motor thực.
