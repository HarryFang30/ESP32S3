#ifndef __PHOTO_UPLOADER_H__
#define __PHOTO_UPLOADER_H__

#include "esp_camera.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "wifi_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi配置 - 在wifi_config.h文件中修改
 */
// WiFi和服务器配置在wifi_config.h文件中定义

/**
 * @brief 初始化WiFi连接
 * @retval ESP_OK: 成功, ESP_FAIL: 失败
 */
esp_err_t wifi_init(void);

/**
 * @brief 检查WiFi连接状态
 * @retval true: 已连接, false: 未连接
 */
bool wifi_is_connected(void);

/**
 * @brief 安全拍照（在摄像头任务暂停后）
 * @retval 成功时返回camera_fb_t指针，失败时返回NULL
 */
camera_fb_t* capture_photo_safe(void);

/**
 * @brief 安全的拍照函数 - 复制数据到PSRAM避免竞争条件
 * @retval 成功时返回安全复制的camera_fb_t指针，失败时返回NULL
 */
camera_fb_t* capture_photo_safe_copy(void);

/**
 * @brief 分段照片结构体
 */
typedef struct {
    uint8_t **segments;         /*!< 分段数据指针数组 */
    size_t *segment_sizes;      /*!< 每个分段的大小 */
    size_t segment_count;       /*!< 分段数量 */
    size_t total_size;          /*!< 总数据大小 */
    size_t width;               /*!< 图像宽度 */
    size_t height;              /*!< 图像高度 */
    pixformat_t format;         /*!< 图像格式 */
    struct timeval timestamp;   /*!< 时间戳 */
} segmented_photo_t;

/**
 * @brief 安全的分段拍照函数 - 将照片分成小块存储，避免大块内存问题
 * @retval 成功时返回分段照片指针，失败时返回NULL
 */
segmented_photo_t* capture_photo_segmented(void);

/**
 * @brief 上传分段照片到服务器
 * @param seg_photo 分段照片指针
 * @retval ESP_OK: 成功, ESP_FAIL: 失败
 */
esp_err_t upload_segmented_photo(segmented_photo_t *seg_photo);

/**
 * @brief 释放安全复制的照片
 * @param copy_fb 需要释放的复制照片指针
 */
void release_copied_photo(camera_fb_t *copy_fb);

/**
 * @brief 释放分段照片
 * @param seg_photo 需要释放的分段照片指针
 */
void release_segmented_photo(segmented_photo_t *seg_photo);

/**
 * @brief 上传预先拍好的照片到服务器
 * @param fb 预先拍好的照片帧缓冲
 * @retval ESP_OK: 成功, ESP_FAIL: 失败
 */
esp_err_t upload_photo(camera_fb_t *fb);

/**
 * @brief 拍照并上传到服务器（保留兼容性）
 * @retval ESP_OK: 成功, ESP_FAIL: 失败
 */
esp_err_t capture_and_upload_photo(void);

/**
 * @brief 初始化照片上传系统
 * @retval ESP_OK: 成功, ESP_FAIL: 失败
 */
esp_err_t photo_uploader_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __PHOTO_UPLOADER_H__ */
