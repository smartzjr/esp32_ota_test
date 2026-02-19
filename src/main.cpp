#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>

// WiFi配置
const char* ssid = "esp32";
const char* password = "44446666";

// 云端服务器配置 - 使用您提供的GitHub链接
const char* server_host = "raw.githubusercontent.com";
const int server_port = 443;  // HTTPS端口

// 固件路径 - 根据您提供的链接
const char* firmware_path = "/smartzjr/esp32_ota_test/refs/heads/main/firmware.bin";

// LED引脚
#define LED_PIN 8

// 函数声明
void downloadFirmware();

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  Serial.begin(115200);
  
  WiFi.begin(ssid, password);
  Serial.println("连接WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\n✅ WiFi连接成功！");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  digitalWrite(LED_PIN, HIGH);
}

void loop() {
  static unsigned long last_time = 0;
  
  if (millis() - last_time > 5000) {
    last_time = millis();
    Serial.println("运行中... 输入'update'开始云端OTA更新");
  }
  
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "update") {
      Serial.println("开始云端OTA更新...");
      downloadFirmware();
    }
  }
  
  digitalWrite(LED_PIN, LOW);
  delay(200);
  digitalWrite(LED_PIN, HIGH);
  delay(200);
}

void downloadFirmware() {
  WiFiClientSecure client;
  HTTPClient http;
  
  String url = String("https://") + server_host + firmware_path;
  Serial.print("📦 正在从云端下载固件: ");
  Serial.println(url);
  
  // 设置SSL参数
  client.setInsecure(); // 跳过证书验证，节省资源
  client.setTimeout(60000); // 60秒超时
  
  http.begin(client, url);
  http.setTimeout(60000); // 60秒超时
  
  // 设置请求头
  http.addHeader("User-Agent", "ESP32-C3-OTA/1.0");
  http.addHeader("Accept", "application/octet-stream");
  http.addHeader("Connection", "close");
  http.addHeader("Cache-Control", "no-cache");
  
  // 发送GET请求
  int httpResponseCode = http.GET();
  
  if (httpResponseCode == 200) { // HTTP OK
    Serial.println("✅ HTTPS请求成功");
    
    // 获取内容长度
    int contentLength = http.getSize();
    if (contentLength <= 0) {
      contentLength = 1048576; // 1MB默认
      Serial.println("⚠️  未找到Content-Length，使用1MB限制");
    } else {
      Serial.print("📏 固件大小: ");
      Serial.print(contentLength);
      Serial.println(" 字节");
    }
    
    // 检查可用Flash空间
    size_t flashSize = ESP.getFreeSketchSpace();
    Serial.printf("💾 可用Flash空间: %d 字节\n", flashSize);
    
    if (contentLength > flashSize) {
      Serial.printf("❌ 固件过大: %d 字节 > %d 字节可用空间\n", contentLength, flashSize);
      Serial.println("💡 请减小固件大小或检查分区表设置");
      http.end();
      return;
    }
    
    // 开始固件更新
    if (Update.begin(contentLength)) {
      Serial.println("🚀 开始写入固件...");
      
      // 获取响应流
      WiFiClient& stream = http.getStream();
      
      // 下载并写入固件
      size_t written = 0;
      size_t total = 0;
      uint8_t buffer[512]; // 减小缓冲区以节省内存
      unsigned long startTime = millis();
      unsigned long lastProgressTime = millis(); // 记录上次进度更新时间
      
      while (http.connected() && (millis() - startTime) < 600000) { // 10分钟超时
        size_t available = stream.available();
        
        if (available > 0) {
          size_t readBytes = stream.readBytes(buffer, min(available, sizeof(buffer)));
          
          if (readBytes > 0) {
            if (Update.write(buffer, readBytes) == readBytes) {
              written += readBytes;
              total += readBytes;
              
              // 每20KB显示进度，或每30秒显示一次（以防进度停滞）
              if (written >= 20 * 1024 || (millis() - lastProgressTime) > 30000) {
                float progress = (float)total / contentLength * 100;
                Serial.printf("📈 进度: %d KB / %d KB (%.1f%%)\n", 
                             total / 1024, 
                             contentLength / 1024,
                             progress);
                written = 0;
                lastProgressTime = millis();
              }
            } else {
              Serial.println("❌ 写入失败!");
              Update.abort();
              http.end();
              return;
            }
          }
        }
        
        // 检查是否已下载了所有数据
        if (total >= contentLength) {
          Serial.println("✅ 数据已全部下载完成");
          break; // 已经下载完所有数据
        }
        
        // 检查是否需要显示定期进度（即使没有新数据）
        if ((millis() - lastProgressTime) > 30000) {
          float progress = (float)total / contentLength * 100;
          Serial.printf("⏰ 定期进度更新: %d KB / %d KB (%.1f%%) - 已运行 %d 秒\n", 
                       total / 1024, 
                       contentLength / 1024,
                       progress,
                       (millis() - startTime) / 1000);
          lastProgressTime = millis();
        }
        
        // 给系统一些时间处理网络任务
        delay(1);
      }
      
      Serial.println();
      Serial.print("✅ 下载完成，已写入: ");
      Serial.print(total);
      Serial.print(" / ");
      Serial.println(contentLength);
      
      // 完成更新
      if (Update.end(true)) { // true表示立即应用更新
        Serial.println("🎉 云端OTA更新成功！正在重启设备...");
        http.end();
        delay(1000);
        ESP.restart();
      } else {
        Serial.print("❌ 固件更新失败: ");
        Serial.println(Update.getError());
        Serial.println("💡 可能原因: 固件过大、Flash空间不足或分区表问题");
      }
    } else {
      Serial.print("❌ 无法开始更新: ");
      Serial.println(Update.getError());
      Serial.printf("💡 可能是固件过大 (%d 字节) 或Flash空间不足\n", contentLength);
    }
  } else {
    Serial.print("❌ HTTPS请求失败，状态码: ");
    Serial.println(httpResponseCode);
    if(httpResponseCode == -11) {
      Serial.println("💡 提示: 状态码-11表示连接超时，可能是网络问题或服务器无响应");
    }
  }
  
  http.end();
}