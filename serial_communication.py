import serial
import time
import threading
import argparse
import json
import socket
from datetime import datetime

# 尝试导入OpenCV，如果不可用则禁用相关功能
try:
    import cv2
    import numpy as np
    CV2_AVAILABLE = True
except ImportError:
    CV2_AVAILABLE = False
    print("警告: OpenCV (cv2) 未安装，图像处理功能将被禁用")
    print("如需启用图像处理，请运行: pip install opencv-python numpy")

# 全局变量
latest_sensor_data = {}
is_running = True

def read_from_serial(ser, device_name):
    """从串口读取数据并处理"""
    global latest_sensor_data
    
    while is_running:
        if ser.in_waiting > 0:
            try:
                line = ser.readline().decode('utf-8').strip()
                print(f"{device_name}: {line}")
                
                # 尝试解析JSON数据
                if line.startswith("{") and line.endswith("}"):
                    try:
                        data = json.loads(line)
                        latest_sensor_data = data
                        print(f"解析数据: {data}")
                    except json.JSONDecodeError:
                        pass
            except UnicodeDecodeError:
                print(f"{device_name}: [收到非UTF8数据]")
        time.sleep(0.01)

def tcp_server(port=8888):
    """TCP服务器，接收ESP8266发送的数据"""
    global latest_sensor_data, is_running
    
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind(('0.0.0.0', port))
    server_socket.settimeout(1.0)  # 设置超时，以便能够正常退出
    server_socket.listen(5)
    
    print(f"TCP服务器已启动，监听端口 {port}")
    
    connections = []
    
    try:
        while is_running:
            try:
                # 接受新连接
                conn, addr = server_socket.accept()
                print(f"新连接: {addr}")
                connections.append(conn)
                
                # 创建线程处理连接
                client_thread = threading.Thread(
                    target=handle_client_connection,
                    args=(conn, addr),
                    daemon=True
                )
                client_thread.start()
            except socket.timeout:
                continue
    finally:
        for conn in connections:
            try:
                conn.close()
            except:
                pass
        server_socket.close()
        print("TCP服务器已关闭")

def handle_client_connection(conn, addr):
    """处理客户端连接"""
    global latest_sensor_data, is_running
    
    try:
        while is_running:
            try:
                # 接收数据
                data = conn.recv(1024)
                if not data:
                    break
                    
                message = data.decode('utf-8').strip()
                print(f"从 {addr} 收到: {message}")
                
                # 尝试解析JSON数据
                try:
                    json_data = json.loads(message)
                    latest_sensor_data = json_data
                    
                    # 处理数据并返回结果
                    result = process_data(json_data)
                    response = json.dumps(result).encode('utf-8')
                    conn.send(response)
                except json.JSONDecodeError:
                    conn.send(b'{"error": "Invalid JSON format"}')
            except socket.timeout:
                continue
    except Exception as e:
        print(f"客户端处理错误: {e}")
    finally:
        conn.close()
        print(f"连接关闭: {addr}")

def process_data(data):
    """处理传感器数据并返回AI处理结果"""
    # 这里是模拟的AI处理逻辑
    # 在实际应用中，这里可以调用您的图像识别模型
    
    result = {
        "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "processed": True,
        "original_data": data
    }
    
    # 根据传感器值模拟不同的处理结果
    if "sensor" in data:
        sensor_value = data["sensor"]
        if sensor_value > 800:
            result["status"] = "high"
            result["action"] = "LED:ON"
        elif sensor_value < 200:
            result["status"] = "low"
            result["action"] = "LED:OFF"
        else:
            result["status"] = "normal"
            result["action"] = None
    
    print(f"处理结果: {result}")
    return result

def simulate_image_processing():
    """模拟图像处理功能"""
    if not CV2_AVAILABLE:
        print("错误: 无法启动图像处理，OpenCV未安装")
        return
        
    try:
        # 尝试打开摄像头
        cap = cv2.VideoCapture(0)
        if not cap.isOpened():
            print("无法打开摄像头")
            return
            
        print("模拟图像处理已启动")
        while is_running:
            # 捕获帧
            ret, frame = cap.read()
            if not ret:
                print("无法获取图像")
                break
                
            # 简单处理 - 转为灰度
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            
            # 显示结果
            cv2.imshow('原图', frame)
            cv2.imshow('处理后', gray)
            
            # 按'q'退出
            if cv2.waitKey(1) == ord('q'):
                break
                
    except Exception as e:
        print(f"图像处理错误: {e}")
    finally:
        if 'cap' in locals() and cap is not None:
            cap.release()
        if CV2_AVAILABLE:
            cv2.destroyAllWindows()
        print("图像处理已停止")

