#include "image_scaler.h"
#include <stdio.h>

/**
 * @brief       RGB565图像缩放函数（最近邻插值，速度更快）
 * @param       src_buf: 源图像缓冲区（RGB565格式）
 * @param       src_width: 源图像宽度
 * @param       src_height: 源图像高度
 * @param       dst_buf: 目标图像缓冲区（RGB565格式）
 * @param       dst_width: 目标图像宽度
 * @param       dst_height: 目标图像高度
 * @retval      0: 成功, -1: 失败
 */
int scale_rgb565_nearest(const uint16_t* src_buf, int src_width, int src_height,
                        uint16_t* dst_buf, int dst_width, int dst_height)
{
    if (!src_buf || !dst_buf || src_width <= 0 || src_height <= 0 || 
        dst_width <= 0 || dst_height <= 0) {
        return -1;
    }

    float x_ratio = (float)src_width / dst_width;
    float y_ratio = (float)src_height / dst_height;

    for (int i = 0; i < dst_height; i++) {
        for (int j = 0; j < dst_width; j++) {
            int px = (int)(j * x_ratio);
            int py = (int)(i * y_ratio);
            
            // 边界检查
            if (px >= src_width) px = src_width - 1;
            if (py >= src_height) py = src_height - 1;
            
            dst_buf[i * dst_width + j] = src_buf[py * src_width + px];
        }
    }

    return 0;
}

/**
 * @brief       RGB565图像缩放函数（双线性插值）
 * @param       src_buf: 源图像缓冲区（RGB565格式）
 * @param       src_width: 源图像宽度
 * @param       src_height: 源图像高度
 * @param       dst_buf: 目标图像缓冲区（RGB565格式）
 * @param       dst_width: 目标图像宽度
 * @param       dst_height: 目标图像高度
 * @retval      0: 成功, -1: 失败
 */
int scale_rgb565_bilinear(const uint16_t* src_buf, int src_width, int src_height,
                         uint16_t* dst_buf, int dst_width, int dst_height)
{
    if (!src_buf || !dst_buf || src_width <= 0 || src_height <= 0 || 
        dst_width <= 0 || dst_height <= 0) {
        return -1;
    }

    float x_ratio = (float)(src_width - 1) / dst_width;
    float y_ratio = (float)(src_height - 1) / dst_height;

    for (int i = 0; i < dst_height; i++) {
        for (int j = 0; j < dst_width; j++) {
            float x_src = j * x_ratio;
            float y_src = i * y_ratio;
            
            int x = (int)x_src;
            int y = (int)y_src;
            
            float x_diff = x_src - x;
            float y_diff = y_src - y;
            
            int x1 = x, y1 = y;
            int x2 = x + 1, y2 = y + 1;
            
            // 边界检查
            if (x2 >= src_width) x2 = src_width - 1;
            if (y2 >= src_height) y2 = src_height - 1;
            
            // 获取四个邻近像素
            uint16_t p1 = src_buf[y1 * src_width + x1];
            uint16_t p2 = src_buf[y1 * src_width + x2];
            uint16_t p3 = src_buf[y2 * src_width + x1];
            uint16_t p4 = src_buf[y2 * src_width + x2];
            
            // 分离RGB分量
            uint8_t r1 = (p1 >> 11) & 0x1F;
            uint8_t g1 = (p1 >> 5) & 0x3F;
            uint8_t b1 = p1 & 0x1F;
            
            uint8_t r2 = (p2 >> 11) & 0x1F;
            uint8_t g2 = (p2 >> 5) & 0x3F;
            uint8_t b2 = p2 & 0x1F;
            
            uint8_t r3 = (p3 >> 11) & 0x1F;
            uint8_t g3 = (p3 >> 5) & 0x3F;
            uint8_t b3 = p3 & 0x1F;
            
            uint8_t r4 = (p4 >> 11) & 0x1F;
            uint8_t g4 = (p4 >> 5) & 0x3F;
            uint8_t b4 = p4 & 0x1F;
            
            // 双线性插值
            float r = r1 * (1 - x_diff) * (1 - y_diff) + 
                     r2 * x_diff * (1 - y_diff) + 
                     r3 * (1 - x_diff) * y_diff + 
                     r4 * x_diff * y_diff;
                     
            float g = g1 * (1 - x_diff) * (1 - y_diff) + 
                     g2 * x_diff * (1 - y_diff) + 
                     g3 * (1 - x_diff) * y_diff + 
                     g4 * x_diff * y_diff;
                     
            float b = b1 * (1 - x_diff) * (1 - y_diff) + 
                     b2 * x_diff * (1 - y_diff) + 
                     b3 * (1 - x_diff) * y_diff + 
                     b4 * x_diff * y_diff;
            
            // 组合RGB565
            uint16_t result = ((uint16_t)(r + 0.5) << 11) | 
                             ((uint16_t)(g + 0.5) << 5) | 
                             (uint16_t)(b + 0.5);
            
            dst_buf[i * dst_width + j] = result;
        }
    }

    return 0;
}
