#!/usr/bin/env python3
"""
RGB565 to Image Converter
将ESP32上传的RGB565原始数据转换为可显示的图像格式
"""

import os
import struct
import numpy as np
from PIL import Image
import argparse
from datetime import datetime

def rgb565_to_rgb888(rgb565_data):
    """
    将RGB565格式数据转换为RGB888格式
    
    Args:
        rgb565_data: 字节数组，包含RGB565格式的像素数据
        
    Returns:
        numpy array: RGB888格式的像素数组 (height, width, 3)
    """
    # RGB565: 5位红色 + 6位绿色 + 5位蓝色
    # ESP32摄像头使用大端序格式
    
    pixel_count = len(rgb565_data) // 2
    rgb888_data = []
    
    for i in range(0, len(rgb565_data), 2):
        # 读取2字节的RGB565数据（大端序）
        pixel_565 = struct.unpack('>H', rgb565_data[i:i+2])[0]
        
        # 提取RGB分量
        r5 = (pixel_565 >> 11) & 0x1F  # 高5位
        g6 = (pixel_565 >> 5) & 0x3F   # 中6位
        b5 = pixel_565 & 0x1F          # 低5位
        
        # 转换为8位RGB (扩展位数)
        r8 = (r5 * 255) // 31
        g8 = (g6 * 255) // 63
        b8 = (b5 * 255) // 31
        
        # 使用正确的RGB顺序（大端序）
        rgb888_data.extend([r8, g8, b8])
    
    return np.array(rgb888_data, dtype=np.uint8)

def convert_rgb_file(input_file, output_file=None, width=400, height=600):
    """
    转换RGB565文件为标准图像格式
    
    Args:
        input_file: 输入的RGB565文件路径
        output_file: 输出图像文件路径（可选）
        width: 图像宽度
        height: 图像高度
    """
    try:
        # 读取RGB565原始数据
        with open(input_file, 'rb') as f:
            rgb565_data = f.read()
        
        expected_size = width * height * 2
        if len(rgb565_data) != expected_size:
            print(f"警告: 文件大小不匹配")
            print(f"期望大小: {expected_size} 字节 ({width}x{height}x2)")
            print(f"实际大小: {len(rgb565_data)} 字节")
            
            # 尝试推断正确的尺寸
            total_pixels = len(rgb565_data) // 2
            possible_sizes = [
                (800, 600), (600, 800),  # SVGA 800x600
                (400, 600), (600, 400),  # SVGA variants
                (320, 240), (240, 320),  # QVGA variants
                (160, 120), (120, 160),  # QQVGA variants
            ]
            
            for w, h in possible_sizes:
                if w * h == total_pixels:
                    width, height = w, h
                    print(f"自动检测尺寸: {width}x{height}")
                    break
            else:
                # 如果找不到标准尺寸，尝试正方形
                import math
                side = int(math.sqrt(total_pixels))
                if side * side == total_pixels:
                    width, height = side, side
                    print(f"检测到正方形图像: {width}x{height}")
                else:
                    print(f"无法确定图像尺寸，使用默认值: {width}x{height}")
        
        # 转换RGB565到RGB888
        rgb888_data = rgb565_to_rgb888(rgb565_data)
        
        # 重塑为图像数组
        try:
            image_array = rgb888_data.reshape(height, width, 3)
        except ValueError as e:
            print(f"重塑数组失败: {e}")
            print(f"数据长度: {len(rgb888_data)}, 期望: {height * width * 3}")
            return None
        
        # 创建PIL图像
        image = Image.fromarray(image_array, 'RGB')
        
        # 生成输出文件名
        if output_file is None:
            base_name = os.path.splitext(os.path.basename(input_file))[0]
            output_file = os.path.join(os.path.dirname(input_file), f"{base_name}_converted.jpg")
        
        # 保存图像
        image.save(output_file, 'JPEG', quality=95)
        
        print(f"✅ 转换成功!")
        print(f"输入文件: {input_file}")
        print(f"输出文件: {output_file}")
        print(f"图像尺寸: {width}x{height}")
        print(f"文件大小: {len(rgb565_data)} -> {os.path.getsize(output_file)} 字节")
        
        return output_file
        
    except Exception as e:
        print(f"❌ 转换失败: {e}")
        return None

def batch_convert_uploads(uploads_dir="uploads"):
    """
    批量转换uploads目录中的所有RGB文件
    """
    if not os.path.exists(uploads_dir):
        print(f"目录不存在: {uploads_dir}")
        return
    
    print(f"🔄 开始批量转换 {uploads_dir} 目录中的文件...")
    
    converted_count = 0
    for filename in os.listdir(uploads_dir):
        file_path = os.path.join(uploads_dir, filename)
        
        # 跳过已经转换的文件
        if filename.endswith('_converted.jpg') or not os.path.isfile(file_path):
            continue
        
        # 检查文件大小（RGB565文件应该比较大）
        file_size = os.path.getsize(file_path)
        if file_size < 10000:  # 小于10KB的文件可能不是RGB数据
            print(f"跳过小文件: {filename} ({file_size} 字节)")
            continue
        
        print(f"\n处理文件: {filename}")
        result = convert_rgb_file(file_path)
        if result:
            converted_count += 1
    
    print(f"\n✅ 批量转换完成，共转换 {converted_count} 个文件")

def main():
    parser = argparse.ArgumentParser(description='RGB565转图像工具')
    parser.add_argument('--file', '-f', help='单个文件转换')
    parser.add_argument('--batch', '-b', action='store_true', help='批量转换uploads目录')
    parser.add_argument('--width', '-w', type=int, default=800, help='图像宽度（默认800）')
    parser.add_argument('--height', type=int, default=600, help='图像高度（默认600）')
    parser.add_argument('--output', '-o', help='输出文件路径')
    
    args = parser.parse_args()
    
    if args.file:
        convert_rgb_file(args.file, args.output, args.width, args.height)
    elif args.batch:
        batch_convert_uploads()
    else:
        # 默认批量转换
        batch_convert_uploads()

if __name__ == "__main__":
    main()
