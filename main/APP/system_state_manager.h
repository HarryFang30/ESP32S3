/**
 ****************************************************************************************************
 * @file        system_state_manager.h
 * @author      AI Assistant
 * @version     V1.0
 * @date        2025-07-25
 * @brief       系统状态管理器 - 协调人脸识别和拍照上传
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 */

#ifndef __SYSTEM_STATE_MANAGER_H
#define __SYSTEM_STATE_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>
#include "esp_camera.h"
#include "photo_uploader.h"  // 包含segmented_photo_t定义

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 系统工作模式枚举
 */
typedef enum {
    SYSTEM_MODE_FACE_DETECTION = 0,    /*!< 人脸识别模式 */
    SYSTEM_MODE_PHOTO_UPLOAD = 1,      /*!< 拍照上传模式 */
    SYSTEM_MODE_TRANSITIONING = 2      /*!< 模式切换中 */
} system_mode_t;

/**
 * @brief 系统状态管理器结构体
 */
typedef struct {
    system_mode_t current_mode;        /*!< 当前工作模式 */
    bool photo_upload_requested;       /*!< 是否请求拍照上传 */
    bool face_detection_paused;        /*!< 人脸识别是否暂停 */
    uint32_t mode_switch_timestamp;    /*!< 模式切换时间戳 */
    uint32_t alarm_start_timestamp;    /*!< 报警自动关闭定时器开始时间戳 */
    bool alarm_timeout_enabled;        /*!< 是否启用报警自动关闭定时器 */
    bool photo_upload_in_progress;     /*!< 拍照上传是否正在进行 */
    segmented_photo_t *captured_photo;  /*!< 保存的实时照片（分段数据指针） */
} system_state_manager_t;

/* 全局状态管理器 */
extern system_state_manager_t g_system_state;

/**
 * @brief 初始化系统状态管理器
 * @retval ESP_OK 成功
 * @retval ESP_FAIL 失败
 */
esp_err_t system_state_manager_init(void);

/**
 * @brief 请求切换到拍照上传模式
 * @note 这将暂停人脸识别，启动拍照上传流程
 */
void system_request_photo_upload(void);

/**
 * @brief 请求拍照上传（使用已保存的实时照片）
 * @note 使用距离检测时已保存的实时照片进行上传
 */
void system_request_photo_upload_with_saved_photo(void);

/**
 * @brief 启动报警自动关闭定时器
 * @param timeout_ms 定时时间（毫秒），时间到后自动停止报警器
 */
void system_start_alarm_timeout(uint32_t timeout_ms);

/**
 * @brief 停止报警自动关闭定时器
 */
void system_stop_alarm_timeout(void);

/**
 * @brief 获取是否允许LCD显示
 * @retval true 允许LCD显示
 * @retval false LCD显示被暂停（拍照上传中）
 */
bool system_can_update_lcd(void);

/**
 * @brief 完成拍照上传，切换回人脸识别模式
 */
void system_photo_upload_complete(void);

/**
 * @brief 获取当前系统模式
 * @retval system_mode_t 当前模式
 */
system_mode_t system_get_current_mode(void);

/**
 * @brief 检查是否可以进行人脸识别
 * @retval true 可以进行人脸识别
 * @retval false 人脸识别被暂停
 */
bool system_can_do_face_detection(void);

/**
 * @brief 检查是否需要进行拍照上传
 * @retval true 需要拍照上传
 * @retval false 不需要拍照上传
 */
bool system_need_photo_upload(void);

/**
 * @brief 系统状态管理任务处理函数
 * @note 在主循环中调用，处理模式切换逻辑
 */
void system_state_task_handler(void);

/* 主任务看门狗相关外部声明 */
extern bool main_watchdog_active;
void safe_watchdog_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* __SYSTEM_STATE_MANAGER_H */
