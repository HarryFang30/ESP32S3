/**
 ****************************************************************************************************
 * @file        face_distance_c_interface.h
 * @author      AI Assistant
 * @version     V1.0
 * @date        2025-07-24
 * @brief       人脸距离检测系统C接口
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 */

#ifndef __FACE_DISTANCE_C_INTERFACE_H
#define __FACE_DISTANCE_C_INTERFACE_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 系统状态枚举
 */
typedef enum {
    FACE_DISTANCE_SAFE = 0,     /*!< 安全距离状态 */
    FACE_DISTANCE_TOO_CLOSE = 1 /*!< 过近状态 */
} face_distance_state_t;

/* 全局距离检测器指针 */
extern void* g_distance_detector_handle;

/**
 * @brief 初始化距离检测系统
 * @retval ESP_OK 成功
 * @retval ESP_FAIL 失败
 */
esp_err_t init_distance_detection_system(void);

/**
 * @brief 销毁距离检测系统
 */
void deinit_distance_detection_system(void);

/**
 * @brief 检查是否已标定
 * @retval true 已标定
 * @retval false 未标定
 */
bool is_distance_calibrated(void);

/**
 * @brief 开始距离标定
 */
void start_distance_calibration(void);

/**
 * @brief 重置距离标定
 */
void reset_distance_calibration(void);

/**
 * @brief 处理人脸检测结果进行距离检测
 * @param results 人脸检测结果列表
 * @param camera_frame 当前摄像头帧（用于拍照上传）
 */
void handle_distance_detection_c(void* results, void* camera_frame);

/**
 * @brief 处理没有检测到人脸的情况
 * 重置状态并关闭蜂鸣器
 */
void handle_no_face_detected_c(void);

#ifdef __cplusplus
}
#endif

#endif /* __FACE_DISTANCE_C_INTERFACE_H */
