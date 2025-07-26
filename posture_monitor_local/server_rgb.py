#!/usr/bin/env python3
"""
智能图像处理服务器
支持JPEG和RGB565格式的图像上传
"""

import os
import datetime
import subprocess
from flask import Flask, render_template, request, send_from_directory
from flask_socketio import SocketIO

# --- 初始化 ---
app = Flask(__name__)
app.config['UPLOAD_FOLDER'] = 'uploads'
app.config['SECRET_KEY'] = 'a_very_secret_dashboard_key'
socketio = SocketIO(app)

# --- 内存数据库 ---
events_history = []

# 确保 uploads 文件夹存在
if not os.path.exists(app.config['UPLOAD_FOLDER']):
    os.makedirs(app.config['UPLOAD_FOLDER'])

def convert_rgb_to_jpeg(rgb_file_path):
    """
    调用RGB转换脚本将RGB565文件转换为JPEG
    """
    try:
        # 调用RGB转换脚本
        result = subprocess.run([
            'python', 'rgb_converter.py', 
            '--file', rgb_file_path
        ], capture_output=True, text=True, cwd=os.path.dirname(__file__))
        
        if result.returncode == 0:
            # 生成转换后的文件名
            base_name = os.path.splitext(rgb_file_path)[0]
            converted_file = f"{base_name}_converted.jpg"
            
            if os.path.exists(converted_file):
                print(f"RGB转换成功: {rgb_file_path} -> {converted_file}")
                return converted_file
            else:
                print(f"转换后文件不存在: {converted_file}")
                return None
        else:
            print(f"RGB转换失败: {result.stderr}")
            return None
            
    except Exception as e:
        print(f"调用RGB转换脚本失败: {e}")
        return None

def is_valid_jpeg(data):
    """检查数据是否是有效的JPEG格式"""
    return len(data) >= 2 and data[0] == 0xFF and data[1] == 0xD8

def is_likely_rgb565(data):
    """检查数据是否可能是RGB565格式"""
    if len(data) % 2 != 0:
        return False
        
    # 检查是否对应常见分辨率
    pixel_count = len(data) // 2
    common_resolutions = [
        800 * 600,   # 480,000 pixels
        400 * 600,   # 240,000 pixels
        320 * 240,   # 76,800 pixels
        160 * 120,   # 19,200 pixels
    ]
    
    return pixel_count in common_resolutions

# --- 路由和事件处理 ---

@app.route('/')
def index():
    """提供看板主页面"""
    return render_template('index.html')

@app.route('/uploads/<filename>')
def uploaded_file(filename):
    """让浏览器可以访问上传的图片"""
    from flask import Response
    import mimetypes
    
    # 确保正确设置MIME类型
    mimetype = mimetypes.guess_type(filename)[0]
    if filename.lower().endswith('.jpg') or filename.lower().endswith('.jpeg'):
        mimetype = 'image/jpeg'
    
    return send_from_directory(app.config['UPLOAD_FOLDER'], filename, mimetype=mimetype)

@app.route('/upload', methods=['POST'])
def upload_file():
    """接收来自 ESP32 的图片上传"""
    if not request.data:
        return "Bad request", 400

    # 1. 创建新的事件记录
    timestamp = datetime.datetime.now()
    image_filename = f"{timestamp.strftime('%Y%m%d_%H%M%S')}.jpg"
    image_filepath = os.path.join(app.config['UPLOAD_FOLDER'], image_filename)
    
    # 2. 保存原始数据
    with open(image_filepath, 'wb') as f:
        f.write(request.data)

    print(f"文件已保存: {image_filename}, 大小: {len(request.data)} 字节")

    # 3. 检查文件格式并处理
    final_image_url = None
    
    if is_valid_jpeg(request.data):
        # 有效的JPEG文件，直接使用
        print("✅ 检测到有效JPEG格式")
        final_image_url = f'/uploads/{image_filename}'
        
    elif is_likely_rgb565(request.data):
        # 可能是RGB565格式，尝试转换
        print("🔄 检测到RGB565格式，使用大端序RGB转换...")
        converted_file = convert_rgb_to_jpeg(image_filepath)
        
        if converted_file:
            # 转换成功，使用转换后的文件
            converted_filename = os.path.basename(converted_file)
            final_image_url = f'/uploads/{converted_filename}'
            print(f"✅ RGB565转换成功: {converted_filename}")
        else:
            # 转换失败，返回错误
            print("❌ RGB565转换失败")
            return "RGB conversion failed", 400
    else:
        # 未知格式
        print(f"❌ 未知文件格式，数据头: {request.data[:10].hex() if len(request.data) >= 10 else 'too short'}")
        return "Unknown file format", 400

    # 4. 创建事件记录
    new_event = {
        'timestamp': timestamp.isoformat(), 
        'image_url': final_image_url
    }

    # 5. 将新事件存入历史记录
    events_history.append(new_event)
    print(f"📝 新事件: {new_event}")

    # 6. 通过WebSocket广播这个"新事件"
    socketio.emit('new_warning', new_event)
    
    return "Upload success", 200

@socketio.on('connect')
def handle_connect():
    """当有新的浏览器客户端连接时，将完整的历史数据发送给它"""
    print("🔗 新客户端连接，发送历史数据...")
    socketio.emit('initial_data', events_history)

# --- 运行服务器 ---
if __name__ == '__main__':
    print("🚀 智能图像处理服务器启动于 http://0.0.0.0:5001")
    print("📷 支持格式: JPEG, RGB565")
    socketio.run(app, host='0.0.0.0', port=5001)
