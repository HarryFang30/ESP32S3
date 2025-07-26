#include "photo_uploader.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_camera.h"
#include "camera.h"
#include <string.h>
#include <inttypes.h>

static const char *TAG = "PhotoUploader";

/* WiFi连接状态 */
static bool wifi_connected = false;
static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

/**
 * @bri        // 分小块发送每个段 - 激进增加块大小最大化效率
        size_t chunk_written = 0;
        const size_t     // 配置HTTP客户端 - upload_photo函数
    esp_ht    // 分块发送图片数据 - 激进增加块大小最大化效率
    size_t total_written = 0;
    const size_t chunk_size = 32768;  // 激进增加到32KB最大化上传效率client_config_t config = {
        .url = SERVER_URL,
        .event_handler = http_event_handler,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 30000,  // 30秒超时，适应大照片上传
        .buffer_size = 16384,   // 激进增加缓冲区到16KB最大化传输效率
        .buffer_size_tx = 16384,
        .keep_alive_enable = true,   // 启用Keep-Alive提高连接效率
        .disable_auto_redirect = true,
    };size = 32768;  // 激进增加到32KB最大化上传效率WiFi事件处理函数
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        ESP_LOGI(TAG, "WiFi disconnected, retry connecting...");
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connected = true;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
 * @brief 初始化WiFi连接
 */
esp_err_t wifi_init(void)
{
    wifi_event_group = xEventGroupCreate();
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // 优化WiFi性能设置
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE)); // 禁用WiFi省电模式
    ESP_LOGI(TAG, "WiFi power save disabled for maximum performance");
    
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi initialization finished.");
    
    /* 等待连接 */
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                          WIFI_CONNECTED_BIT,
                                          pdFALSE,
                                          pdFALSE,
                                          portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP SSID:%s", WIFI_SSID);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to connect to WiFi");
        return ESP_FAIL;
    }
}

/**
 * @brief 检查WiFi连接状态
 */
bool wifi_is_connected(void)
{
    return wifi_connected;
}

/**
 * @brief HTTP事件处理函数
 */
esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    static int64_t connection_start_time = 0;
    static size_t data_sent = 0;
    
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "❌ HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            connection_start_time = esp_timer_get_time();
            data_sent = 0;
            ESP_LOGI(TAG, "✅ HTTP_EVENT_ON_CONNECTED - connection established");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "📤 HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "📥 HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            data_sent += evt->data_len;
            if (connection_start_time > 0) {
                int64_t elapsed = (esp_timer_get_time() - connection_start_time) / 1000;
                ESP_LOGI(TAG, "📊 HTTP_EVENT_ON_DATA, len=%d, total_sent=%zu, elapsed=%lld ms", 
                         evt->data_len, data_sent, elapsed);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            if (connection_start_time > 0) {
                int64_t total_time = (esp_timer_get_time() - connection_start_time) / 1000;
                ESP_LOGI(TAG, "✅ HTTP_EVENT_ON_FINISH - total time: %lld ms", total_time);
            }
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "🔌 HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGI(TAG, "🔄 HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

/**
 * @brief 安全拍照（在摄像头任务暂停后）
 */
camera_fb_t* capture_photo_safe(void)
{
    ESP_LOGI(TAG, "📸 Capturing photo safely after camera tasks are paused...");
    
    // 检查内存状况
    uint32_t free_heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "Free heap before photo capture: %" PRIu32 " bytes", free_heap);
    
    if (free_heap < 50000) {
        ESP_LOGW(TAG, "Insufficient memory for photo capture: %" PRIu32 " bytes", free_heap);
        return NULL;
    }
    
    // 确保摄像头任务已经暂停，等待足够时间让缓冲区稳定
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // 清理可能的残留缓冲区
    camera_fb_t *temp_fb = esp_camera_fb_get();
    if (temp_fb) {
        esp_camera_fb_return(temp_fb);
        ESP_LOGI(TAG, "Cleared residual camera buffer");
    }
    
    // 短暂延迟后获取新的干净帧
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 获取摄像头帧，重试机制
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGW(TAG, "Failed to capture photo on first attempt, retrying...");
        vTaskDelay(pdMS_TO_TICKS(200));
        fb = esp_camera_fb_get();
        
        if (!fb) {
            ESP_LOGW(TAG, "Failed to capture photo on second attempt, final retry...");
            vTaskDelay(pdMS_TO_TICKS(300));
            fb = esp_camera_fb_get();
        }
    }
    
    if (fb) {
        ESP_LOGI(TAG, "✅ Photo captured safely, size: %zu bytes, format: %d", fb->len, fb->format);
    } else {
        ESP_LOGE(TAG, "❌ Failed to capture photo after multiple attempts");
    }
    
    return fb;
}

