#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <base64.h>

// WiFi设置
const char* ssid = "letlmulee";      // 替换为您的WiFi名称
const char* password = "12345678"; // 替换为您的WiFi密码

// 阿里云API设置
const char* host = "dashscope.aliyuncs.com";
const String api_endpoint = "/compatible-mode/v1/chat/completions";
const String api_key = "DASHSCOPE_API_KEY";  // 替换为您的DashScope API Key

// 图像接收变量
#define MAX_IMAGE_SIZE 65536  // 最大图像缓冲区大小 (64KB)
#define START_MARKER 0xAA     // 起始标记
#define END_MARKER 0xBB       // 结束标记
#define SIZE_BYTES 4          // 图像大小占用字节数
#define CHUNK_SIZE 1024       // 分块处理的大小

bool receiving = false;       // 是否正在接收图像
uint32_t imageSize = 0;       // 图像大小
uint32_t bytesReceived = 0;   // 已接收字节数
uint8_t* imageBuffer = NULL;  // 图像缓冲区，动态分配

// 连接变量
WiFiClientSecure client;
unsigned long lastConnectionTime = 0;
const unsigned long reconnectInterval = 30000;  // 重连间隔 (30秒)

// 状态指示
const int LED_PIN = LED_BUILTIN;    // 使用板载LED
bool isProcessing = false;

void setup() {
  // 初始化串口
  Serial.begin(115200);  // 与OpenMV通信的波特率
  Serial.setTimeout(1000);
  
  // 设置LED指示灯
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // ESP8266上LED是低电平点亮
  
  delay(1000);
  Serial.println();
  Serial.println("ESP8266启动中...");
  
  // 连接WiFi
  WiFi.mode(WIFI_STA);
  connectWiFi();
  
  // 设置SSL客户端为不验证模式（简化开发）
  client.setInsecure();
  
  // 指示灯闪烁表示就绪
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, LOW); // 点亮
    delay(100);
    digitalWrite(LED_PIN, HIGH); // 熄灭
    delay(100);
  }
  
  Serial.println("系统就绪，等待图像数据...");
}

void loop() {
  // 检查WiFi连接状态，如果断开则重连
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi连接已断开，尝试重连...");
    connectWiFi();
  }
  
  // 接收图像数据
  receiveImage();
  
  // 处理状态LED指示
  if (isProcessing) {
    // 处理中LED快速闪烁
    if (millis() % 500 < 250) {
      digitalWrite(LED_PIN, LOW); // 点亮
    } else {
      digitalWrite(LED_PIN, HIGH); // 熄灭
    }
  } else {
    // 空闲状态LED缓慢闪烁
    if (millis() % 2000 < 100) {
      digitalWrite(LED_PIN, LOW); // 点亮
    } else {
      digitalWrite(LED_PIN, HIGH); // 熄灭
    }
  }
  
  // 检查是否有来自云服务的响应
  while (client.available()) {
    String line = client.readStringUntil('\n');
    // 转发响应到串口，供Python脚本接收
    Serial.println(line);
  }
}

