#include <SoftwareSerial.h>

// ESP-01S引脚定义
SoftwareSerial esp01s(2, 3); // RX, TX (Arduino D2连接ESP-01S的TX, D3连接ESP-01S的RX)

// WiFi设置
const char* sta_ssid = "letlmulee"; // 替换为您的WiFi名称
const char* sta_password = "12345678"; // 替换为您的WiFi密码
const char* ap_ssid = "ESP8266_AP"; // 热点名称
const char* ap_password = "12345678"; // 热点密码

void setup() {
  // 初始化串口通信
  Serial.begin(9600);  // 调试用串口
  delay(1000);  // 稳定串口
  
  esp01s.begin(115200); // ESP-01S波特率
  delay(2000);  // 等待ESP-01S启动完成
  
  Serial.println("ESP-01S WiFi双模式设置");
  
  // 多次尝试AT命令确保连接稳定
  for(int i=0; i<3; i++) {
    sendATCommand("AT");
    delay(500);
  }
  
  // 重置ESP-01S
  sendATCommand("AT+RST");
  delay(3000);  // 等待重置完成
  
  // 设置STA+AP双模式(模式3)
  sendATCommand("AT+CWMODE=3");
  delay(1000);
  
  // 设置AP参数
  String ap_cmd = "AT+CWSAP=\"";
  ap_cmd += ap_ssid;
  ap_cmd += "\",\"";
  ap_cmd += ap_password;
  ap_cmd += "\",5,3";  // 通道5，加密方式3(WPA2_PSK)
  sendATCommand(ap_cmd);
  delay(1000);
  
  // 连接WiFi
  Serial.print("正在连接到WiFi: ");
  Serial.println(sta_ssid);
  
  String cmd = "AT+CWJAP=\"";
  cmd += sta_ssid;
  cmd += "\",\"";
  cmd += sta_password;
  cmd += "\"";
  
  sendATCommand(cmd);
  delay(7000); // 等待连接
  
  // 检查IP地址
  sendATCommand("AT+CIFSR");
  
  // 设置多连接模式
  sendATCommand("AT+CIPMUX=1");
  
  // 建立服务器，端口80
  sendATCommand("AT+CIPSERVER=1,80");
  
  Serial.println("ESP8266 STA+AP模式配置完成");
  Serial.println("AP热点: ESP8266_AP，密码: 12345678");
  Serial.println("可通过AT+CIFSR查询IP地址");
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
  
  // 增加响应等待时间
  delay(1000);
  
  String response = "";
  unsigned long startTime = millis();
  
  // 最多等待5秒钟接收响应
  while (millis() - startTime < 5000) {
    if (esp01s.available()) {
      char c = esp01s.read();
      response += c;
      delay(2); // 给字符接收留出时间
    }
    
    // 如果接收到完整响应，提前结束等待
    if (response.indexOf("OK") != -1 || response.indexOf("ERROR") != -1) {
      delay(100);  // 再等待一点时间确保接收完整
      break;
    }
  }
  
  Serial.print("接收: ");
  Serial.println(response);
}