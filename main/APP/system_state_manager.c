/**
 ****************************************************************************************************
 * @file        system_state_manager.c
 * @author      AI Assistant
 * @version     V1.0
 * @date        2025-07-25
 * @brief       系统状态管理器实现 - 协调人脸识别和拍照上传
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 */

#include "system_state_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "photo_uploader.h"
#include "buzzer.h"
#include "esp_face_detection.hpp"
#include <inttypes.h>

static const char *TAG = "SystemStateMgr";

/* 全局状态管理器实例 */
system_state_manager_t g_system_state = {0};

/**
 * @brief 初始化系统状态管理器
 */
esp_err_t system_state_manager_init(void)
{
    g_system_state.current_mode = SYSTEM_MODE_FACE_DETECTION;
    g_system_state.photo_upload_requested = false;
    g_system_state.face_detection_paused = false;
    g_system_state.mode_switch_timestamp = 0;
    g_system_state.alarm_start_timestamp = 0;
    g_system_state.alarm_timeout_enabled = false;
    g_system_state.photo_upload_in_progress = false;
    g_system_state.captured_photo = NULL;  // 初始化实时照片指针
    
    ESP_LOGI(TAG, "System state manager initialized - starting in face detection mode");
    return ESP_OK;
}

/**
 * @brief 请求切换到拍照上传模式（新流程：分段安全数据复制）
 */
void system_request_photo_upload(void)
{
    if (g_system_state.current_mode == SYSTEM_MODE_FACE_DETECTION && 
        !g_system_state.photo_upload_requested && 
        !g_system_state.photo_upload_in_progress) {
        
        ESP_LOGI(TAG, "📸 Photo upload requested - will pause AI tasks first");
        printf("📸 Photo upload requested - pausing AI tasks for safe camera access...\r\n");
        
        // 设置状态切换标志，让AI任务自行暂停
        g_system_state.photo_upload_requested = true;
        g_system_state.face_detection_paused = true;
        g_system_state.current_mode = SYSTEM_MODE_TRANSITIONING;
        g_system_state.mode_switch_timestamp = esp_timer_get_time() / 1000; // 转换为毫秒
        
        printf("🔄 Switching to transitioning mode - AI tasks will pause automatically...\r\n");
        
    } else {
        ESP_LOGW(TAG, "Photo upload already requested or in progress (mode: %d, requested: %d, in_progress: %d)", 
                 g_system_state.current_mode, 
                 g_system_state.photo_upload_requested,
                 g_system_state.photo_upload_in_progress);
    }
}

/**
 * @brief 请求拍照上传（使用已保存的实时照片）
 */
void system_request_photo_upload_with_saved_photo(void)
{
    if (g_system_state.current_mode == SYSTEM_MODE_FACE_DETECTION && 
        !g_system_state.photo_upload_requested && 
        !g_system_state.photo_upload_in_progress) {
        
        if (g_system_state.captured_photo) {
            ESP_LOGI(TAG, "📸 Photo upload requested with pre-saved real-time photo");
            printf("📸 Using pre-saved real-time photo for upload...\r\n");
            
            // 直接开始模式切换流程，无需重新拍照
            printf("🖥️  Pausing LCD display for photo upload...\r\n");
            g_system_state.photo_upload_requested = true;
            g_system_state.face_detection_paused = true;
            g_system_state.current_mode = SYSTEM_MODE_TRANSITIONING;
            g_system_state.mode_switch_timestamp = esp_timer_get_time() / 1000; // 转换为毫秒
            
            printf("🔄 Switching to photo upload mode with pre-saved photo...\r\n");
        } else {
            ESP_LOGW(TAG, "No pre-saved photo available, falling back to regular safe copy photo upload");
            system_request_photo_upload();
        }
    } else {
        ESP_LOGW(TAG, "Photo upload already requested or in progress (mode: %d, requested: %d, in_progress: %d)", 
                 g_system_state.current_mode, 
                 g_system_state.photo_upload_requested,
                 g_system_state.photo_upload_in_progress);
    }
}

/**
 * @brief 完成拍照上传，切换回人脸识别模式
 */
void system_photo_upload_complete(void)
{
    ESP_LOGI(TAG, "📸 Photo upload completed - switching back to face detection");
    g_system_state.photo_upload_requested = false;
    g_system_state.face_detection_paused = false;
    g_system_state.current_mode = SYSTEM_MODE_FACE_DETECTION;
    g_system_state.mode_switch_timestamp = esp_timer_get_time() / 1000;
    
    // 清理可能残留的分段照片
    if (g_system_state.captured_photo) {
        ESP_LOGI(TAG, "Cleaning up remaining segmented photo");
        release_segmented_photo(g_system_state.captured_photo);
        g_system_state.captured_photo = NULL;
    }
    
    printf("🔄 Switching back to face detection mode...\r\n");
}