// 连接WiFi
void connectWiFi() {
  Serial.print("连接到WiFi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  // 等待连接，最多尝试20次
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    delay(500);
    Serial.print(".");
    attempt++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi已连接");
    Serial.print("IP地址: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("");
    Serial.println("WiFi连接失败，请检查网络设置");
  }
}

// 接收图像数据
void receiveImage() {
  while (Serial.available() > 0) {
    if (!receiving) {
      // 寻找起始标记
      uint8_t inByte = Serial.read();
      if (inByte == START_MARKER) {
        // 读取图像大小（4字节）
        uint8_t sizeBuf[SIZE_BYTES];
        size_t bytesRead = Serial.readBytes(sizeBuf, SIZE_BYTES);
        
        if (bytesRead == SIZE_BYTES) {
          // 将4个字节组合成一个32位整数
          imageSize = ((uint32_t)sizeBuf[0]) | 
                      ((uint32_t)sizeBuf[1] << 8) | 
                      ((uint32_t)sizeBuf[2] << 16) | 
                      ((uint32_t)sizeBuf[3] << 24);
          
          // 验证图像大小是否合理
          if (imageSize > 0 && imageSize <= MAX_IMAGE_SIZE) {
            // 动态分配内存
            imageBuffer = (uint8_t*)malloc(imageSize);
            if (imageBuffer == NULL) {
              Serial.println("内存分配失败，无法接收图像");
              return;
            }
            
            receiving = true;
            bytesReceived = 0;
            Serial.print("开始接收图像，大小: ");
            Serial.print(imageSize);
            Serial.println(" 字节");
          } else {
            Serial.print("图像大小超出范围: ");
            Serial.println(imageSize);
          }
        }
      }
    } else {
      // 接收图像数据
      uint8_t inByte = Serial.read();
      
      // 检查是否为结束标记
      if (bytesReceived == imageSize && inByte == END_MARKER) {
        receiving = false;
        Serial.println("图像接收完成");
        
        // 发送图像到阿里云
        isProcessing = true;
        digitalWrite(LED_PIN, LOW); // 指示处理开始
        sendImageToCloud();
        isProcessing = false;
        digitalWrite(LED_PIN, HIGH); // 指示处理结束
        
        // 释放内存
        if (imageBuffer != NULL) {
          free(imageBuffer);
          imageBuffer = NULL;
        }
      } else if (bytesReceived < imageSize) {
        // 存储图像数据
        imageBuffer[bytesReceived++] = inByte;
        
        // 打印接收进度（每接收10KB打印一次）
        if (bytesReceived % 10240 == 0) {
          Serial.print("已接收: ");
          Serial.print(bytesReceived);
          Serial.print(" / ");
          Serial.println(imageSize);
        }
      }
      
      // 检查缓冲区是否已满
      if (bytesReceived >= imageSize) {
        Serial.println("警告：已达到图像大小上限");
      }
    }
  }
}

// 分块Base64编码
void encodeAndSendBase64(WiFiClientSecure& client, uint8_t* data, size_t dataLength, const String& prefix, const String& suffix) {
  // 计算Base64编码后的大致长度
  size_t encodedLength = ((dataLength + 2) / 3) * 4 + 1;
  
  // 创建临时缓冲区
  const size_t bufferSize = 512;
  uint8_t inputChunk[bufferSize];
  char outputChunk[bufferSize * 2]; // Base64编码最多会增加33%的大小
  
  // 发送前缀
  if (prefix.length() > 0) {
    client.print(prefix);
  }
  
  // 分块处理
  for (size_t i = 0; i < dataLength; i += bufferSize) {
    // 确定当前块大小
    size_t chunkSize = min(bufferSize, dataLength - i);
    
    // 复制数据到临时缓冲区
    memcpy(inputChunk, data + i, chunkSize);
    
    // 编码当前块
    size_t outputSize = encode_base64(inputChunk, chunkSize, (unsigned char*)outputChunk);
    outputChunk[outputSize] = '\0'; // 确保字符串正确终止
    
    // 发送编码后的数据
    client.print(outputChunk);
    
    // 允许WiFi处理事件
    yield();
  }
  
  // 发送后缀
  if (suffix.length() > 0) {
    client.print(suffix);
  }
}

// 发送图像到阿里云DashScope
void sendImageToCloud() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi未连接，无法发送数据");
    return;
  }
  
  if (imageBuffer == NULL || imageSize == 0) {
    Serial.println("没有图像数据可发送");
    return;
  }
  
  Serial.println("正在发送图像到阿里云DashScope...");
  
  // 准备JSON部分数据
  const char* jsonStart = "{"
    "\"model\":\"qvq-max\","
    "\"messages\":[{"
      "\"role\":\"user\","
      "\"content\":[{"
        "\"type\":\"image_url\","
        "\"image_url\":{"
          "\"url\":\"data:image/jpeg;base64,";
  
  const char* jsonMiddle = "\""
        "}"
      "},{"
        "\"type\":\"text\","
        "\"text\":\"分析这张图片中是否有火灾场景或人员，如果有请详细描述位置和数量\""
      "}]"
    "}],"
    "\"stream\":false"
  "}";
  
  // 尝试连接阿里云API
  if (client.connect(host, 443)) {
    Serial.println("已连接到阿里云DashScope");
    
    // 计算内容长度（估计值，实际长度会在编码过程中准确计算）
    size_t base64Length = ((imageSize + 2) / 3) * 4; // Base64编码后的大致长度
    size_t contentLength = strlen(jsonStart) + base64Length + strlen(jsonMiddle);
    
    // 构建HTTP请求头
    client.print("POST ");
    client.print(api_endpoint);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(host);
    client.print("Authorization: Bearer ");
    client.println(api_key);
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(contentLength);
    client.println("Connection: close");
    client.println();
    
    // 发送JSON开始部分
    client.print(jsonStart);
    
    // 分块编码和发送图像数据
    encodeAndSendBase64(client, imageBuffer, imageSize, "", "");
    
    // 发送JSON结束部分
    client.print(jsonMiddle);
    
    Serial.println("请求已发送，等待响应...");
    
    // 设置超时
    unsigned long timeout = millis();
    while (client.connected() && !client.available()) {
      if (millis() - timeout > 30000) { // 30秒超时
        Serial.println("响应超时");
        client.stop();
        return;
      }
      delay(100);
      yield(); // 让ESP8266处理WiFi事件
    }
    
    // 读取响应头
    String line;
    while (client.connected()) {
      line = client.readStringUntil('\n');
      if (line == "\r") {
        // 响应头结束
        break;
      }
      yield(); // 让ESP8266处理WiFi事件
    }
    
    // 读取响应体
    String response = "";
    while (client.available()) {
      char c = client.read();
      response += c;
      
      // 防止响应过大
      if (response.length() > 10000) {
        Serial.println("响应过大，截断处理");
        break;
      }
      
      // 每读取一段数据就让ESP8266处理其他事件
      if (response.length() % 100 == 0) {
        yield();
      }
    }
    
    // 解析响应以提取结果
    Serial.println("AI响应:");
    Serial.println(response);
    
    // 将响应发送到Python脚本
    Serial.println("{\"ai_response\":" + response + "}");
    
    client.stop();
    Serial.println("连接已关闭");
  } else {
    Serial.println("无法连接到阿里云DashScope");
  }
}