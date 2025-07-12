import serial
import time
import os
import socket
from openai import OpenAI

# 串口通信部分
def serial_communication():
    # 配置串口
    # 替换'COM3'为您的实际串口号(Windows上通常是COMx，Linux/Mac上通常是/dev/ttyUSBx或/dev/ttyACMx)
    # 波特率需要与Arduino代码中设置的一致(9600)
    ser = serial.Serial(
        port='COM3',
        baudrate=9600,  # 与Arduino代码中设置的一致
        timeout=1,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        bytesize=serial.EIGHTBITS
    )
    
    try:
        # 确保串口已打开
        if not ser.is_open:
            ser.open()
        
        print("串口已打开，等待数据...")
        
        while True:
            # 读取数据
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8').strip()
                print(f"接收到数据: {line}")
                
                # 如果收到的是IP地址，可以尝试建立TCP连接
                if "IP地址:" in line:
                    ip = line.split("IP地址:")[1].strip()
                    print(f"检测到ESP-01S的IP地址: {ip}")
                    try:
                        tcp_client(ip, 80)  # 尝试连接ESP模块
                    except Exception as e:
                        print(f"TCP连接失败: {e}")
            
            # 发送AT指令示例
            # 取消注释下面的代码来发送AT指令
            # command = "AT+GMR\r\n"  # 获取版本信息
            # ser.write(command.encode('utf-8'))
            # time.sleep(1)
            
            time.sleep(0.1)  # 短暂延时，减少CPU使用率
            
    except KeyboardInterrupt:
        print("程序已终止")
    except Exception as e:
        print(f"发生错误: {e}")
    finally:
        # 确保关闭串口
        if ser.is_open:
            ser.close()
            print("串口已关闭")

# TCP客户端，连接到ESP-01S的服务器
def tcp_client(host, port):
    print(f"尝试连接到 {host}:{port}")
    client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client.settimeout(5)  # 设置超时时间
    client.connect((host, port))
    print("TCP连接成功！")
    
    try:
        while True:
            # 发送数据
            message = input("请输入要发送的消息 (输入'exit'退出): ")
            if message.lower() == 'exit':
                break
                
            client.send(message.encode('utf-8'))
            print(f"已发送: {message}")
            
            # 接收响应
            response = client.recv(1024).decode('utf-8')
            print(f"收到响应: {response}")
            
    except Exception as e:
        print(f"通信错误: {e}")
    finally:
        client.close()
        print("TCP连接已关闭")

# 使用阿里云大模型API示例
def use_ai_api():
    try:
        client = OpenAI(
            api_key=os.getenv("DASHSCOPE_API_KEY"),
            base_url="https://dashscope.aliyuncs.com/compatible-mode/v1",
        )

        completion = client.chat.completions.create(
            model="qwen-plus",
            messages=[
                {"role": "system", "content": "You are a helpful assistant."},
                {"role": "user", "content": "请提供一个简单的Arduino与ESP8266通信示例"},
            ],
        )
        print("AI响应:")
        print(completion.choices[0].message.content)
    except Exception as e:
        print(f"API调用错误: {e}")

if __name__ == "__main__":
    print("1. 串口通信模式")
    print("2. 使用AI API")
    choice = input("请选择模式 (1/2): ")
    
    if choice == "1":
        serial_communication()
    elif choice == "2":
        use_ai_api()
    else:
        print("无效选择") 