/**
 * @brief 报警器自动关闭任务
 */
static void alarm_auto_stop_task(void *pvParameters)
{
    uint32_t delay_ms = (uint32_t)pvParameters;
    
    ESP_LOGI(TAG, "⏰ Alarm auto-stop task started - will stop buzzer in %" PRIu32 " ms", delay_ms);
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    
    ESP_LOGW(TAG, "⏰ Alarm auto-stop timer expired - stopping buzzer now");
    printf("⏰ %d-second alarm completed - stopping buzzer\r\n", (int)(delay_ms/1000));
    buzzer_alarm(0);
    g_system_state.alarm_timeout_enabled = false;
    
    ESP_LOGI(TAG, "⏰ Alarm auto-stop task completed and deleted");
    vTaskDelete(NULL); // 删除自己
}

/**
 * @brief 启动报警自动关闭定时器
 */
void system_start_alarm_timeout(uint32_t timeout_ms)
{
    g_system_state.alarm_start_timestamp = esp_timer_get_time() / 1000;
    g_system_state.alarm_timeout_enabled = true;
    
    // 创建一个专门的任务来处理报警自动关闭
    xTaskCreate(alarm_auto_stop_task, "alarm_stop", 2048, (void*)timeout_ms, 5, NULL);
    
    ESP_LOGI(TAG, "Alarm auto-stop timer started: %" PRIu32 " ms (using dedicated task)", timeout_ms);
}

/**
 * @brief 停止报警自动关闭定时器
 */
void system_stop_alarm_timeout(void)
{
    g_system_state.alarm_timeout_enabled = false;
    ESP_LOGD(TAG, "Alarm timeout stopped");
}

/**
 * @brief 获取是否允许LCD显示
 */
bool system_can_update_lcd(void)
{
    // 在拍照上传期间禁用LCD显示，避免SPI冲突
    return (g_system_state.current_mode == SYSTEM_MODE_FACE_DETECTION);
}

/**
 * @brief 获取当前系统模式
 */
system_mode_t system_get_current_mode(void)
{
    return g_system_state.current_mode;
}

/**
 * @brief 检查是否可以进行人脸识别
 */
bool system_can_do_face_detection(void)
{
    return (g_system_state.current_mode == SYSTEM_MODE_FACE_DETECTION && 
            !g_system_state.face_detection_paused);
}

/**
 * @brief 检查是否需要进行拍照上传
 */
bool system_need_photo_upload(void)
{
    return g_system_state.photo_upload_requested;
}

/**
 * @brief 系统状态管理任务处理函数
 */
