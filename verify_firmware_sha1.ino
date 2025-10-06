#include <WiFi.h>              // Thư viện kết nối WiFi cho ESP32
#include <WiFiClientSecure.h>  // Thư viện hỗ trợ kết nối HTTPS (bảo mật SSL/TLS)
#include <HTTPClient.h>        // Thư viện thực hiện HTTP requests (GET, POST,...)
#include "mbedtls/sha1.h"      // Thư viện mã hóa SHA-1 để tạo hash từ dữ liệu

// ===== CẤU HÌNH WIFI VÀ URL =====
const char* ssid = "LAB TRUYEN THONG DPT";      // Tên WiFi cần kết nối
const char* password = "khoacntt2@25";           // Mật khẩu WiFi
// URL trỏ tới file firmware cần tải về và kiểm tra
// const char* fileUrl = "https://raw.githubusercontent.com/phohoccode/bao-mat-iot/refs/heads/main/bat_tat_led.ino.bin";
const char* fileUrl = "https://drive.google.com/uc?export=download&id=1WaJhIrg1JHdG4lOj9atHmHERjGRnfIzq";

void setup() {
  // ===== KHỞI TẠO SERIAL MONITOR =====
  Serial.begin(115200);  // Bắt đầu giao tiếp Serial với tốc độ 115200 baud
  delay(1000);           // Đợi 1 giây để Serial Monitor ổn định

  // ===== KẾT NỐI WIFI =====
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);  // Bắt đầu kết nối WiFi với SSID và password

  // Vòng lặp thử kết nối WiFi (tối đa 20 lần = 10 giây)
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);        // Đợi 0.5 giây mỗi lần thử
    Serial.print("."); // In dấu chấm để hiển thị tiến trình
    attempts++;        // Tăng số lần thử
  }

  // Kiểm tra nếu không kết nối được WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nFailed to connect to WiFi!");
    return;  // Dừng chương trình nếu không kết nối được
  }

  // Thông báo kết nối thành công và hiển thị địa chỉ IP
  Serial.println("\nWiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());  // In địa chỉ IP của ESP32

  // ===== CẤU HÌNH HTTPS CLIENT =====
  WiFiClientSecure client;
  client.setInsecure();  // Bỏ qua xác thực SSL certificate 
                         // (Chỉ dùng để test, không an toàn cho production!)

  // ===== KHỞI TẠO HTTP CLIENT VÀ THIẾT LẬP REQUEST =====
  HTTPClient http;
  http.begin(client, fileUrl);  // Bắt đầu kết nối HTTPS với URL đã cho
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);  // Tự động follow redirects (301, 302)

  // ===== GỬI HTTP GET REQUEST =====
  Serial.println("Downloading file...");
  int httpCode = http.GET();  // Gửi GET request và nhận mã phản hồi

  Serial.printf("HTTP Response code: %d\n", httpCode);  // In mã phản hồi HTTP
  // Mã 200 = OK, 404 = Not Found, 301/302 = Redirect, v.v.

  // ===== XỬ LÝ NẾU TẢI FILE THÀNH CÔNG =====
  if (httpCode == HTTP_CODE_OK) {  // Kiểm tra nếu mã phản hồi là 200 (OK)
    
    // Lấy kích thước file từ Content-Length header
    int contentLength = http.getSize();
    Serial.printf("Content-Length: %d bytes\n", contentLength);

    // Lấy stream để đọc dữ liệu từ server
    WiFiClient* stream = http.getStreamPtr();

    // ===== KHỞI TẠO SHA-1 CONTEXT =====
    mbedtls_sha1_context sha1_ctx;  // Tạo context cho SHA-1
    mbedtls_sha1_init(&sha1_ctx);   // Khởi tạo context
    mbedtls_sha1_starts(&sha1_ctx); // Bắt đầu tính toán SHA-1

    // ===== CHUẨN BỊ BUFFER ĐỂ ĐỌC DỮ LIỆU =====
    const int bufSize = 512;   // Kích thước buffer = 512 bytes
    uint8_t buffer[bufSize];   // Mảng buffer để lưu dữ liệu tạm
    size_t totalLen = 0;       // Biến đếm tổng số bytes đã đọc

    // ===== VÒNG LẶP ĐỌC VÀ XỬ LÝ DỮ LIỆU =====
    // Tiếp tục đọc cho đến khi:
    // - Mất kết nối HTTP, HOẶC
    // - Đã đọc đủ contentLength bytes (nếu contentLength > 0)
    while (http.connected() && (totalLen < contentLength || contentLength == -1)) {
      
      size_t available = stream->available();  // Kiểm tra có bao nhiêu bytes sẵn sàng đọc
      
      if (available) {  // Nếu có dữ liệu
        // Đọc dữ liệu vào buffer (đọc tối đa bufSize hoặc số bytes available)
        int len = stream->readBytes(buffer, min((size_t)bufSize, available));

        // ===== DEBUG: IN 1000 BYTES ĐẦU TIÊN =====
        // Giúp kiểm tra xem dữ liệu tải về có đúng không
        if (totalLen < 1000) {
          for (int i = 0; i < len && totalLen + i < 1000; i++) {
            char c = buffer[i];
            // In ký tự nếu là ký tự in được, ngược lại in dấu chấm
            if (isPrintable(c)) Serial.print(c);
            else Serial.print(".");
          }
        }

        // ===== CẬP NHẬT SHA-1 HASH =====
        // Thêm dữ liệu vừa đọc vào quá trình tính SHA-1
        mbedtls_sha1_update(&sha1_ctx, buffer, len);
        totalLen += len;  // Cộng dồn số bytes đã đọc

        // ===== HIỂN THỊ TIẾN TRÌNH TẢI =====
        // Mỗi 5KB (5120 bytes) in ra số bytes đã tải
        if (totalLen % 5120 == 0) {
          Serial.printf("\rDownloaded: %d bytes", totalLen);
        }
      }
      delay(1);  // Delay nhỏ để tránh watchdog timer reset
    }

    // ===== HOÀN TẤT VIỆC ĐỌC FILE =====
    Serial.println("\n=== Done reading file ===");
    Serial.printf("Total bytes read: %d\n", totalLen);  // In tổng số bytes đã đọc

    // ===== HOÀN TẤT TÍNH SHA-1 =====
    unsigned char sha1_result[20];  // Mảng lưu kết quả SHA-1 (20 bytes)
    mbedtls_sha1_finish(&sha1_ctx, sha1_result);  // Kết thúc và lấy kết quả hash
    mbedtls_sha1_free(&sha1_ctx);  // Giải phóng bộ nhớ của context

    // ===== IN SHA-1 CỦA FILE VỪA TẢI =====
    Serial.print("SHA-1 firmware: ");
    for (int i = 0; i < 20; i++) {
      Serial.printf("%02x", sha1_result[i]);  // In mỗi byte dưới dạng hex (2 chữ số)
    }
    Serial.println();

    // ===== ĐỊNH NGHĨA SHA-1 MONG ĐỢI (EXPECTED HASH) =====
    // Hash này được tính trước từ firmware gốc đáng tin cậy
    // Dùng để so sánh xem file tải về có bị thay đổi/hỏng không
    const unsigned char expected_sha1[20] = {
      0x53, 0x42, 0xdc, 0x4b, 0x54, 0x33, 0xa3, 0x8f, 0x66, 0x04,
      0xf9, 0x3d, 0xee, 0x3b, 0xf2, 0x74, 0xf8, 0x94, 0x67, 0x05  // Lưu ý: byte cuối là 0x05 không phải 051
    };

    // ===== IN SHA-1 MONG ĐỢI =====
    Serial.print("SHA-1 expected: ");
    for (int i = 0; i < 20; i++) {
      Serial.printf("%02x", expected_sha1[i]);
    }
    Serial.println();

    // ===== SO SÁNH HAI SHA-1 HASH =====
    bool valid = true;  // Biến cờ kiểm tra tính hợp lệ
    
    // Vòng lặp so sánh từng byte
    for (int i = 0; i < 20; i++) {
      if (sha1_result[i] != expected_sha1[i]) {  // Nếu có byte nào khác nhau
        valid = false;  // Đánh dấu không hợp lệ
        break;          // Dừng vòng lặp ngay
      }
    }

    // ===== THÔNG BÁO KẾT QUẢ =====
    if (valid) {
      Serial.println("✓ Firmware hợp lệ");  // Hash khớp = firmware an toàn
    } else {
      Serial.println("✗ Firmware bị hỏng hoặc hash không khớp");  // Hash khác = có vấn đề
    }

  } else {
    // ===== XỬ LÝ LỖI HTTP =====
    Serial.printf("HTTP error: %d\n", httpCode);  // In mã lỗi
    Serial.println(http.errorToString(httpCode)); // In mô tả lỗi
  }

  // ===== ĐÓNG KẾT NỐI HTTP =====
  http.end();  // Giải phóng tài nguyên HTTP client
}

void loop() {
  // Không làm gì trong loop vì chỉ cần chạy 1 lần trong setup()
}