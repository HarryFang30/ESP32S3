# ESP32-S3 Face Distance Detection System

A comprehensive face distance detection system based on ESP32-S3 with real-time monitoring, photo upload, and alarm functionality.

## 🌟 Features

- **Real-time Face Detection**: Uses ESP32-WHO AI library for accurate face detection
- **Distance Monitoring**: Measures face distance from camera and provides safety warnings
- **Smart Alarm System**: 3-second auto-stop buzzer alarm when face is too close (<48cm)
- **Photo Upload**: Automatic photo capture and upload to web server when alarm triggers
- **Web Interface**: Local web server for monitoring and photo viewing
- **LCD Display**: Real-time visual feedback on 1.3/2.4 inch SPI LCD
- **Segmented Memory Management**: Efficient handling of large photo data using PSRAM

## 🛠️ Hardware Requirements

### Development Board
- **Platform**: 正点原子 ESP32-S3 Development Board
- **Camera**: OV2640/OV5640 camera module
- **Display**: 1.3/2.4 inch SPI LCD module
- **IO Expander**: XL9555
- **Audio**: Buzzer for alarm notifications

### Pin Configuration

#### LED
- LED(RED) - IO1

#### XL9555 IO Expander
- INT --> IO0
- SDA --> IO41
- CLK --> IO42

#### Camera (OV2640/OV5640)
- OV_D0 --> IO4
- OV_D1 --> IO5
- OV_D2 --> IO6
- OV_D3 --> IO7
- OV_D4 --> IO15
- OV_D5 --> IO16
- OV_D6 --> IO17
- OV_D7 --> IO18
- OV_VSYNC --> IO47
- OV_HREF --> IO48
- OV_PCLK --> IO45
- OV_SCL --> IO38
- OV_SDA --> IO39
- OV_PWDN --> IO扩展4(OV_PWDN)
- OV_RESET --> IO扩展5(OV_RESET)

## 🚀 Quick Start

### Prerequisites
- ESP-IDF v5.x
- Python 3.8+
- Git

### Build Instructions

1. **Clone the repository**
   ```bash
   git clone <repository-url>
   cd Project
   ```

2. **Set up ESP-IDF environment**
   ```bash
   . $HOME/esp/esp-idf/export.sh
   ```

3. **Configure the project**
   ```bash
   idf.py menuconfig
   ```

4. **Build the project**
   ```bash
   idf.py build
   ```

5. **Flash to device**
   ```bash
   idf.py -p /dev/ttyUSB0 flash monitor
   ```

### WiFi Configuration
Update the WiFi credentials in `main/APP/photo_uploader.h`:
```c
#define WIFI_SSID "YourWiFiSSID"
#define WIFI_PASSWORD "YourWiFiPassword"
#define SERVER_URL "http://your-server.com/upload"
```

## 📋 System Operation

### Distance Calibration
1. Position yourself 50cm from the camera
2. Use `start_distance_calibration()` function
3. Stay still for 20 frames during calibration
4. System will automatically complete calibration

### Normal Operation
- **Safe Distance**: > 48cm - Normal operation
- **Too Close**: < 48cm - Triggers alarm and photo capture
- **No Face**: Automatic alarm reset when face leaves view

### Alarm System
- **Duration**: 3 seconds auto-stop
- **Trigger**: Face distance < 48cm
- **Actions**: Buzzer alarm + Photo capture + Upload to server

## 🗂️ Project Structure

```
Project/
├── main/                    # Main application
│   ├── main.c              # Main program entry
│   └── APP/                # Application modules
│       ├── system_state_manager.c  # System state coordination
│       ├── photo_uploader.c        # WiFi and photo upload
│       ├── face_distance_c_interface.cpp  # Distance detection
│       └── ...
├── components/             # Hardware abstraction layers
│   ├── BSP/               # Board support packages
│   ├── esp32-camera/      # Camera driver
│   ├── esp-dl/            # Deep learning library
│   └── ...
├── posture_monitor_local/  # Web server for monitoring
├── examples/              # Example code
└── build/                 # Build output (ignored)
```

## 🌐 Web Interface

The system includes a local web server accessible via ESP32's IP address:
- Real-time photo viewing
- Upload history
- System status monitoring

Access at: `http://[ESP32_IP]:80`

## 🔧 Advanced Features

### Segmented Photo Management
- Efficient memory usage with PSRAM
- Large photo handling (up to 1MB)
- Automatic memory cleanup

### Performance Optimization
- Aggressive upload parameters for speed
- Network performance testing
- WiFi power optimization
- Keep-Alive connections

### System Coordination
- Task synchronization for camera access
- Watchdog management during uploads
- State machine for mode switching

## 📊 Performance Metrics

- **Photo Capture**: < 500ms
- **Upload Speed**: Optimized for network conditions
- **Memory Usage**: PSRAM-optimized for large images
- **Alarm Response**: Immediate (<100ms)

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to the branch
5. Create a Pull Request

## 📄 License

Copyright (c) 2020-2032, 广州市星翼电子科技有限公司（正点原子）

## 📞 Support

- **Company**: 广州市星翼电子科技有限公司（正点原子）
- **Phone**: 020-38271790
- **Website**: www.alientek.com
- **Store**: zhengdianyuanzi.tmall.com
- **Forum**: http://www.openedv.com/forum.php

## 📈 Version History

See `CHANGELOG.md` for detailed version history and updates.
 * 最新资料：www.openedv.com/docs/index.html
 * 在线视频：www.yuanzige.com
 * B 站视频：space.bilibili.com/394620890
 * 公 众 号：mp.weixin.qq.com/s/y--mG3qQT8gop0VRuER9bw
 * 抖    音：douyin.com/user/MS4wLjABAAAAi5E95JUBpqsW5kgMEaagtIITIl15hAJvMO8vQMV1tT6PEsw-V5HbkNLlLMkFf1Bd
 ***********************************************************************************************************
 */