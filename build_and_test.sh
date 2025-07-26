#!/bin/bash

# 人脸距离检测系统构建和测试脚本
# Face Distance Detection System Build and Test Script

echo "======================================"
echo "Face Distance Detection System"
echo "Build and Test Script"
echo "======================================"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 检查ESP-IDF环境
check_esp_idf() {
    echo -e "${BLUE}[INFO]${NC} Checking ESP-IDF environment..."
    
    if [ -z "$IDF_PATH" ]; then
        echo -e "${RED}[ERROR]${NC} ESP-IDF environment not found!"
        echo "Please install and source ESP-IDF first:"
        echo ". \$HOME/esp/esp-idf/export.sh"
        exit 1
    fi
    
    echo -e "${GREEN}[OK]${NC} ESP-IDF found at: $IDF_PATH"
}

# 检查项目结构
check_project_structure() {
    echo -e "${BLUE}[INFO]${NC} Checking project structure..."
    
    required_files=(
        "main/APP/face_distance_detector.hpp"
        "main/APP/face_distance_detector.cpp"
        "main/APP/esp_face_detection.hpp"
        "main/APP/esp_face_detection.cpp"
        "main/main.c"
        "CMakeLists.txt"
    )
    
    for file in "${required_files[@]}"; do
        if [ ! -f "$file" ]; then
            echo -e "${RED}[ERROR]${NC} Missing required file: $file"
            exit 1
        fi
    done
    
    echo -e "${GREEN}[OK]${NC} All required files found"
}

# 清理构建
clean_build() {
    echo -e "${BLUE}[INFO]${NC} Cleaning previous build..."
    
    if [ -d "build" ]; then
        rm -rf build
        echo -e "${GREEN}[OK]${NC} Build directory cleaned"
    fi
}

# 配置项目
configure_project() {
    echo -e "${BLUE}[INFO]${NC} Configuring project..."
    
    idf.py set-target esp32s3
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}[OK]${NC} Target set to ESP32-S3"
    else
        echo -e "${RED}[ERROR]${NC} Failed to set target"
        exit 1
    fi
}

# 构建项目
build_project() {
    echo -e "${BLUE}[INFO]${NC} Building project..."
    
    idf.py build
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}[OK]${NC} Build successful"
    else
        echo -e "${RED}[ERROR]${NC} Build failed"
        exit 1
    fi
}

# 检查构建产物
check_build_artifacts() {
    echo -e "${BLUE}[INFO]${NC} Checking build artifacts..."
    
    if [ -f "build/main.elf" ]; then
        echo -e "${GREEN}[OK]${NC} Firmware ELF found"
        
        # 显示固件信息
        echo -e "${BLUE}[INFO]${NC} Firmware information:"
        idf.py size-components | head -20
        
    else
        echo -e "${RED}[ERROR]${NC} Firmware ELF not found"
        exit 1
    fi
}

# 烧录固件（可选）
flash_firmware() {
    echo -e "${YELLOW}[QUESTION]${NC} Do you want to flash the firmware? (y/N)"
    read -r response
    
    if [[ "$response" =~ ^[Yy]$ ]]; then
        echo -e "${BLUE}[INFO]${NC} Flashing firmware..."
        echo "Please connect your ESP32-S3 device and press Enter..."
        read -r
        
        idf.py flash
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}[OK]${NC} Firmware flashed successfully"
        else
            echo -e "${RED}[ERROR]${NC} Failed to flash firmware"
            exit 1
        fi
    fi
}

# 启动监视器（可选）
start_monitor() {
    echo -e "${YELLOW}[QUESTION]${NC} Do you want to start the serial monitor? (y/N)"
    read -r response
    
    if [[ "$response" =~ ^[Yy]$ ]]; then
        echo -e "${BLUE}[INFO]${NC} Starting serial monitor..."
        echo "Press Ctrl+] to exit monitor"
        sleep 2
        
        idf.py monitor
    fi
}

# 显示使用说明
show_usage_instructions() {
    echo ""
    echo "======================================"
    echo -e "${GREEN}BUILD COMPLETED SUCCESSFULLY!${NC}"
    echo "======================================"
    echo ""
    echo "Next steps:"
    echo "1. Flash the firmware to your ESP32-S3:"
    echo "   idf.py flash"
    echo ""
    echo "2. Open serial monitor to see output:"
    echo "   idf.py monitor"
    echo ""
    echo "3. Follow calibration instructions:"
    echo "   - Position face 50cm from camera"
    echo "   - Keep face visible and still"
    echo "   - Wait for calibration completion"
    echo ""
    echo "4. Test distance detection:"
    echo "   - Move closer/farther from camera"
    echo "   - Observe distance warnings"
    echo ""
    echo "======================================"
    echo "System Features:"
    echo "✓ Pose-aware distance measurement"
    echo "✓ Robust filtering and smoothing"
    echo "✓ Automatic calibration system"
    echo "✓ Persistent calibration storage"
    echo "✓ Real-time face distance monitoring"
    echo "======================================"
}

# 主函数
main() {
    echo "Starting build process..."
    
    # 执行构建步骤
    check_esp_idf
    check_project_structure
    clean_build
    configure_project
    build_project
    check_build_artifacts
    
    # 可选步骤
    flash_firmware
    start_monitor
    
    # 显示完成信息
    show_usage_instructions
}

# 错误处理
set -e
trap 'echo -e "${RED}[ERROR]${NC} Script failed at line $LINENO"' ERR

# 运行主函数
main "$@"
