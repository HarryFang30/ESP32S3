# 🚀 快速启动指南

## 基于姿态感知的鲁棒型人脸过近检测系统

### 📋 系统要求

- **硬件**: ESP32-S3 开发板
- **软件**: ESP-IDF 4.4+
- **摄像头**: 支持的摄像头模块
- **内存**: 至少 8MB PSRAM

### 🛠️ 快速构建

1. **环境准备**
```bash
# 确保ESP-IDF环境已配置
source $HOME/esp/esp-idf/export.sh
```

2. **一键构建**
```bash
# 进入项目目录
cd /Users/harry/Desktop/Project

# 运行自动构建脚本
./build_and_test.sh
```

3. **手动构建（可选）**
```bash
# 设置目标芯片
idf.py set-target esp32s3

# 构建项目
idf.py build

# 烧录固件
idf.py flash

# 启动监视器
idf.py monitor
```

### 🎯 首次使用

#### 第一步：系统启动
上电后，串口输出：
```
=== Distance Detection System ===
System needs calibration first!
Instructions:
1. Position yourself 50cm from camera
2. System will auto-start calibration when face detected
3. Stay still during 20-frame calibration
```

#### 第二步：标定过程
1. **准确定位**: 坐在距离摄像头 **50cm** 的位置
2. **保持正面**: 面部正对摄像头，无遮挡
3. **等待检测**: 系统检测到人脸后自动开始标定
4. **保持静止**: 标定过程约2-3秒，保持不动

#### 第三步：标定完成
看到以下信息表示标定成功：
```
=== CALIBRATION COMPLETED ===
Distance Detection System Ready
System is calibrated and monitoring face distance
Safe distance threshold: 30-33 cm
```

### 📊 实时监测

标定完成后，系统将持续监测：

- **✅ 安全距离 (>33cm)**
  ```
  ✅ Face distance is now safe. Distance: 45.2 cm
  ```

- **⚠️ 过近警告 (<30cm)**
  ```
  ⚠️ WARNING: FACE TOO CLOSE! Distance: 25.1 cm
  Please move back for eye safety!
  ```

### 🔧 高级配置

#### 重新标定
如需为新用户或新环境重新标定：

```cpp
// 在代码中调用
reset_distance_calibration();
start_distance_calibration();
```

或重启设备并删除NVS数据。

#### 调整阈值
修改 `face_distance_detector.hpp` 中的参数：

```cpp
static constexpr float ENTER_THRESHOLD_CM = 30.0f;  // 进入警告距离
static constexpr float EXIT_THRESHOLD_CM = 33.0f;   // 退出警告距离
```

#### 滤波参数
调整响应速度vs稳定性：

```cpp
static constexpr int FILTER_QUEUE_SIZE = 7;  // 增大=更稳定，减小=更快响应
```

### 🔍 调试与排错

#### 常见问题

**Q: 标定一直失败？**
- 检查光线是否充足
- 确认人脸清晰可见
- 验证距离准确为50cm
- 确保摄像头焦点清晰

**Q: 距离测量不准确？**
- 重新标定系统
- 检查摄像头是否移动
- 确认环境光线稳定

**Q: 频繁误报警？**
- 增大进入阈值
- 增大滤波队列长度
- 检查头部姿态是否过大

#### 调试输出

启用详细调试：
```cpp
// 在menuconfig中设置日志级别为DEBUG
esp_log_level_set("FaceDistanceDetector", ESP_LOG_DEBUG);
```

### 📈 性能优化

#### 内存优化
- 减小滤波队列大小
- 使用固定点数学替代浮点运算
- 优化数据结构内存对齐

#### 速度优化
- 降低人脸检测分辨率
- 减少滤波窗口大小
- 使用硬件加速功能

### 🔌 集成到其他项目

#### 基本集成
```cpp
#include "face_distance_detector.hpp"

FaceDistanceDetector detector;
detector.init();

// 在人脸检测循环中
face_distance_state_t state = detector.processFrame(face_results);
```

#### 高级集成
参考 `examples/face_distance_example.cpp` 中的完整示例。

### 📞 技术支持

- **文档**: 查看 `README_FaceDistance.md`
- **示例**: 参考 `examples/` 目录
- **论坛**: www.openedv.com
- **官网**: www.alientek.com

### 🎉 开始使用

现在你已经准备好使用这个强大的人脸距离检测系统了！

```bash
# 立即开始
./build_and_test.sh
```

---

*系统特色：姿态感知 | 鲁棒滤波 | 个性化标定 | 实时监测*