/**
 * @brief 测试PSRAM是否可用
 */
static void test_psram_availability(void)
{
    ESP_LOGI(TAG, "=== PSRAM Availability Test ===");
    
    uint32_t total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    uint32_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    uint32_t total_internal = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    uint32_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    
    ESP_LOGI(TAG, "PSRAM Total: %" PRIu32 " bytes, Free: %" PRIu32 " bytes", total_psram, free_psram);
    ESP_LOGI(TAG, "Internal RAM Total: %" PRIu32 " bytes, Free: %" PRIu32 " bytes", total_internal, free_internal);
    
    if (total_psram > 0) {
        ESP_LOGI(TAG, "✅ PSRAM is available and configured");
        
        // 测试PSRAM分配
        void *test_ptr = heap_caps_malloc(100000, MALLOC_CAP_SPIRAM);
        if (test_ptr) {
            ESP_LOGI(TAG, "✅ PSRAM allocation test successful");
            free(test_ptr);
        } else {
            ESP_LOGW(TAG, "❌ PSRAM allocation test failed");
        }
    } else {
        ESP_LOGW(TAG, "❌ PSRAM is not available or not configured");
    }
    
    ESP_LOGI(TAG, "=== End PSRAM Test ===");
}

/**
 * @brief 分段安全拍照函数 - 将照片分成小块存储，避免大块内存问题
 */

#define MAX_CHUNK_SIZE 131072  // 128KB per chunk - 激进增加块大小最大化上传效率

/**
 * @brief 创建分段照片
 */
segmented_photo_t* create_segmented_photo(camera_fb_t *original_fb)
{
    if (!original_fb || !original_fb->buf || original_fb->len == 0) {
        ESP_LOGE(TAG, "Invalid original frame buffer");
        return NULL;
    }
    
    ESP_LOGI(TAG, "Creating segmented photo from %zu bytes", original_fb->len);
    
    // 计算需要的分段数
    size_t segment_count = (original_fb->len + MAX_CHUNK_SIZE - 1) / MAX_CHUNK_SIZE;
    
    // 分配分段照片结构
    segmented_photo_t *seg_photo = malloc(sizeof(segmented_photo_t));
    if (!seg_photo) {
        ESP_LOGE(TAG, "Failed to allocate segmented photo structure");
        return NULL;
    }
    
    // 分配分段指针数组
    seg_photo->segments = malloc(segment_count * sizeof(uint8_t*));
    seg_photo->segment_sizes = malloc(segment_count * sizeof(size_t));
    
    if (!seg_photo->segments || !seg_photo->segment_sizes) {
        ESP_LOGE(TAG, "Failed to allocate segment arrays");
        if (seg_photo->segments) free(seg_photo->segments);
        if (seg_photo->segment_sizes) free(seg_photo->segment_sizes);
        free(seg_photo);
        return NULL;
    }
    
    // 初始化结构
    memset(seg_photo->segments, 0, segment_count * sizeof(uint8_t*));
    seg_photo->segment_count = segment_count;
    seg_photo->total_size = original_fb->len;
    seg_photo->format = original_fb->format;
    seg_photo->width = original_fb->width;
    seg_photo->height = original_fb->height;
    
    // 分段复制数据
    size_t offset = 0;
    for (size_t i = 0; i < segment_count; i++) {
        size_t segment_size = (offset + MAX_CHUNK_SIZE <= original_fb->len) ? 
                           MAX_CHUNK_SIZE : (original_fb->len - offset);
        
        seg_photo->segment_sizes[i] = segment_size;
        
        // 优先使用PSRAM分配小段
        seg_photo->segments[i] = heap_caps_malloc(segment_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!seg_photo->segments[i]) {
            // 降级到内部RAM
            seg_photo->segments[i] = malloc(segment_size);
        }
        
        if (!seg_photo->segments[i]) {
            ESP_LOGE(TAG, "Failed to allocate segment %zu (%zu bytes)", i, segment_size);
            // 清理已分配的段
            for (size_t j = 0; j < i; j++) {
                free(seg_photo->segments[j]);
            }
            free(seg_photo->segments);
            free(seg_photo->segment_sizes);
            free(seg_photo);
            return NULL;
        }
        
        // 复制数据
        memcpy(seg_photo->segments[i], original_fb->buf + offset, segment_size);
        offset += segment_size;
        
        ESP_LOGD(TAG, "Copied segment %zu: %zu bytes", i, segment_size);
    }
    
    ESP_LOGI(TAG, "✅ Created segmented photo: %zu segments, %zu total bytes", 
             segment_count, original_fb->len);
    
    return seg_photo;
}

