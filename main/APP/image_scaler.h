#ifndef __IMAGE_SCALER_H__
#define __IMAGE_SCALER_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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
                         uint16_t* dst_buf, int dst_width, int dst_height);

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
                        uint16_t* dst_buf, int dst_width, int dst_height);

#ifdef __cplusplus
}
#endif

#endif /* __IMAGE_SCALER_H__ */
