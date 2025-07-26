# ESP32-S3 人脸距离监控系统

这个项目实现了一个基于ESP32-S3的智能人脸距离监控系统，具有以下功能：
- 实时人脸检测
- 距离测量和警告
- 蜂鸣器报警
- 自动拍照并上传到网页监控面板

## 功能特性

### 硬件功能
- 🎯 **实时人脸检测**: 使用ESP-DL库进行人脸识别
- 📏 **距离测量**: 基于眼部距离的深度估算
- 🚨 **多重警告**: 屏幕显示 + 蜂鸣器报警
- 📸 **自动拍照**: 警告触发时自动拍照上传
- 🖥️ **LCD显示**: 800x600摄像头图像缩放到320x240显示

### 网页监控面板
- 📊 **实时统计**: 显示总触发次数
- 📈 **时间图表**: 按小时统计触发次数
- 🖼️ **照片画廊**: 显示所有触发时刻的照片
- 🔄 **实时更新**: 通过WebSocket实时接收新的警告事件

## 快速开始

### 1. 硬件要求
- ESP32-S3开发板
- OV5640摄像头模块
- LCD显示屏 (320x240)
- 蜂鸣器

### 2. 配置WiFi和服务器
1. 打开 `main/APP/wifi_config.h` 文件
2. 修改你的WiFi信息：
   ```c
   #define WIFI_SSID      "你的WiFi名称"
   #define WIFI_PASSWORD  "你的WiFi密码"
   ```
3. 修改服务器地址（替换为你的电脑IP地址）：
   ```c
   #define SERVER_URL     "http://你的电脑IP:5000/upload"
   ```

### 3. 获取电脑IP地址
**Windows:**
```bash
ipconfig
```
找到"无线局域网适配器"下的IPv4地址

**Mac/Linux:**
```bash
ifconfig
```
找到en0或wlan0接口下的inet地址

### 4. 启动网页服务器
1. 进入 `posture_monitor_local` 文件夹
2. 安装Python依赖：
   ```bash
   pip install flask flask-socketio
   ```
3. 启动服务器：
   ```bash
   python server.py
   ```
4. 在浏览器中打开：`http://localhost:5000`

### 5. 编译和烧录ESP32代码
```bash
idf.py build
idf.py flash
idf.py monitor
```

## 系统工作流程

1. **初始化阶段**
   - 启动摄像头和LCD显示
   - 连接WiFi网络
   - 初始化人脸检测算法

2. **标定阶段**
   - 用户坐在距离摄像头50cm的位置
   - 系统收集20帧数据进行标定
   - 建立距离计算的基准

3. **监控阶段**
   - 实时人脸检测和距离计算
   - 当距离 < 45cm时触发警告
   - 当距离 > 48cm时解除警告

4. **警告处理**
   - 屏幕显示警告信息
   - 蜂鸣器发出报警声
   - 自动拍照并上传到服务器
   - 网页实时更新显示新的触发事件

## 距离阈值配置

可以在 `main/APP/face_distance_detector.hpp` 中调整距离阈值：

```cpp
static constexpr float ENTER_THRESHOLD_CM = 45.0f;    // 进入警告距离
static constexpr float EXIT_THRESHOLD_CM = 48.0f;     // 退出警告距离
```

## 故障排除

### WiFi连接问题
- 检查WiFi名称和密码是否正确
- 确保ESP32和电脑在同一网络
- 检查路由器是否支持2.4GHz（ESP32不支持5GHz）

### 网页无法接收数据
- 检查电脑防火墙设置
- 确认服务器已启动在5000端口
- 检查ESP32串口输出的上传状态

### 人脸检测不工作
- 确保摄像头正确连接
- 检查光线条件是否充足
- 确认人脸正对摄像头且距离适中

### 距离测量不准确
- 重新进行距离标定
- 确保标定时严格保持50cm距离
- 检查眼部关键点是否正确检测

## 技术架构

### ESP32端
- **主控**: ESP32-S3 (WiFi + 摄像头)
- **AI算法**: ESP-DL人脸检测库
- **距离算法**: 基于双眼间距的深度估算
- **网络**: WiFi + HTTP客户端

### 服务器端
- **后端**: Flask + Socket.IO
- **前端**: HTML5 + Chart.js
- **通信**: WebSocket实时更新
- **存储**: 内存数据库（重启后清空）

## 开发说明

### 添加新功能
1. 硬件控制代码在 `components/BSP/` 目录
2. AI算法代码在 `main/APP/` 目录
3. 网页代码在 `posture_monitor_local/` 目录

### 调试技巧
- 使用 `idf.py monitor` 查看ESP32串口输出
- 检查网页浏览器的开发者工具控制台
- 服务器日志会显示在运行Python脚本的终端中

## 许可证
本项目基于ESP-IDF许可证开源。