/**
 * @brief 释放分段照片
 */
void release_segmented_photo(segmented_photo_t *seg_photo)
{
    if (seg_photo) {
        if (seg_photo->segments) {
            for (size_t i = 0; i < seg_photo->segment_count; i++) {
                if (seg_photo->segments[i]) {
                    free(seg_photo->segments[i]);
                }
            }
            free(seg_photo->segments);
        }
        if (seg_photo->segment_sizes) {
            free(seg_photo->segment_sizes);
        }
        free(seg_photo);
        ESP_LOGI(TAG, "Released segmented photo");
    }
}

/**
 * @brief 安全的分段拍照函数
 */
segmented_photo_t* capture_photo_segmented(void)
{
    ESP_LOGI(TAG, "📸 Capturing photo with segmented storage...");
    
    // 首先测试PSRAM可用性
    test_psram_availability();
    
    // 获取原始摄像头帧
    camera_fb_t *original_fb = esp_camera_fb_get();
    if (!original_fb) {
        ESP_LOGE(TAG, "Failed to get camera frame");
        return NULL;
    }
    
    ESP_LOGI(TAG, "Original photo: size=%zu, format=%d", original_fb->len, original_fb->format);
    
    // 检查照片大小是否合理
    if (original_fb->len > 1000000) {  // 1MB上限
        ESP_LOGW(TAG, "Photo too large: %zu bytes, rejecting", original_fb->len);
        esp_camera_fb_return(original_fb);
        return NULL;
    }
    
    // 创建分段照片
    segmented_photo_t *seg_photo = create_segmented_photo(original_fb);
    
    // 立即释放原始帧
    esp_camera_fb_return(original_fb);
    
    if (seg_photo) {
        ESP_LOGI(TAG, "✅ Photo safely captured in %zu segments", seg_photo->segment_count);
    } else {
        ESP_LOGE(TAG, "❌ Failed to create segmented photo");
    }
    
    return seg_photo;
}

/**
 * @brief 释放安全复制的照片
 */
void release_copied_photo(camera_fb_t *copy_fb)
{
    if (copy_fb) {
        if (copy_fb->buf) {
            ESP_LOGI(TAG, "Freeing copied photo buffer (%zu bytes)", copy_fb->len);
            free((void*)copy_fb->buf);  // 释放图像数据
        }
        ESP_LOGI(TAG, "Freeing copied photo structure");
        free(copy_fb);  // 释放结构体
    }
}

/**
 * @brief 测试网络连接性能
 */
static void test_network_performance(void)
{
    ESP_LOGI(TAG, "=== Network Performance Test ===");
    
    // 简单的连接测试
    esp_http_client_config_t test_config = {
        .url = SERVER_URL,
        .method = HTTP_METHOD_HEAD,
        .timeout_ms = 5000,
    };
    
    esp_http_client_handle_t test_client = esp_http_client_init(&test_config);
    if (test_client) {
        int64_t start_time = esp_timer_get_time();
        esp_err_t err = esp_http_client_perform(test_client);
        int64_t end_time = esp_timer_get_time();
        
        if (err == ESP_OK) {
            int status_code = esp_http_client_get_status_code(test_client);
            int64_t latency = (end_time - start_time) / 1000;
            ESP_LOGI(TAG, "✅ Server connectivity test: %d status, %lld ms latency", status_code, latency);
            printf("🌐 Network latency to server: %lld ms\r\n", latency);
        } else {
            ESP_LOGW(TAG, "❌ Server connectivity test failed: %s", esp_err_to_name(err));
        }
        
        esp_http_client_cleanup(test_client);
    }
    
    ESP_LOGI(TAG, "=== End Network Test ===");
}

/**
 * @brief 上传分段照片到服务器
 */
