#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// STA模式的WiFi设置
const char* sta_ssid = "letlmulee";     // 已有WiFi名称
const char* sta_password = "12345678";  // 已有WiFi密码

// AP模式的WiFi设置
const char* ap_ssid = "ESP8266_AP";     // 创建的热点名称
const char* ap_password = "12345678";   // 热点密码

ESP8266WebServer server(80);            // 创建web服务器对象，端口80

// LED状态变量
bool ledState = false;

void setup() {
  // 初始化串口
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n初始化开始...");
  
  // 设置LED引脚
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // ESP8266上LED是低电平点亮
  
  // 设置STA+AP双模式
  WiFi.mode(WIFI_AP_STA);
  
  // 配置AP模式
  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("AP模式已启动，IP地址: ");
  Serial.println(WiFi.softAPIP());
  
  // 配置STA模式(连接到WiFi)
  WiFi.begin(sta_ssid, sta_password);
  Serial.print("正在连接WiFi...");
  
  // 等待连接，最多等待20秒
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 40) {
    delay(500);
    Serial.print(".");
    timeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi已连接");
    Serial.print("STA模式IP地址: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi连接失败！仅AP模式可用");
  }
  
  // 配置Web服务器路由
  server.on("/", handleRoot);          // 根路径
  server.on("/led/on", turnLedOn);     // LED开
  server.on("/led/off", turnLedOff);   // LED关
  server.on("/status", getStatus);     // 获取状态
  server.onNotFound(handleNotFound);   // 404处理
  
  // 启动Web服务器
  server.begin();
  Serial.println("HTTP服务器已启动");
}

void loop() {
  server.handleClient();  // 处理客户端请求
}

// 网页根目录处理函数
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<meta charset='utf-8'>";
  html += "<style>";
  html += "body {font-family: Arial, sans-serif; text-align: center; margin: 20px; background-color: #f0f0f0;}";
  html += ".container {max-width: 500px; margin: 0 auto; padding: 20px; background-color: white; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1);}";
  html += "h1 {color: #0066cc;}";
  html += ".status {margin: 20px 0; padding: 10px; background-color: #e6f7ff; border-radius: 5px;}";
  html += "button {background-color: #4CAF50; color: white; border: none; padding: 10px 20px; margin: 5px; border-radius: 5px; cursor: pointer;}";
  html += "button.off {background-color: #f44336;}";
  html += ".network-info {margin-top: 20px; text-align: left; padding: 10px; background-color: #f9f9f9; border-radius: 5px;}";
  html += "</style>";
  html += "<title>ESP8266 控制面板</title></head><body>";
  html += "<div class='container'>";
  html += "<h1>ESP8266 双模式控制面板</h1>";
  
  html += "<div class='status'>";
  html += "<p>LED 状态: <b id='led-status'>" + String(ledState ? "开启" : "关闭") + "</b></p>";
  html += "</div>";
  
  html += "<div>";
  html += "<button onclick='turnOn()'>打开 LED</button>";
  html += "<button class='off' onclick='turnOff()'>关闭 LED</button>";
  html += "</div>";
  
  html += "<div class='network-info'>";
  html += "<h3>网络信息:</h3>";
  html += "<p>AP 模式 SSID: <b>" + String(ap_ssid) + "</b></p>";
  html += "<p>AP 模式 IP: <b>" + WiFi.softAPIP().toString() + "</b></p>";
  
  if (WiFi.status() == WL_CONNECTED) {
    html += "<p>STA 模式已连接到: <b>" + String(sta_ssid) + "</b></p>";
    html += "<p>STA 模式 IP: <b>" + WiFi.localIP().toString() + "</b></p>";
  } else {
    html += "<p>STA 模式: <b>未连接</b></p>";
  }
  html += "</div>";
  
  html += "</div>";
  
  html += "<script>";
  html += "function turnOn() { fetch('/led/on').then(response => response.json()).then(updateStatus); }";
  html += "function turnOff() { fetch('/led/off').then(response => response.json()).then(updateStatus); }";
  html += "function updateStatus() { fetch('/status').then(response => response.json()).then(data => { document.getElementById('led-status').textContent = data.led ? '开启' : '关闭'; }); }";
  html += "setInterval(updateStatus, 2000);";
  html += "</script>";
  
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// LED开启
void turnLedOn() {
  ledState = true;
  digitalWrite(LED_BUILTIN, LOW);  // ESP8266上LED是低电平点亮
  server.send(200, "application/json", "{\"success\":true, \"led\":true}");
}

// LED关闭
void turnLedOff() {
  ledState = false;
  digitalWrite(LED_BUILTIN, HIGH); // ESP8266上LED是高电平熄灭
  server.send(200, "application/json", "{\"success\":true, \"led\":false}");
}

// 获取状态
void getStatus() {
  String status = "{\"led\":" + String(ledState ? "true" : "false") + ", ";
  status += "\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\", ";
  
  if (WiFi.status() == WL_CONNECTED) {
    status += "\"wifi_connected\":true, ";
    status += "\"sta_ip\":\"" + WiFi.localIP().toString() + "\"";
  } else {
    status += "\"wifi_connected\":false";
  }
  
  status += "}";
  server.send(200, "application/json", status);
}

// 404页面处理
void handleNotFound() {
  String message = "页面未找到\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\n方法: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\n参数: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}
