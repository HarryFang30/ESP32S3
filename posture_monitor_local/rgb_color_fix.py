#!/usr/bin/env python3
"""
RGB565 颜色修复工具
测试不同的字节序和颜色通道顺序
"""

import os
import struct
import numpy as np
from PIL import Image
import argparse

def rgb565_to_rgb888_v1(rgb565_data):
    """版本1: 小端序, RGB顺序"""
    rgb888_data = []
    for i in range(0, len(rgb565_data), 2):
        pixel_565 = struct.unpack('<H', rgb565_data[i:i+2])[0]
        r5 = (pixel_565 >> 11) & 0x1F
        g6 = (pixel_565 >> 5) & 0x3F
        b5 = pixel_565 & 0x1F
        r8 = (r5 * 255) // 31
        g8 = (g6 * 255) // 63
        b8 = (b5 * 255) // 31
        rgb888_data.extend([r8, g8, b8])
    return np.array(rgb888_data, dtype=np.uint8)

def rgb565_to_rgb888_v2(rgb565_data):
    """版本2: 大端序, RGB顺序"""
    rgb888_data = []
    for i in range(0, len(rgb565_data), 2):
        pixel_565 = struct.unpack('>H', rgb565_data[i:i+2])[0]
        r5 = (pixel_565 >> 11) & 0x1F
        g6 = (pixel_565 >> 5) & 0x3F
        b5 = pixel_565 & 0x1F
        r8 = (r5 * 255) // 31
        g8 = (g6 * 255) // 63
        b8 = (b5 * 255) // 31
        rgb888_data.extend([r8, g8, b8])
    return np.array(rgb888_data, dtype=np.uint8)

def rgb565_to_rgb888_v3(rgb565_data):
    """版本3: 小端序, BGR顺序"""
    rgb888_data = []
    for i in range(0, len(rgb565_data), 2):
        pixel_565 = struct.unpack('<H', rgb565_data[i:i+2])[0]
        r5 = (pixel_565 >> 11) & 0x1F
        g6 = (pixel_565 >> 5) & 0x3F
        b5 = pixel_565 & 0x1F
        r8 = (r5 * 255) // 31
        g8 = (g6 * 255) // 63
        b8 = (b5 * 255) // 31
        rgb888_data.extend([b8, g8, r8])  # BGR顺序
    return np.array(rgb888_data, dtype=np.uint8)

def rgb565_to_rgb888_v4(rgb565_data):
    """版本4: 大端序, BGR顺序"""
    rgb888_data = []
    for i in range(0, len(rgb565_data), 2):
        pixel_565 = struct.unpack('>H', rgb565_data[i:i+2])[0]
        r5 = (pixel_565 >> 11) & 0x1F
        g6 = (pixel_565 >> 5) & 0x3F
        b5 = pixel_565 & 0x1F
        r8 = (r5 * 255) // 31
        g8 = (g6 * 255) // 63
        b8 = (b5 * 255) // 31
        rgb888_data.extend([b8, g8, r8])  # BGR顺序
    return np.array(rgb888_data, dtype=np.uint8)

def rgb565_to_rgb888_v5(rgb565_data):
    """版本5: 直接字节交换"""
    rgb888_data = []
    for i in range(0, len(rgb565_data), 2):
        # 交换字节顺序
        byte1 = rgb565_data[i+1]
        byte0 = rgb565_data[i]
        pixel_565 = (byte1 << 8) | byte0
        
        r5 = (pixel_565 >> 11) & 0x1F
        g6 = (pixel_565 >> 5) & 0x3F
        b5 = pixel_565 & 0x1F
        r8 = (r5 * 255) // 31
        g8 = (g6 * 255) // 63
        b8 = (b5 * 255) // 31
        rgb888_data.extend([r8, g8, b8])
    return np.array(rgb888_data, dtype=np.uint8)

def rgb565_to_rgb888_v6(rgb565_data):
    """版本6: 使用numpy快速转换"""
    # 将数据重新解释为16位整数数组
    pixels_565 = np.frombuffer(rgb565_data, dtype=np.uint16)
    
    # 提取RGB分量
    r5 = (pixels_565 >> 11) & 0x1F
    g6 = (pixels_565 >> 5) & 0x3F
    b5 = pixels_565 & 0x1F
    
    # 转换为8位
    r8 = ((r5 * 255) // 31).astype(np.uint8)
    g8 = ((g6 * 255) // 63).astype(np.uint8)
    b8 = ((b5 * 255) // 31).astype(np.uint8)
    
    # 组合RGB通道
    rgb888 = np.stack([r8, g8, b8], axis=-1)
    return rgb888.flatten()

def test_all_versions(input_file, width=800, height=600):
    """测试所有转换版本"""
    with open(input_file, 'rb') as f:
        rgb565_data = f.read()
    
    versions = [
        ("v1_little_rgb", rgb565_to_rgb888_v1),
        ("v2_big_rgb", rgb565_to_rgb888_v2),
        ("v3_little_bgr", rgb565_to_rgb888_v3),
        ("v4_big_bgr", rgb565_to_rgb888_v4),
        ("v5_swap_rgb", rgb565_to_rgb888_v5),
        ("v6_numpy_rgb", rgb565_to_rgb888_v6),
    ]
    
    base_name = os.path.splitext(os.path.basename(input_file))[0]
    
    for version_name, convert_func in versions:
        try:
            print(f"测试 {version_name}...")
            rgb888_data = convert_func(rgb565_data)
            
            # 重塑为图像数组
            image_array = rgb888_data.reshape(height, width, 3)
            
            # 创建PIL图像
            image = Image.fromarray(image_array, 'RGB')
            
            # 保存图像
            output_file = f"uploads/{base_name}_{version_name}.jpg"
            image.save(output_file, 'JPEG', quality=95)
            
            print(f"✅ {version_name} 转换成功: {output_file}")
            
        except Exception as e:
            print(f"❌ {version_name} 转换失败: {e}")

def main():
    parser = argparse.ArgumentParser(description='RGB565颜色修复测试工具')
    parser.add_argument('--file', '-f', required=True, help='输入的RGB565文件')
    parser.add_argument('--width', '-w', type=int, default=800, help='图像宽度')
    parser.add_argument('--height', type=int, default=600, help='图像高度')
    
    args = parser.parse_args()
    
    test_all_versions(args.file, args.width, args.height)

if __name__ == "__main__":
    main()
