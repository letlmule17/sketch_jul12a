import serial
import json
import time
import threading
import argparse
import winsound  # Windows声音提示，其他系统可用其他库
from datetime import datetime
import matplotlib.pyplot as plt
import re

# 全局变量
latest_results = {}
detection_history = {
    'timestamp': [],
    'fire_detected': [],
    'people_count': []
}
is_running = True
alarm_active = False

# 声音报警
def sound_alarm(duration=1000, frequency=2500):
    """播放声音报警"""
    try:
        winsound.Beep(frequency, duration)
    except:
        print("\a")  # 使用终端的响铃作为备选

# 初始化图表
def init_plots():
    """初始化实时监测图表"""
    plt.ion()  # 开启交互模式
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8))
    
    # 火灾检测图表
    ax1.set_title('火灾检测历史')
    ax1.set_ylabel('检测状态')
    ax1.set_ylim(-0.1, 1.1)
    ax1.set_yticks([0, 1])
    ax1.set_yticklabels(['未检测', '已检测'])
    ax1.grid(True)
    
    # 人数检测图表
    ax2.set_title('人数检测历史')
    ax2.set_xlabel('时间')
    ax2.set_ylabel('人数')
    ax2.set_ylim(-0.1, 10)  # 根据实际情况调整
    ax2.grid(True)
    
    return fig, ax1, ax2

# 更新图表
def update_plots(fig, ax1, ax2):
    """更新监测图表"""
    while is_running:
        if len(detection_history['timestamp']) > 0:
            # 更新火灾检测图表
            ax1.clear()
            ax1.set_title('火灾检测历史')
            ax1.set_ylabel('检测状态')
            ax1.set_ylim(-0.1, 1.1)
            ax1.set_yticks([0, 1])
            ax1.set_yticklabels(['未检测', '已检测'])
            ax1.grid(True)
            ax1.plot(detection_history['timestamp'], detection_history['fire_detected'], 
                    'r-o', label='火灾')
            ax1.legend()
            
            # 更新人数检测图表
            ax2.clear()
            ax2.set_title('人数检测历史')
            ax2.set_xlabel('时间')
            ax2.set_ylabel('人数')
            max_people = max(detection_history['people_count'] + [1])
            ax2.set_ylim(-0.1, max_people + 1)
            ax2.grid(True)
            ax2.plot(detection_history['timestamp'], detection_history['people_count'], 
                    'b-o', label='人数')
            ax2.legend()
            
            # 限制历史数据点数量
            max_points = 20
            if len(detection_history['timestamp']) > max_points:
                detection_history['timestamp'] = detection_history['timestamp'][-max_points:]
                detection_history['fire_detected'] = detection_history['fire_detected'][-max_points:]
                detection_history['people_count'] = detection_history['people_count'][-max_points:]
            
            plt.tight_layout()
            fig.canvas.draw_idle()
            plt.pause(1)
        else:
            plt.pause(1)

# 从文本中提取检测结果
def extract_detection_results(text):
    """从DashScope响应中提取火灾和人员检测结果"""
    fire_detected = False
    people_count = 0
    
    # 检测火灾的关键词
    fire_keywords = ['火灾', '火', '火焰', '燃烧', 'fire', 'burning', 'flame']
    for keyword in fire_keywords:
        if keyword in text.lower():
            fire_detected = True
            break
    
    # 提取人数
    # 尝试不同的模式匹配人数
    people_patterns = [
        r'(\d+)\s*人',  # 中文模式: 3人, 5个人
        r'(\d+)\s*persons?',  # 英文模式: 3 persons, 1 person
        r'(\d+)\s*individuals?',  # 英文模式: 2 individuals
        r'(\d+)\s*people',  # 英文模式: 5 people
        r'总共\s*(\d+)',  # 中文模式: 总共3
        r'共\s*(\d+)',  # 中文模式: 共3
        r'total\s*of\s*(\d+)',  # 英文模式: total of 3
    ]
    
    for pattern in people_patterns:
        matches = re.findall(pattern, text, re.IGNORECASE)
        if matches:
            try:
                # 取找到的最大数字
                people_count = max([int(m) for m in matches])
                break
            except ValueError:
                continue
    
    # 如果没有找到人数但提到了人
    if people_count == 0 and ('人' in text or 'person' in text.lower() or 'people' in text.lower()):
        people_count = 1
    
    return fire_detected, people_count