def main():
    global is_running
    
    parser = argparse.ArgumentParser(description='Arduino和ESP8266通信系统')
    parser.add_argument('--arduino', help='Arduino串口 (例如 COM3)')
    parser.add_argument('--esp', help='ESP8266串口 (例如 COM4)')
    parser.add_argument('--baud_arduino', type=int, default=9600, help='Arduino波特率')
    parser.add_argument('--baud_esp', type=int, default=115200, help='ESP8266波特率')
    parser.add_argument('--tcp_port', type=int, default=8888, help='TCP服务器端口')
    parser.add_argument('--enable_camera', action='store_true', help='启用摄像头图像处理')
    
    args = parser.parse_args()
    
    # 连接Arduino
    arduino_ser = None
    if args.arduino:
        try:
            arduino_ser = serial.Serial(args.arduino, args.baud_arduino, timeout=0.1)
            print(f"已连接到Arduino，端口 {args.arduino}，波特率 {args.baud_arduino}")
            
            # 启动线程读取Arduino数据
            arduino_thread = threading.Thread(
                target=read_from_serial, 
                args=(arduino_ser, "Arduino"), 
                daemon=True
            )
            arduino_thread.start()
        except Exception as e:
            print(f"连接Arduino失败: {e}")
    
    # 连接ESP8266
    esp_ser = None
    if args.esp:
        try:
            esp_ser = serial.Serial(args.esp, args.baud_esp, timeout=0.1)
            print(f"已连接到ESP8266，端口 {args.esp}，波特率 {args.baud_esp}")
            
            # 启动线程读取ESP8266数据
            esp_thread = threading.Thread(
                target=read_from_serial, 
                args=(esp_ser, "ESP8266"), 
                daemon=True
            )
            esp_thread.start()
        except Exception as e:
            print(f"连接ESP8266失败: {e}")
    
    # 启动TCP服务器
    tcp_thread = threading.Thread(target=tcp_server, args=(args.tcp_port,), daemon=True)
    tcp_thread.start()
    
    # 启动图像处理（如果启用且OpenCV可用）
    if args.enable_camera and CV2_AVAILABLE:
        image_thread = threading.Thread(target=simulate_image_processing, daemon=True)
        image_thread.start()
    elif args.enable_camera and not CV2_AVAILABLE:
        print("警告: 无法启动图像处理，OpenCV未安装")
    
    print("\n命令:")
    print("  a:消息 - 发送消息到Arduino")
    print("  e:消息 - 发送消息到ESP8266")
    if CV2_AVAILABLE:
        print("  c - 启动摄像头图像处理")
    print("  q - 退出")
    print("\n开始交互模式. 输入命令:")
    
    try:
        while True:
            cmd = input("> ")
            
            if cmd.lower() == 'q':
                break
                
            elif cmd.lower() == 'c':
                if not CV2_AVAILABLE:
                    print("错误: 无法启动图像处理，OpenCV未安装")
                elif not args.enable_camera:
                    print("启动图像处理...")
                    image_thread = threading.Thread(target=simulate_image_processing, daemon=True)
                    image_thread.start()
                else:
                    print("图像处理已经在运行")
                
            elif cmd.startswith("a:") and arduino_ser:
                message = cmd[2:] + "\n"
                arduino_ser.write(message.encode())
                print(f"发送到Arduino: {cmd[2:]}")
                
            elif cmd.startswith("e:") and esp_ser:
                message = cmd[2:] + "\n"
                esp_ser.write(message.encode())
                print(f"发送到ESP8266: {cmd[2:]}")
                
            else:
                print("无效的命令格式或设备未连接")
                
    except KeyboardInterrupt:
        print("\n退出中...")
    
    finally:
        is_running = False
        time.sleep(1)  # 给线程一些时间清理
        
        # 关闭串口连接
        if arduino_ser:
            arduino_ser.close()
        if esp_ser:
            esp_ser.close()

if __name__ == "__main__":
    main() 