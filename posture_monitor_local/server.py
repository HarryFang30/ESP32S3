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
# 不再使用简单的 warning_count，而是用一个列表来存储所有事件
# 每次重启服务器，历史数据会清空。如果需要持久化，未来可以替换为数据库。
events_history = []

# 确保 uploads 文件夹存在
if not os.path.exists(app.config['UPLOAD_FOLDER']):
    os.makedirs(app.config['UPLOAD_FOLDER'])

def convert_rgb_to_jpeg(rgb_file_path):
    """
    调用RGB转换脚本将RGB565文件转换为JPEG
    
    Args:
        rgb_file_path: RGB565文件路径
        
    Returns:
        转换后的JPEG文件路径，失败返回None
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
    # RGB565文件特征：
    # 1. 文件大小应该是偶数（每像素2字节）
    # 2. 文件大小应该对应常见的分辨率
    # 3. 数据不是JPEG格式
    
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

    # 验证是否是有效的JPEG文件
    if len(request.data) < 2 or request.data[0] != 0xFF or request.data[1] != 0xD8:
        print(f"Invalid JPEG header: {request.data[:10].hex() if len(request.data) >= 10 else 'too short'}")
        return "Invalid JPEG format", 400

    # 1. 创建新的事件记录
    timestamp = datetime.datetime.now()
    image_filename = f"{timestamp.strftime('%Y%m%d_%H%M%S')}.jpg"
    image_filepath = os.path.join(app.config['UPLOAD_FOLDER'], image_filename)
    
    with open(image_filepath, 'wb') as f:
        f.write(request.data)

    print(f"Photo saved: {image_filename}, size: {len(request.data)} bytes")

    new_event = {
        # 时间戳转换为ISO格式字符串，便于JS处理
        'timestamp': timestamp.isoformat(), 
        'image_url': f'/uploads/{image_filename}'
    }

    # 2. 将新事件存入历史记录
    events_history.append(new_event)
    print(f"新事件: {new_event}")

    # 3. 通过WebSocket广播这个“新事件”，而不是全部数据
    socketio.emit('new_warning', new_event)
    
    return "Upload success", 200

@socketio.on('connect')
def handle_connect():
    """当有新的浏览器客户端连接时，将完整的历史数据发送给它"""
    print("新客户端连接，发送历史数据...")
    socketio.emit('initial_data', events_history)

# --- 运行服务器 ---
if __name__ == '__main__':
    print("数据看板服务器启动于 http://0.0.0.0:5001")
    socketio.run(app, host='0.0.0.0', port=5001)