esp_err_t upload_segmented_photo(segmented_photo_t *seg_photo)
{
    if (!seg_photo) {
        ESP_LOGE(TAG, "Cannot upload NULL segmented photo");
        return ESP_FAIL;
    }
    
    // 检查WiFi连接状态
    if (!wifi_connected) {
        ESP_LOGW(TAG, "WiFi not connected, cannot upload photo");
        return ESP_FAIL;
    }
    
    // 测试网络性能
    test_network_performance();
    
    const char *content_type;
    if (seg_photo->format == PIXFORMAT_JPEG) {
        content_type = "image/jpeg";
        ESP_LOGI(TAG, "Uploading JPEG format photo");
    } else {
        ESP_LOGW(TAG, "Unknown format %d, uploading as binary", seg_photo->format);
        content_type = "application/octet-stream";
    }

    // 配置HTTP客户端 - upload_segmented_photo函数
    esp_http_client_config_t config = {
        .url = SERVER_URL,
        .event_handler = http_event_handler,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 60000,  // 增加到60秒超时，但添加更多调试信息
        .buffer_size = 16384,   // 激进增加缓冲区到16KB最大化传输效率
        .buffer_size_tx = 16384,
        .keep_alive_enable = true,   // 启用Keep-Alive提高连接效率
        .disable_auto_redirect = true,
        .is_async = false,  // 确保同步模式
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    esp_err_t err = ESP_OK;
    
    // 设置HTTP头
    esp_http_client_set_header(client, "Content-Type", content_type);
    
    char content_length_str[32];
    snprintf(content_length_str, sizeof(content_length_str), "%zu", seg_photo->total_size);
    esp_http_client_set_header(client, "Content-Length", content_length_str);
    
    // 发送POST请求
    err = esp_http_client_open(client, seg_photo->total_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        goto cleanup;
    }

    ESP_LOGI(TAG, "Starting photo upload - total size: %zu bytes", seg_photo->total_size);
    
    // 记录开始时间
    int64_t upload_start_time = esp_timer_get_time();
    
    // 逐块发送数据
    size_t total_written = 0;
    
    for (size_t i = 0; i < seg_photo->segment_count; i++) {
        // 验证当前块
        if (!seg_photo->segments[i] || seg_photo->segment_sizes[i] == 0) {
            ESP_LOGE(TAG, "Invalid segment %zu", i);
            err = ESP_FAIL;
            goto cleanup;
        }
        
        ESP_LOGD(TAG, "Uploading segment %zu/%zu (%zu bytes)", 
                 i + 1, seg_photo->segment_count, seg_photo->segment_sizes[i]);
        
        // 分小块发送每个段 - 进一步增加块大小提高效率
        size_t chunk_written = 0;
        const size_t mini_chunk_size = 16384;  // 增加到16KB提高上传效率
        
        while (chunk_written < seg_photo->segment_sizes[i]) {
            size_t write_len = (seg_photo->segment_sizes[i] - chunk_written) > mini_chunk_size ? 
                              mini_chunk_size : (seg_photo->segment_sizes[i] - chunk_written);
            
            int wlen = esp_http_client_write(client, 
                                            (const char*)(seg_photo->segments[i] + chunk_written), 
                                            write_len);
            if (wlen < 0) {
                ESP_LOGE(TAG, "Failed to write segment %zu data at offset %zu", i, chunk_written);
                err = ESP_FAIL;
                goto cleanup;
            }
            
            chunk_written += wlen;
            total_written += wlen;
            
            // 移除不必要的延时以提高上传速度
        }
        
        ESP_LOGI(TAG, "Chunk %zu uploaded, total: %zu/%zu bytes", 
                 i + 1, total_written, seg_photo->total_size);
        
        // 移除块间延时以提高上传速度
    }
    
    ESP_LOGI(TAG, "All chunks sent, total: %zu bytes", total_written);
    
    // 计算上传速度
    int64_t upload_time = (esp_timer_get_time() - upload_start_time) / 1000; // 转换为毫秒
    if (upload_time > 0) {
        double speed_kbps = (double)(total_written * 8) / upload_time; // kbps
        ESP_LOGI(TAG, "📊 Upload performance: %lld ms, %.2f kbps", upload_time, speed_kbps);
        printf("📊 Upload speed: %.2f kbps (%lld ms for %zu bytes)\r\n", speed_kbps, upload_time, total_written);
    }

    // 获取响应
    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    
    ESP_LOGI(TAG, "HTTP Status: %d, Content-Length: %d", status_code, content_length);

    if (status_code >= 200 && status_code < 300) {
        ESP_LOGI(TAG, "✅ Segmented photo uploaded successfully! Status: %d", status_code);
        printf("📸 Server confirmed photo upload successful (Status: %d)\r\n", status_code);
        err = ESP_OK;
    } else if (status_code > 0) {
        ESP_LOGW(TAG, "Photo upload completed with status: %d", status_code);
        printf("⚠️  Photo upload completed with status: %d\r\n", status_code);
        err = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Photo upload failed - no valid HTTP response");
        printf("❌ Photo upload failed - no HTTP response\r\n");
        err = ESP_FAIL;
    }

cleanup:
    if (client) {
        ESP_LOGI(TAG, "Cleaning up HTTP client");
        esp_http_client_cleanup(client);
    }
    
    return err;
}

/**
 * @brief 上传预先拍好的照片到服务器（保留原函数用于兼容性）
 */
esp_err_t upload_photo(camera_fb_t *fb)
{
    if (!fb) {
        ESP_LOGE(TAG, "Cannot upload NULL photo");
        return ESP_FAIL;
    }
    
    // 检查WiFi连接状态
    if (!wifi_connected) {
        ESP_LOGW(TAG, "WiFi not connected, cannot upload photo");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Uploading photo: %zu bytes", fb->len);
    
    // 根据格式设置Content-Type
    const char* content_type;
    if (fb->format == PIXFORMAT_JPEG) {
        content_type = "image/jpeg";
        ESP_LOGI(TAG, "Uploading JPEG format photo");
    } else {
        ESP_LOGW(TAG, "Unknown format %d, uploading as binary", fb->format);
        content_type = "application/octet-stream";
    }

    // 配置HTTP客户端 - upload_photo函数
    esp_http_client_config_t config = {
        .url = SERVER_URL,
        .event_handler = http_event_handler,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 30000,  // 30秒超时，适应大照片上传
        .buffer_size = 8192,   // 进一步增加缓冲区到8KB提高传输效率
        .buffer_size_tx = 8192,
        .keep_alive_enable = true,   // 启用Keep-Alive提高连接效率
        .disable_auto_redirect = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    esp_err_t err = ESP_OK;
    
    // 设置HTTP头
    esp_http_client_set_header(client, "Content-Type", content_type);
    
    char content_length_str[32];
    snprintf(content_length_str, sizeof(content_length_str), "%zu", fb->len);
    esp_http_client_set_header(client, "Content-Length", content_length_str);
    
    // 发送POST请求
    err = esp_http_client_open(client, fb->len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        goto cleanup;
    }

    // 分块发送图片数据 - 进一步增加块大小提高效率
    size_t total_written = 0;
    const size_t chunk_size = 16384;  // 增加到16KB提高上传效率
    
    while (total_written < fb->len) {
        size_t write_len = (fb->len - total_written) > chunk_size ? 
                          chunk_size : (fb->len - total_written);
        
        int wlen = esp_http_client_write(client, 
                                        (const char*)(fb->buf + total_written), 
                                        write_len);
        if (wlen < 0) {
            ESP_LOGE(TAG, "Failed to write HTTP data at offset %zu", total_written);
            err = ESP_FAIL;
            goto cleanup;
        }
        
        total_written += wlen;
        
        // 移除不必要的延时以提高上传速度
    }
    
    ESP_LOGI(TAG, "Sent %zu bytes to server", total_written);

    // 获取响应
    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    
    ESP_LOGI(TAG, "HTTP Status: %d, Content-Length: %d", status_code, content_length);

    if (status_code >= 200 && status_code < 300) {
        ESP_LOGI(TAG, "✅ Photo uploaded successfully! Status: %d", status_code);
        err = ESP_OK;
    } else if (status_code > 0) {
        ESP_LOGW(TAG, "Photo upload completed with status: %d", status_code);
        err = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Photo upload failed - no valid HTTP response");
        err = ESP_FAIL;
    }

cleanup:
    if (client) {
        ESP_LOGI(TAG, "Cleaning up HTTP client");
        esp_http_client_cleanup(client);
    }
    
    return err;
}

/**
 * @brief 拍照并上传到服务器（保留兼容性，现在改为正确的暂停-拍照-恢复流程）
 */
esp_err_t capture_and_upload_photo(void)
{
    ESP_LOGI(TAG, "🔄 capture_and_upload_photo called (correct flow: pause->capture->resume)");
    
    // 正确的流程：先请求系统状态管理器暂停人脸识别和LCD
    // 通过系统状态管理器来协调整个流程
    ESP_LOGI(TAG, "Requesting photo upload through system state manager...");
    
    // 使用外部函数设置拍照上传请求标志，避免直接访问结构体
    extern void system_request_photo_upload(void);
    system_request_photo_upload();
    
    // 等待系统状态管理器完成拍照上传流程
    // 这里只是触发请求，实际的暂停-拍照-上传-恢复由系统状态管理器负责
    ESP_LOGI(TAG, "Photo upload request submitted to system state manager");
    
    return ESP_OK;
}

/**
 * @brief 初始化照片上传系统
 */
esp_err_t photo_uploader_init(void)
{
    ESP_LOGI(TAG, "Initializing photo uploader system...");
    
    // 初始化WiFi
    esp_err_t ret = wifi_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi initialization failed");
        return ret;
    }

    ESP_LOGI(TAG, "Photo uploader system initialized successfully");
    return ESP_OK;
}
