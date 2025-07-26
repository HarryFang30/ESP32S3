/**
 ****************************************************************************************************
 * @file        main.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-12-01
 * @brief       人脸识别实验
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 ESP32-S3开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 ****************************************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "led.h"
#include "lcd.h"
#include "camera.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_face_detection.hpp"
#include "face_distance_c_interface.h"
#include "image_scaler.h"
#include "buzzer.h"
#include "photo_uploader.h"
#include "system_state_manager.h"
#include "esp_task_wdt.h"


i2c_obj_t i2c0_master;
unsigned long x_i = 0;
unsigned long x_j = 0;
extern QueueHandle_t xQueueAIFrameO;
camera_fb_t *face_ai_frameO = NULL;

// 全局变量存储最新的眼部坐标
int g_left_eye_x = -1, g_left_eye_y = -1;
int g_right_eye_x = -1, g_right_eye_y = -1;
int g_face_detected = 0;

// 距离检测相关变量  
static bool calibration_printed = false;

// 主任务的看门狗状态
bool main_watchdog_active = false;

/**
 * @brief 安全地重置看门狗
 */
void safe_watchdog_reset(void)
{
    /* 只有在已加入看门狗时才重置 */
    if (main_watchdog_active) {
        esp_task_wdt_reset();
    }
}

/**
 * @brief       人脸识别（RGB565）带缩放显示
 * @param       x:x轴坐标
 * @param       y:y轴坐标
 * @retval      无
 */
void lcd_human_detection_camera(uint16_t x, uint16_t y)
{
    /* 使用非阻塞方式接收队列，避免主任务被长时间阻塞导致看门狗超时 */
    if (xQueueReceive(xQueueAIFrameO, &face_ai_frameO, pdMS_TO_TICKS(100))) // 最多等待100ms
    {
        // 检查是否允许更新LCD（避免拍照上传期间的SPI冲突）
        if (!system_can_update_lcd()) {
            ESP_LOGD("main", "LCD update skipped during photo upload");
            goto err; // 直接释放帧缓冲，跳过LCD更新
        }
        
        // 目标显示尺寸 - LCD屏幕尺寸
        int target_width = 320;
        int target_height = 240;
        
        // 检查显示区域是否超出屏幕
        if (x + target_width > lcd_self.width || y + target_height > lcd_self.height)
        {
            goto err;
        }

        lcd_set_window(x, y, x + target_width - 1, y + target_height - 1);

        // 如果原图像不是320x240，需要缩放
        if (face_ai_frameO->width != target_width || face_ai_frameO->height != target_height) {
            
            // 使用极小的分块缩放，最大限度减少内存使用
            const int chunk_height = 4;  // 减少到4行，进一步减少内存需求
            size_t chunk_size = target_width * chunk_height * 2;
            
            // 检查内存是否足够
            uint32_t free_heap = esp_get_free_heap_size();
            if (free_heap < chunk_size + 20000) { // 预留20KB缓冲
                ESP_LOGW("main", "Insufficient memory for LCD display: %" PRIu32 " free, %zu needed", 
                         free_heap, chunk_size + 20000);
                goto err;
            }
            
            uint16_t* chunk_buf = (uint16_t*)malloc(chunk_size);
            
            if (chunk_buf) {
                for (int y_chunk = 0; y_chunk < target_height; y_chunk += chunk_height) {
                    int current_chunk_height = (y_chunk + chunk_height > target_height) ? 
                                             (target_height - y_chunk) : chunk_height;
                    
                    // 创建临时缓冲区用于一块的缩放
                    for (int i = 0; i < current_chunk_height; i++) {
                        for (int j = 0; j < target_width; j++) {
                            // 计算源图像中对应的像素位置
                            int src_x = (j * face_ai_frameO->width) / target_width;
                            int src_y = ((y_chunk + i) * face_ai_frameO->height) / target_height;
                            
                            // 边界检查
                            if (src_x >= face_ai_frameO->width) src_x = face_ai_frameO->width - 1;
                            if (src_y >= face_ai_frameO->height) src_y = face_ai_frameO->height - 1;
                            
                            chunk_buf[i * target_width + j] = 
                                ((uint16_t*)face_ai_frameO->buf)[src_y * face_ai_frameO->width + src_x];
                        }
                    }
                    
                    // 发送这一块数据到LCD
                    lcd_write_data((uint8_t*)chunk_buf, target_width * current_chunk_height * 2);
                }
                
                free(chunk_buf);
            } else {
                ESP_LOGE("main", "Failed to allocate chunk buffer (%zu bytes)", chunk_size);
                goto err;
            }
        } else {
            // 不需要缩放，直接显示
            /* lcd_buf存储摄像头整一帧RGB数据 */
            for (x_j = 0; x_j < face_ai_frameO->width * face_ai_frameO->height; x_j++)
            {
                lcd_buf[2 * x_j] = (face_ai_frameO->buf[2 * x_i]) ;
                lcd_buf[2 * x_j + 1] =  (face_ai_frameO->buf[2 * x_i + 1]);
                x_i ++;
            }
            
            /* 例如：96*96*2/1536 = 12;分12次发送RGB数据 */
            for (x_j = 0; x_j < (face_ai_frameO->width * face_ai_frameO->height * 2 / LCD_BUF_SIZE); x_j++)
            {
                /* &lcd_buf[j * LCD_BUF_SIZE] 偏移地址发送数据 */
                lcd_write_data(&lcd_buf[x_j * LCD_BUF_SIZE] , LCD_BUF_SIZE);
            }
        }
        
err:
        esp_camera_fb_return(face_ai_frameO);
        x_i = 0;
        face_ai_frameO = NULL;
    }
    else
    {
        /* 队列接收超时，可能是拍照期间或系统繁忙，不需要特殊处理 */
        ESP_LOGV("main", "No frame available from AI queue (timeout)");
    }
}

