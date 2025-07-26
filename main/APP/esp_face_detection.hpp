/**
 ****************************************************************************************************
 * @file        esp_face_detection.hpp
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

#ifndef __ESP_FACE_DETECTION_HPP
#define __ESP_FACE_DETECTION_HPP

#include "esp_camera.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// Face distance C interface
#include "face_distance_c_interface.h"

#ifdef __cplusplus
#include <list>
#include "dl_detect_define.hpp"
extern "C" {
#endif

extern QueueHandle_t xQueueAIFrameO;
extern TaskHandle_t camera_task_handle;
extern TaskHandle_t ai_task_handle;

/* C函数声明 */
uint8_t esp_face_detection_ai_strat(void);
void esp_face_detection_ai_deinit(void);

#ifdef __cplusplus
}

/* C++函数声明 */
void print_eye_coordinates(std::list<dl::detect::result_t> &results);
#endif

#endif
