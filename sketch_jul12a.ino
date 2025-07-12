#include <SoftwareSerial.h>

// ESP-01S引脚定义
SoftwareSerial esp01s(2, 3); // RX, TX (Arduino D2连接ESP-01S的TX, D3连接ESP-01S的RX)

// WiFi设置
const char* ssid = "letlmulee"; // 替换为您的WiFi名称
const char* password = "12345678"; // 替换为您的WiFi密码

void setup() {
  // 初始化串口通信
  Serial.begin(9600);  // 调试用串口
  esp01s.begin(115200); // ESP-01S默认波特率
  
  Serial.println("ESP-01S WiFi模块测试");
  
  // 重置ESP-01S
  sendATCommand("AT+RST");
  delay(1000);
  
  // 设置ESP-01S为Station模式
  sendATCommand("AT+CWMODE=1");
  
  // 连接WiFi
  Serial.print("正在连接到WiFi: ");
  Serial.println(ssid);
  
  String cmd = "AT+CWJAP=\"";
  cmd += ssid;
  cmd += "\",\"";
  cmd += password;
  cmd += "\"";
  
  sendATCommand(cmd);
  delay(5000); // 等待连接
  
  // 检查IP地址
  sendATCommand("AT+CIFSR");
}

void loop() {
  // 将ESP-01S接收到的数据转发到串口监视器
  if (esp01s.available()) {
    Serial.write(esp01s.read());
  }
  
  // 将串口监视器的数据发送给ESP-01S
  if (Serial.available()) {
    esp01s.write(Serial.read());
  }
}

// 发送AT命令并打印响应
void sendATCommand(String command) {
  Serial.print("发送: ");
  Serial.println(command);
  
  esp01s.println(command);
  delay(1000);
  
  String response = "";
  while (esp01s.available()) {
    char c = esp01s.read();
    response += c;
  }
  
  Serial.print("接收: ");
  Serial.println(response);
}
