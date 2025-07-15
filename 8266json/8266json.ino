 /*
 * ESP8266-NodeMCU Json解析程序
 * 使用ArduinoJson来对Json数据进行解析  
 * 
 */
#include <ArduinoJson.h>
 
void setup() {
  Serial.begin(115200);
  Serial.println("");

  /* 1. 自定义一个Json数据用来解析 */
  String json_str = "{\"name\":\"william\",\"years\":18}";

  /* 2. 创建一个DynamicJsonDocument对象*/
  /* 计算创建一个DynamicJsonDocument对象将要占用多大的内存空间 */
  const size_t capacity = JSON_OBJECT_SIZE(2) + 30;
  /* 通过计算好的内存空间传入参数创建一个DynamicJsonDocument对象 */
  DynamicJsonDocument doc(capacity);
  
  /* 3. 使用deserializeJson()函数来解析Json数据 */
  deserializeJson(doc, json_str);
 
  /* 4. 获取解析后的数据信息 */
  String nameStr = doc["name"].as<String>();
  int yearsInt = doc["years"].as<int>();
 
  /* 5. 通过串口监视器输出解析后的数据信息 */
  Serial.print("json_str = ");Serial.println(json_str);
  Serial.print("name = ");Serial.println(nameStr);
  Serial.print("years = ");Serial.println(yearsInt);
}
void loop() {}
