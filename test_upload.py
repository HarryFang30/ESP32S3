#!/usr/bin/env python3
"""
测试照片上传功能的脚本
模拟ESP32发送照片到服务器
"""

import requests
import time
import os

# 服务器配置
SERVER_URL = "http://127.0.0.1:5001/upload"

def test_upload():
    """测试上传功能"""
    
    # 创建一个简单的测试图片数据（实际应用中这将是JPEG图片）
    test_image_data = b'\xff\xd8\xff\xe0\x00\x10JFIF\x00\x01\x01\x01\x00H\x00H\x00\x00\xff\xdb\x00C\x00\x08\x06\x06\x07\x06\x05\x08\x07\x07\x07\t\t\x08\n\x0c\x14\r\x0c\x0b\x0b\x0c\x19\x12\x13\x0f\x14\x1d\x1a\x1f\x1e\x1d\x1a\x1c\x1c $.\' ",#\x1c\x1c(7),01444\x1f\'9=82<.342\xff\xc0\x00\x11\x08\x00\x01\x00\x01\x01\x01\x11\x00\x02\x11\x01\x03\x11\x01\xff\xc4\x00\x14\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x08\xff\xc4\x00\x14\x10\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xda\x00\x0c\x03\x01\x00\x02\x11\x03\x11\x00\x3f\x00\xaa\xff\xd9'
    
    print("🧪 开始测试照片上传功能...")
    print(f"📡 服务器地址: {SERVER_URL}")
    
    try:
        # 发送POST请求模拟ESP32上传照片
        response = requests.post(
            SERVER_URL,
            data=test_image_data,
            headers={'Content-Type': 'application/octet-stream'},
            timeout=10
        )
        
        if response.status_code == 200:
            print("✅ 上传成功！服务器响应:", response.text)
            print("🌐 请在浏览器中查看 http://localhost:5001 来查看上传的图片")
            return True
        else:
            print(f"❌ 上传失败，状态码: {response.status_code}")
            print(f"响应内容: {response.text}")
            return False
            
    except requests.exceptions.RequestException as e:
        print(f"❌ 网络错误: {e}")
        return False
    except Exception as e:
        print(f"❌ 其他错误: {e}")
        return False

def test_multiple_uploads():
    """测试多次上传"""
    print("🔄 测试多次上传...")
    
    for i in range(3):
        print(f"\n📸 第 {i+1} 次上传:")
        success = test_upload()
        if success:
            print(f"✅ 第 {i+1} 次上传成功")
        else:
            print(f"❌ 第 {i+1} 次上传失败")
        
        # 等待2秒再进行下一次测试
        if i < 2:
            print("⏳ 等待2秒...")
            time.sleep(2)

if __name__ == "__main__":
    print("=" * 50)
    print("🎯 ESP32人脸监控系统 - 网站上传测试")
    print("=" * 50)
    
    # 检查服务器是否运行
    try:
        response = requests.get("http://127.0.0.1:5001", timeout=5)
        print("✅ 服务器正在运行")
    except:
        print("❌ 服务器未运行，请先启动服务器: python server.py")
        exit(1)
    
    # 运行测试
    test_multiple_uploads()
    
    print("\n" + "=" * 50)
    print("🎉 测试完成！请在浏览器中查看结果: http://localhost:5001")
    print("=" * 50)
