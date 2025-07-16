#include <SoftwareSerial.h>

// ESP-01S引脚定义
SoftwareSerial esp01s(2, 3); // RX, TX (Arduino D2连接ESP-01S的TX, D3连接ESP-01S的RX)

// WiFi设置
const char* sta_ssid = "letlmulee"; // 替换为您的WiFi名称
const char* sta_password = "12345678"; // 替换为您的WiFi密码
const char* ap_ssid = "ESP8266_AP"; // 热点名称
const char* ap_password = "12345678"; // 热点密码

// 数据传输相关
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 1000; // 数据发送间隔(毫秒)
int sensorValue = 0; // 传感器数据示例

void setup() {
  // 初始化串口通信
  Serial.begin(9600);  // 调试用串口
  delay(1000);  // 稳定串口
  
  esp01s.begin(115200); // ESP-01S波特率
  delay(2000);  // 等待ESP-01S启动完成
  
  Serial.println("Arduino与ESP-01S通信系统启动");
  
  // 配置ESP-01S为STA+AP模式
  setupESP();
  
  Serial.println("系统就绪，开始数据传输");
}

void loop() {
  // 1. 读取传感器数据(示例)
  sensorValue = analogRead(A0);
  
  // 2. 定时发送数据到Python
  if (millis() - lastSendTime > sendInterval) {
    sendDataToPython();
    lastSendTime = millis();
  }
  
  // 3. 接收来自Python的命令
  receiveCommands();
}

// 配置ESP-01S
void setupESP() {
  Serial.println("配置ESP-01S...");
  
  // 重置ESP-01S
  sendCommand("AT+RST");
  delay(3000);
  
  // 设置STA+AP双模式(模式3)
  sendCommand("AT+CWMODE=3");
  delay(1000);
  
  // 设置AP参数
  String ap_cmd = "AT+CWSAP=\"";
  ap_cmd += ap_ssid;
  ap_cmd += "\",\"";
  ap_cmd += ap_password;
  ap_cmd += "\",5,3";
  sendCommand(ap_cmd);
  delay(1000);
  
  // 连接WiFi
  Serial.print("连接WiFi: ");
  Serial.println(sta_ssid);
  
  String cmd = "AT+CWJAP=\"";
  cmd += sta_ssid;
  cmd += "\",\"";
  cmd += sta_password;
  cmd += "\"";
  
  sendCommand(cmd);
  delay(7000);
  
  // 获取IP地址
  sendCommand("AT+CIFSR");
  
  // 设置多连接模式
  sendCommand("AT+CIPMUX=1");
  
  // 建立TCP服务器，端口8888
  sendCommand("AT+CIPSERVER=1,8888");
  
  Serial.println("ESP8266配置完成");
}

// 发送数据到Python
void sendDataToPython() {
  // 构建数据包
  String dataPacket = "{\"sensor\":";
  dataPacket += sensorValue;
  dataPacket += ",\"device\":\"Arduino\"}";
  
  // 通过ESP8266发送数据
  String cmd = "AT+CIPSEND=0,";
  cmd += dataPacket.length();
  sendCommand(cmd);
  delay(100);
  sendCommand(dataPacket);
  
  Serial.print("发送数据: ");
  Serial.println(dataPacket);
}

// 接收来自Python的命令
void receiveCommands() {
  if (esp01s.available()) {
    String response = esp01s.readStringUntil('\n');
    
    // 检查是否是数据包
    if (response.indexOf("+IPD") >= 0) {
      // 提取数据部分
      int dataStart = response.indexOf(":", response.indexOf("+IPD"));
      if (dataStart > 0) {
        String command = response.substring(dataStart + 1);
        command.trim();
        
        Serial.print("收到命令: ");
        Serial.println(command);
        
        // 处理命令(示例)
        processCommand(command);
      }
    } 
    else {
      // 其他ESP8266响应
      Serial.print("ESP响应: ");
      Serial.println(response);
    }
  }
  
  // 处理来自串口监视器的命令
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    esp01s.println(command); // 将命令转发给ESP-01S
  }
}

// 处理收到的命令
void processCommand(String command) {
  // 示例: 如果命令是"LED:ON"，点亮LED
  if (command == "LED:ON") {
    digitalWrite(LED_BUILTIN, HIGH);
  } 
  // 如果命令是"LED:OFF"，关闭LED
  else if (command == "LED:OFF") {
    digitalWrite(LED_BUILTIN, LOW);
  }
  // 可以添加更多命令处理逻辑
}

// 发送AT命令并等待响应
void sendCommand(String command) {
  Serial.print("发送: ");
  Serial.println(command);
  
  esp01s.println(command);
  delay(1000);
  
  // 读取响应
  while (esp01s.available()) {
    Serial.write(esp01s.read());
  }
  Serial.println();
}
}