# 串口读取线程
def read_from_serial(ser):
    """从串口读取数据并处理"""
    global latest_results, alarm_active
    
    buffer = ""
    json_buffer = ""
    collecting_json = False
    
    while is_running:
        if ser.in_waiting > 0:
            try:
                char = ser.read(1).decode('utf-8')
                
                # 检测JSON开始
                if char == '{' and not collecting_json:
                    collecting_json = True
                    json_buffer = "{"
                    continue
                
                # 收集JSON数据
                if collecting_json:
                    json_buffer += char
                    
                    # 检测JSON结束
                    if char == '}' and json_buffer.count('{') == json_buffer.count('}'):
                        collecting_json = False
                        process_json(json_buffer)
                        json_buffer = ""
                else:
                    # 普通行处理
                    if char == '\n':
                        if buffer.strip():
                            print(f"ESP8266: {buffer}")
                        buffer = ""
                    else:
                        buffer += char
                        
            except UnicodeDecodeError:
                print("[警告] 收到非UTF8数据")
                buffer = ""
                json_buffer = ""
                collecting_json = False
            except Exception as e:
                print(f"[错误] 读取串口数据失败: {e}")
                buffer = ""
                json_buffer = ""
                collecting_json = False
        else:
            time.sleep(0.01)

# 处理JSON数据
def process_json(json_str):
    """处理接收到的JSON数据"""
    try:
        data = json.loads(json_str)
        
        # 检查是否为阿里云DashScope响应
        if "ai_response" in data:
            ai_response = data["ai_response"]
            
            # 提取DashScope返回的文本内容
            if isinstance(ai_response, dict) and "choices" in ai_response:
                choices = ai_response["choices"]
                if len(choices) > 0 and "message" in choices[0]:
                    message = choices[0]["message"]
                    if "content" in message:
                        text_content = message["content"]
                        print("\n===== AI分析结果 =====")
                        print(text_content)
                        print("=====================\n")
                        
                        # 提取火灾和人员检测结果
                        fire_detected, people_count = extract_detection_results(text_content)
                        
                        # 更新检测历史
                        now = datetime.now().strftime("%H:%M:%S")
                        detection_history['timestamp'].append(now)
                        detection_history['fire_detected'].append(1 if fire_detected else 0)
                        detection_history['people_count'].append(people_count)
                        
                        # 输出检测结果
                        status_msg = []
                        if fire_detected:
                            status_msg.append("🔥 检测到火灾!")
                            if not alarm_active:
                                alarm_active = True
                                threading.Thread(target=sound_alarm, daemon=True).start()
                        else:
                            alarm_active = False
                            
                        if people_count > 0:
                            status_msg.append(f"👤 检测到 {people_count} 人")
                        
                        if status_msg:
                            print("\n" + " | ".join(status_msg))
    except json.JSONDecodeError:
        print(f"[警告] 无法解析JSON: {json_str[:100]}...")
    except Exception as e:
        print(f"[错误] 处理JSON数据失败: {e}")

# 主函数
def main():
    global is_running
    
    # 命令行参数
    parser = argparse.ArgumentParser(description='阿里云DashScope视觉分析系统')
    parser.add_argument('--port', required=True, help='ESP8266串口 (例如 COM3)')
    parser.add_argument('--baud', type=int, default=115200, help='串口波特率')
    parser.add_argument('--no-plot', action='store_true', help='禁用图表显示')
    
    args = parser.parse_args()
    
    # 连接ESP8266
    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.1)
        print(f"已连接到ESP8266，端口 {args.port}，波特率 {args.baud}")
        
        # 启动串口读取线程
        serial_thread = threading.Thread(target=read_from_serial, args=(ser,), daemon=True)
        serial_thread.start()
        
        # 初始化并启动图表
        if not args.no_plot:
            fig, ax1, ax2 = init_plots()
            plot_thread = threading.Thread(target=update_plots, args=(fig, ax1, ax2), daemon=True)
            plot_thread.start()
        
        # 主循环 - 处理用户输入
        print("\n命令:")
        print("  q - 退出程序")
        print("  s - 查看当前状态")
        print("  t - 测试报警")
        print("\n等待ESP8266发送检测数据...")
        
        while True:
            cmd = input("> ")
            
            if cmd.lower() == 'q':
                break
            elif cmd.lower() == 's':
                print("\n当前状态:")
                if detection_history['timestamp']:
                    print(f"最近检测时间: {detection_history['timestamp'][-1]}")
                    print(f"火灾检测: {'是' if detection_history['fire_detected'][-1] == 1 else '否'}")
                    print(f"检测到人数: {detection_history['people_count'][-1]}")
                else:
                    print("尚未收到任何检测数据")
            elif cmd.lower() == 't':
                print("测试报警...")
                sound_alarm()
            else:
                print("未知命令")
                
    except serial.SerialException as e:
        print(f"连接ESP8266失败: {e}")
    except KeyboardInterrupt:
        print("\n程序被用户中断")
    except Exception as e:
        print(f"发生错误: {e}")
    finally:
        is_running = False
        if 'ser' in locals():
            ser.close()
            print("串口已关闭")
        print("程序已退出")

if __name__ == "__main__":
    main()