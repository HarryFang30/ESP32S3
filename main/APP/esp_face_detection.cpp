/**
 ****************************************************************************************************
* @file        esp_face_detection.cpp
* @author      正点原子团队(ALIENTEK)
* @version     V1.0
* @date        2023-12-01
* @brief       人脸识别代码
* @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
****************************************************************************************************
* @attention
*
* 实验平台:正点原子 ESP32-S3 开发板
* 在线视频:www.yuanzige.com
* 技术论坛:www.openedv.com
* 公司网址:www.alientek.com
* 购买地址:openedv.taobao.com
*
****************************************************************************************************
*/

#include "esp_face_detection.hpp"
#include "dl_image.hpp"
#include "human_face_detect_msr01.hpp"
#include "human_face_detect_mnp01.hpp"
#include "who_ai_utils.hpp"
#include "face_distance_c_interface.h"
#include "esp_task_wdt.h"
#include "system_state_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


TaskHandle_t camera_task_handle;
TaskHandle_t ai_task_handle;
QueueHandle_t xQueueFrameO = NULL;
QueueHandle_t xQueueAIFrameO = NULL;


/**
 * @brief       摄像头图像数据获取任务
 * @param       arg：未使用
 * @retval      无
 */
static void camera_process_handler(void *arg)
{
    arg = arg;
    camera_fb_t *camera_frame = NULL;
    bool watchdog_active = false;
    bool cleanup_logged = false; // 移动到函数级别
    esp_err_t wdt_ret;

    /* 安全地将当前任务添加到看门狗监控 */
    wdt_ret = esp_task_wdt_add(NULL);
    if (wdt_ret == ESP_OK) {
        watchdog_active = true;
        ESP_LOGI("Camera_Task", "Successfully added to watchdog");
    } else {
        ESP_LOGW("Camera_Task", "Failed to add to watchdog: %s", esp_err_to_name(wdt_ret));
    }

    while (1)
    {
        /* 检查是否可以获取摄像头帧（避免与照片上传冲突） */
        if (!system_can_do_face_detection()) {
            /* 照片上传期间完全暂停摄像头获取，释放摄像头资源 */
            if (watchdog_active) {
                wdt_ret = esp_task_wdt_delete(NULL);
                if (wdt_ret == ESP_OK) {
                    watchdog_active = false;
                    ESP_LOGI("Camera_Task", "Removed from watchdog during photo upload");
                } else {
                    ESP_LOGW("Camera_Task", "Failed to remove from watchdog: %s", esp_err_to_name(wdt_ret));
                    /* 即使失败也标记为非活跃，避免重复尝试删除 */
                    watchdog_active = false;
                }
            }
            
            /* 在暂停期间不访问摄像头，但保持较短的等待时间避免摄像头休眠 */
            if (!cleanup_logged) {
                ESP_LOGI("Camera_Task", "Camera task paused - releasing camera resources for photo upload");
                cleanup_logged = true;
            }
            
            /* 适度的等待时间，避免摄像头硬件进入休眠状态 */
            vTaskDelay(200 / portTICK_PERIOD_MS);
            continue;
        } else {
            /* 恢复正常操作时重新加入看门狗 */
            cleanup_logged = false; // 重置日志标志
            if (!watchdog_active) {
                wdt_ret = esp_task_wdt_add(NULL);
                if (wdt_ret == ESP_OK) {
                    watchdog_active = true;
                    ESP_LOGI("Camera_Task", "Re-added to watchdog after photo upload");
                } else {
                    ESP_LOGW("Camera_Task", "Failed to re-add to watchdog: %s", esp_err_to_name(wdt_ret));
                    /* 重新加入失败，保持非活跃状态 */
                }
            }
        }
        
        /* 重置看门狗 - 只有在成功加入看门狗时才重置 */
        if (watchdog_active) {
            esp_task_wdt_reset();
        }
        
        /* 获取摄像头图像 */
        camera_frame = esp_camera_fb_get();

        if (camera_frame)
        {
            /* 以队列的形式发送 */
            xQueueSend(xQueueFrameO, &camera_frame, portMAX_DELAY);
        } else {
            /* 如果获取失败，短暂延时避免CPU占用过高 */
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        /* 给其他任务一些执行时间 */
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/**
 * @brief       摄像头图像数据传入AI处理任务
 * @param       arg：未使用
 * @retval      无
 */
static void ai_process_handler(void *arg)
{
    arg = arg;
    camera_fb_t *face_ai_frameI = NULL;
    HumanFaceDetectMSR01 detector(0.3F, 0.3F, 10, 0.3F);
    HumanFaceDetectMNP01 detector2(0.4F, 0.3F, 10);
    bool watchdog_active = false;
    esp_err_t wdt_ret;

    /* 安全地将当前任务添加到看门狗监控 */
    wdt_ret = esp_task_wdt_add(NULL);
    if (wdt_ret == ESP_OK) {
        watchdog_active = true;
        ESP_LOGI("AI_Task", "Successfully added to watchdog");
    } else {
        ESP_LOGW("AI_Task", "Failed to add to watchdog: %s", esp_err_to_name(wdt_ret));
    }

    /* 添加帧率控制，降低AI推理频率减少CPU负载 */
    int frame_skip_counter = 0;
    const int FRAME_SKIP_RATE = 3; // 每4帧处理1帧

    while(1)
    {
        /* 检查是否可以进行人脸识别 */
        if (!system_can_do_face_detection()) {
            if (watchdog_active) {
                wdt_ret = esp_task_wdt_delete(NULL);
                if (wdt_ret == ESP_OK) {
                    watchdog_active = false;
                    ESP_LOGI("AI_Task", "Removed from watchdog during photo upload");
                } else {
                    ESP_LOGW("AI_Task", "Failed to remove from watchdog: %s", esp_err_to_name(wdt_ret));
                    /* 即使失败也标记为非活跃，避免重复尝试删除 */
                    watchdog_active = false;
                }
            }
            
            /* 跳过本次处理并释放帧缓冲 */
            if (xQueueReceive(xQueueFrameO, &face_ai_frameI, 10 / portTICK_PERIOD_MS)) {
                /* 直接转发到输出队列，不进行AI处理 */
                xQueueSend(xQueueAIFrameO, &face_ai_frameI, portMAX_DELAY);
            }
            /* 短暂延时，让主任务有时间处理拍照 */
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        } else {
            /* 恢复正常操作时重新加入看门狗 */
            if (!watchdog_active) {
                wdt_ret = esp_task_wdt_add(NULL);
                if (wdt_ret == ESP_OK) {
                    watchdog_active = true;
                    ESP_LOGI("AI_Task", "Re-added to watchdog after photo upload");
                } else {
                    ESP_LOGW("AI_Task", "Failed to re-add to watchdog: %s", esp_err_to_name(wdt_ret));
                    /* 重新加入失败，保持非活跃状态 */
                }
            }
        }
        
        /* 重置看门狗，防止AI处理任务超时 - 只有在成功加入看门狗时才重置 */
        if (watchdog_active) {
            esp_task_wdt_reset();
        }
        
        /* 以队列的形式获取摄像头图像数据 */
        if (xQueueReceive(xQueueFrameO, &face_ai_frameI, portMAX_DELAY))
        {
            /* 帧率控制 - 跳过一些帧以减少CPU负载 */
            frame_skip_counter++;
            if (frame_skip_counter < FRAME_SKIP_RATE) {
                /* 直接转发帧，不进行AI处理 */
                xQueueSend(xQueueAIFrameO, &face_ai_frameI, portMAX_DELAY);
                continue;
            }
            frame_skip_counter = 0; // 重置计数器
            
            /* 在AI推理前重置看门狗 - 因为推理可能耗时较长 */
            if (watchdog_active) {
                esp_task_wdt_reset();
            }
            
            /* 判断图像是否出现人脸 - 第一次推理 */
            std::list<dl::detect::result_t> &detect_candidates = detector.infer((uint16_t *)face_ai_frameI->buf, {(int)face_ai_frameI->height, (int)face_ai_frameI->width, 3});
            
            /* 第二次推理 - 添加超时保护 */
            std::list<dl::detect::result_t> &detect_results = detector2.infer((uint16_t *)face_ai_frameI->buf, {(int)face_ai_frameI->height, (int)face_ai_frameI->width, 3}, detect_candidates);

            /* 推理完成后重置看门狗 */
            if (watchdog_active) {
                esp_task_wdt_reset();
            }

            if (detect_results.size() > 0)
            {
                printf("Face detected - Count: %d\r\n", (int)detect_results.size());
                
                /* 输出人脸关键点信息用于调试 */
                print_eye_coordinates(detect_results);
                
                /* 处理距离检测 */
                printf("Calling distance detection...\r\n");
                handle_distance_detection_c(&detect_results, face_ai_frameI);
                
                /* 此处是在图像中绘画检测效果 */
                draw_detection_result((uint16_t *)face_ai_frameI->buf, face_ai_frameI->height, face_ai_frameI->width, detect_results);
            }
            else
            {
                /* 当没有检测到人脸时，处理状态重置和蜂鸣器关闭 */
                handle_no_face_detected_c();
            }
            
            /* 以队列的形式发送AI处理的图像 */
            xQueueSend(xQueueAIFrameO, &face_ai_frameI, portMAX_DELAY);
        }
    }
}

/**
 * @brief       AI图像数据开启
 * @param       无
 * @retval      1：创建失败；0：创建成功
 */
uint8_t esp_face_detection_ai_strat(void)
{
    /* 创建队列及任务 - 调整栈大小平衡内存使用 */
    xQueueFrameO = xQueueCreate(5, sizeof(camera_fb_t *));
    xQueueAIFrameO = xQueueCreate(5, sizeof(camera_fb_t *));
    xTaskCreatePinnedToCore(camera_process_handler, "cam_task", 6 * 1024, NULL, 5, &camera_task_handle, 1);
    xTaskCreatePinnedToCore(ai_process_handler, "ai_process_hand", 10 * 1024, NULL, 4, &ai_task_handle, 1);

    if (xQueueFrameO != NULL 
        || xQueueAIFrameO != NULL 
        || camera_task_handle != NULL 
        || ai_task_handle != NULL)
    {
        return 0;
    }

    return 1;
}

/**
 * @brief       删除AI图像处理任务和队列
 * @param       无
 * @retval      无
 */
void esp_face_detection_ai_deinit(void)
{
    /* 在删除任务前，确保从看门狗中移除 */
    if (camera_task_handle != NULL) {
        ESP_LOGI("Camera_Task", "Removing camera task from watchdog before deletion");
        /* 使用任务句柄删除看门狗监控 */
        esp_err_t ret = esp_task_wdt_delete(camera_task_handle);
        if (ret != ESP_OK) {
            ESP_LOGW("Camera_Task", "Failed to remove from watchdog: %s", esp_err_to_name(ret));
        }
        vTaskDelete(camera_task_handle);
        camera_task_handle = NULL;
    }

    if (ai_task_handle != NULL) {
        ESP_LOGI("AI_Task", "Removing AI task from watchdog before deletion");
        /* 使用任务句柄删除看门狗监控 */
        esp_err_t ret = esp_task_wdt_delete(ai_task_handle);
        if (ret != ESP_OK) {
            ESP_LOGW("AI_Task", "Failed to remove from watchdog: %s", esp_err_to_name(ret));
        }
        vTaskDelete(ai_task_handle);
        ai_task_handle = NULL;
    }

    if (xQueueFrameO != NULL)
    {
        vQueueDelete(xQueueFrameO);
        xQueueFrameO = NULL;
    }

    if (xQueueAIFrameO != NULL)
    {
        vQueueDelete(xQueueAIFrameO);
        xQueueAIFrameO = NULL;
    }
    
    /* 清理距离检测器 */
    deinit_distance_detection_system();
}

/**
 * @brief       输出左右眼中心坐标
 * @param       results：检测结果
 * @retval      无
 */
void print_eye_coordinates(std::list<dl::detect::result_t> &results)
{
    int face_count = 0;
    for (std::list<dl::detect::result_t>::iterator prediction = results.begin(); prediction != results.end(); prediction++, face_count++)
    {
        if (prediction->keypoint.size() == 10)
        {
            // 获取左右眼坐标
            int left_eye_x = prediction->keypoint[0];
            int left_eye_y = prediction->keypoint[1];
            int right_eye_x = prediction->keypoint[6];
            int right_eye_y = prediction->keypoint[7];
            
            // 输出眼部坐标信息
            printf("=== Face %d Eye Coordinates ===\r\n", face_count + 1);
            printf("Left Eye Center:  (%3d, %3d)\r\n", left_eye_x, left_eye_y);
            printf("Right Eye Center: (%3d, %3d)\r\n", right_eye_x, right_eye_y);
            printf("Eye Distance: %d pixels\r\n", abs(right_eye_x - left_eye_x));
            printf("================================\r\n");
        }
        else
        {
            printf("Face %d: No keypoints detected\r\n", face_count + 1);
        }
    }
}