void system_state_task_handler(void)
{
    static uint32_t last_log_time = 0;
    uint32_t current_time = esp_timer_get_time() / 1000; // 转换为毫秒
    
    // 报警自动关闭现在由专门的任务处理，这里不需要检查了
    
    switch (g_system_state.current_mode) {
        case SYSTEM_MODE_FACE_DETECTION:
            // 正常人脸识别模式，无需特殊处理
            break;
            
        case SYSTEM_MODE_TRANSITIONING:
            // 模式切换中，检查是否需要执行拍照上传
            if (g_system_state.photo_upload_requested && !g_system_state.photo_upload_in_progress) {
                // 确保AI任务有足够时间暂停，避免摄像头资源冲突
                uint32_t elapsed_time = current_time - g_system_state.mode_switch_timestamp;
                if (elapsed_time < 500) { // 等待500ms让AI任务完全暂停
                    ESP_LOGD(TAG, "Waiting for AI tasks to pause completely... (%d ms elapsed)", (int)elapsed_time);
                    break;
                }
                
                ESP_LOGI(TAG, "🔄 AI tasks should be paused now, starting photo capture...");
                printf("🚫 LCD display DISABLED for SPI exclusive access\r\n");
                printf("⏸️  Face detection PAUSED for camera exclusive access\r\n");
                
                // AI任务现在应该已经暂停，可以安全拍照
                printf("📸 CAPTURING REAL-TIME PHOTO with segmented safe storage...\r\n");
                segmented_photo_t *segmented_photo = capture_photo_segmented();
                
                if (segmented_photo) {
                    ESP_LOGI(TAG, "✅ Real-time photo safely captured in segments, total size: %zu bytes", 
                             segmented_photo->total_size);
                    
                    // 保存分段照片到全局变量
                    g_system_state.captured_photo = segmented_photo;
                    
                    // 切换到上传模式
                    g_system_state.current_mode = SYSTEM_MODE_PHOTO_UPLOAD;
                    g_system_state.photo_upload_in_progress = true;
                    printf("🔄 Photo captured successfully, switching to upload mode...\r\n");
                } else {
                    ESP_LOGE(TAG, "❌ Failed to capture photo, aborting upload and returning to face detection");
                    printf("❌ Failed to capture photo, returning to face detection mode\r\n");
                    
                    // 拍照失败，返回人脸识别模式
                    g_system_state.photo_upload_requested = false;
                    g_system_state.face_detection_paused = false;
                    g_system_state.current_mode = SYSTEM_MODE_FACE_DETECTION;
                }
            }
            break;
            
        case SYSTEM_MODE_PHOTO_UPLOAD:
            // 执行照片上传（使用预先拍好的实时照片）
            if (g_system_state.photo_upload_in_progress) {
                ESP_LOGI(TAG, "📸 Uploading pre-captured real-time photo...");
                
                // 通知主任务暂时退出看门狗监控，因为照片上传耗时较长
                bool was_watchdog_active = main_watchdog_active;
                if (main_watchdog_active) {
                    esp_err_t ret = esp_task_wdt_delete(xTaskGetCurrentTaskHandle());
                    if (ret == ESP_OK) {
                        main_watchdog_active = false;
                        ESP_LOGI(TAG, "Main task temporarily removed from watchdog for photo upload");
                    }
                }
                
                esp_err_t upload_ret = ESP_FAIL;
                
                // 使用预先分段保存的实时照片进行上传
                if (g_system_state.captured_photo) {
                    ESP_LOGI(TAG, "Using pre-captured segmented photo for upload, total size: %zu bytes", 
                             g_system_state.captured_photo->total_size);
                    upload_ret = upload_segmented_photo(g_system_state.captured_photo);
                    
                    // 使用安全的释放函数释放分段照片内存
                    ESP_LOGI(TAG, "Releasing segmented photo memory");
                    release_segmented_photo(g_system_state.captured_photo);
                    g_system_state.captured_photo = NULL;
                } else {
                    ESP_LOGE(TAG, "No pre-captured photo available, upload failed");
                    upload_ret = ESP_FAIL;
                }
                
                // 重新加入看门狗监控
                if (was_watchdog_active && !main_watchdog_active) {
                    esp_err_t ret = esp_task_wdt_add(xTaskGetCurrentTaskHandle());
                    if (ret == ESP_OK) {
                        main_watchdog_active = true;
                        ESP_LOGI(TAG, "Main task re-added to watchdog after photo upload");
                    }
                }
                
                // 处理上传结果
                if (upload_ret == ESP_OK) {
                    printf("✅ Photo uploaded successfully! ✅\r\n");
                    ESP_LOGI(TAG, "✅ Photo upload successful");
                } else {
                    printf("❌ Photo upload failed! ❌\r\n");
                    ESP_LOGW(TAG, "❌ Photo upload failed");
                }
                
                // 无论成功失败都要停止报警并切换回人脸识别模式
                ESP_LOGI(TAG, "Stopping alarm and switching back to face detection mode");
                buzzer_alarm(0); // 确保停止报警
                
                // 完成上传，立即切换回人脸识别模式
                g_system_state.photo_upload_in_progress = false;
                printf("🖥️  LCD display RE-ENABLED - resuming normal operation\r\n");
                printf("▶️  Face detection RESUMED - camera access restored\r\n");
                
                // 不需要手动恢复任务，任务会检测到 system_can_do_face_detection() 返回 true
                // 并自动恢复正常运行和看门狗状态
                
                system_photo_upload_complete();
            }
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown system mode: %d", g_system_state.current_mode);
            // 强制切换回人脸识别模式
            g_system_state.current_mode = SYSTEM_MODE_FACE_DETECTION;
            break;
    }
    
    // 每5秒打印一次状态日志（用于调试）
    if (current_time - last_log_time > 5000) {
        if (g_system_state.current_mode != SYSTEM_MODE_FACE_DETECTION) {
            ESP_LOGD(TAG, "System mode: %d, Photo requested: %d, Face detection paused: %d", 
                     g_system_state.current_mode, 
                     g_system_state.photo_upload_requested,
                     g_system_state.face_detection_paused);
        }
        last_log_time = current_time;
    }
}