/**
 * @brief       程序入口
 * @param       无
 * @retval      无
 */
void app_main(void)
{
    uint8_t x = 0;
    esp_err_t ret;
    
    ret = nvs_flash_init();  /* 初始化NVS */

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    led_init();                 /* 初始化LED */
    i2c0_master = iic_init(I2C_NUM_0);   /* 初始化IIC0 */
    spi2_init();                /* 初始化SPI2 */
    xl9555_init(i2c0_master);   /* IO扩展芯片初始化 */
    buzzer_init_alarm_task();   /* 初始化蜂鸣器报警任务 */
    
    lcd_init();                 /* 初始化LCD */
    
    lcd_show_string(30, 50, 200, 16, 16, "ESP32S3", RED);
    lcd_show_string(30, 70, 200, 16, 16, "FACED DETECTIOIN TEST", RED);
    lcd_show_string(30, 90, 200, 16, 16, "ATOM@ALIENTEK", RED);
    
    /* 初始化WiFi和照片上传系统 */
    printf("开始初始化WiFi和照片上传系统...\r\n");
    esp_err_t wifi_ret = photo_uploader_init();
    if (wifi_ret == ESP_OK) {
        printf("WiFi初始化成功!\r\n");
        lcd_show_string(30, 110, 200, 16, 16, "WiFi Connected!", GREEN);
    } else {
        printf("WiFi初始化失败!\r\n");
        lcd_show_string(30, 110, 200, 16, 16, "WiFi Failed!", RED);
    }

    /* 初始化摄像头 */
    while (camera_init())
    {
        lcd_show_string(30, 110, 200, 16, 16, "CAMERA Fail!", BLUE);
        vTaskDelay(500);
    }

    lcd_clear(BLACK);

    /* 创建AI所需的内存及任务 */
    while (esp_face_detection_ai_strat())
    {
        lcd_show_string(30, 110, 200, 16, 16, "Create Task/Queue Fail!", BLUE);
        /* 删除AI所需的内存及任务 */
        esp_face_detection_ai_deinit();
        vTaskDelay(500);
    }

    /* 初始化距离检测系统 */
    if (init_distance_detection_system() != ESP_OK) {
        ESP_LOGE("main", "Failed to initialize distance detection system");
    }

    /* 初始化系统状态管理器 */
    if (system_state_manager_init() != ESP_OK) {
        ESP_LOGE("main", "Failed to initialize system state manager");
    } else {
        ESP_LOGI("main", "System state manager initialized successfully");
    }
    
    /* 将主任务添加到看门狗监控 */
    esp_err_t wdt_ret = esp_task_wdt_add(NULL);
    if (wdt_ret == ESP_OK) {
        main_watchdog_active = true;
        ESP_LOGI("main", "Main task successfully added to watchdog");
    } else {
        ESP_LOGW("main", "Failed to add main task to watchdog: %s", esp_err_to_name(wdt_ret));
        main_watchdog_active = false;
    }

    /* 显示距离检测状态信息 */
    if (!calibration_printed) {
        if (is_distance_calibrated()) {
            printf("\r\n=== Distance Detection System Ready ===\r\n");
            printf("System is calibrated and monitoring face distance\r\n");
            printf("Safe distance threshold: 30-33 cm\r\n");
            printf("=========================================\r\n\r\n");
        } else {
            printf("\r\n=== Distance Detection System ===\r\n");
            printf("System needs calibration first!\r\n");
            printf("Instructions:\r\n");
            printf("1. Position yourself 50cm from camera\r\n");
            printf("2. System will auto-start calibration when face detected\r\n");
            printf("3. Stay still during 20-frame calibration\r\n");
            printf("==================================\r\n\r\n");
            
            /* 自动开始标定 */
            start_distance_calibration();
        }
        calibration_printed = true;
    }

    while (1)
    {
        /* 定期喂看门狗 - 使用安全方法 */
        safe_watchdog_reset();
        
        /* 系统状态管理任务处理 - 处理拍照上传等异步任务 */
        system_state_task_handler();
        
        /* 处理完系统状态后重置看门狗 */
        safe_watchdog_reset();
        
        /* 检查可用内存 */
        uint32_t free_heap = esp_get_free_heap_size();
        if (free_heap < 30000) { // 如果可用内存少于30KB
            ESP_LOGW("main", "Critical low memory: %" PRIu32 " bytes, delaying processing", free_heap);
            safe_watchdog_reset(); // 延迟前重置看门狗
            vTaskDelay(pdMS_TO_TICKS(100)); // 延迟100ms让其他任务释放内存
            continue;
        }
        
        /* 处理图像 - 显示在屏幕中央 */
        lcd_human_detection_camera(0, 0);
        
        /* LCD处理完成后重置看门狗 */
        safe_watchdog_reset();
        
        x++;

        if (x % 20 == 0)
        {
            LED_TOGGLE();
        }

        /* 每100帧检查一次WiFi连接状态并更新LCD显示 */
        if (x % 100 == 0) {
            static bool last_wifi_status = false;
            bool current_wifi_status = wifi_is_connected();
            
            // 只在状态改变时更新LCD显示
            if (current_wifi_status != last_wifi_status) {
                if (current_wifi_status) {
                    printf("WiFi Status: Connected\r\n");
                    // 在LCD底部显示WiFi状态
                    lcd_show_string(10, 220, 100, 16, 12, "WiFi: OK", GREEN);
                } else {
                    printf("WiFi Status: Disconnected - Reconnecting...\r\n");
                    lcd_show_string(10, 220, 100, 16, 12, "WiFi: --", RED);
                }
                last_wifi_status = current_wifi_status;
            }
        }

        /* 每5秒检查一次距离检测状态 */
        if (x % 5000 == 0 && is_distance_calibrated()) {
            printf("Distance monitoring active... (Press reset to recalibrate)\r\n");
        }

        vTaskDelay(1);
    }
}
