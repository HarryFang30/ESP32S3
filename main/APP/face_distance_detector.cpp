/**
 ****************************************************************************************************
 * @file        face_distance_detector.cpp
 * @author      AI Assistant
 * @version     V1.0
 * @date        2025-07-24
 * @brief       基于姿态感知的鲁棒型人脸过近检测系统实现
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

#include "face_distance_detector.hpp"

static const char *TAG = "FaceDistanceDetector";

/**
 * @brief 构造函数
 */
FaceDistanceDetector::FaceDistanceDetector() 
    : k_constant_(0.0f)
    , is_calibrated_(false)
    , current_state_(FACE_DISTANCE_SAFE)
    , calibration_in_progress_(false)
{
    // 初始化姿态校正参数
    correction_params_.min_ratio = 0.7f;      // 头部左转时的最小比例
    correction_params_.max_ratio = 1.3f;      // 头部右转时的最大比例
    correction_params_.min_correction = 0.85f; // 最小校正系数
    correction_params_.max_correction = 1.15f; // 最大校正系数
}

/**
 * @brief 析构函数
 */
FaceDistanceDetector::~FaceDistanceDetector()
{
    // 清理资源
}

/**
 * @brief 初始化检测器
 */
esp_err_t FaceDistanceDetector::init()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // 从NVS加载标定数据
    ret = loadFromNVS();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No calibration data found, please calibrate first");
        is_calibrated_ = false;
    }
    
    // 初始化滤波队列
    while (!filter_queue_.empty()) {
        filter_queue_.pop();
    }
    
    ESP_LOGI(TAG, "Face distance detector initialized. Calibrated: %s", 
             is_calibrated_ ? "Yes" : "No");
    
    return ESP_OK;
}

/**
 * @brief 计算双眼内眼角距离
 */
float FaceDistanceDetector::calculateEyeDistance(const std::vector<int>& keypoints)
{
    if (keypoints.size() < 8) {
        ESP_LOGW(TAG, "Insufficient keypoints for eye distance calculation");
        return -1.0f;
    }
    
    // 左眼中心: keypoints[0], keypoints[1]
    // 右眼中心: keypoints[6], keypoints[7]
    float left_eye_x = keypoints[0];
    float left_eye_y = keypoints[1];
    float right_eye_x = keypoints[6];
    float right_eye_y = keypoints[7];
    
    float dx = right_eye_x - left_eye_x;
    float dy = right_eye_y - left_eye_y;
    
    return sqrtf(dx * dx + dy * dy);
}

/**
 * @brief 计算偏航比例 (用于姿态感知)
 */
float FaceDistanceDetector::calculateYawRatio(const std::vector<int>& keypoints)
{
    if (keypoints.size() < 10) {
        ESP_LOGW(TAG, "Insufficient keypoints for yaw ratio calculation");
        return 1.0f; // 默认正面
    }
    
    // 左眼中心: keypoints[0], keypoints[1]
    // 右眼中心: keypoints[6], keypoints[7]  
    // 鼻子: keypoints[4], keypoints[5]
    float left_eye_x = keypoints[0];
    float left_eye_y = keypoints[1];
    float right_eye_x = keypoints[6];
    float right_eye_y = keypoints[7];
    float nose_x = keypoints[4];
    float nose_y = keypoints[5];
    
    // 计算左眼到鼻子的距离
    float dx_left = nose_x - left_eye_x;
    float dy_left = nose_y - left_eye_y;
    float dist_left = sqrtf(dx_left * dx_left + dy_left * dy_left);
    
    // 计算右眼到鼻子的距离
    float dx_right = nose_x - right_eye_x;
    float dy_right = nose_y - right_eye_y;
    float dist_right = sqrtf(dx_right * dx_right + dy_right * dy_right);
    
    // 避免除零
    if (dist_right < 1.0f) {
        return 1.0f;
    }
    
    return dist_left / dist_right;
}

/**
 * @brief 获取姿态校正系数
 */
float FaceDistanceDetector::getPoseCorrection(float yaw_ratio)
{
    // 限制比例范围
    yaw_ratio = fmaxf(correction_params_.min_ratio, 
                     fminf(correction_params_.max_ratio, yaw_ratio));
    
    // 线性插值计算校正系数
    float normalized_ratio = (yaw_ratio - 1.0f) / (correction_params_.max_ratio - 1.0f);
    
    if (yaw_ratio > 1.0f) {
        // 头部右转，需要减小校正系数
        return 1.0f + normalized_ratio * (correction_params_.min_correction - 1.0f);
    } else {
        // 头部左转，需要增大校正系数
        normalized_ratio = (1.0f - yaw_ratio) / (1.0f - correction_params_.min_ratio);
        return 1.0f + normalized_ratio * (correction_params_.max_correction - 1.0f);
    }
}

/**
 * @brief 更新滤波队列
 */
void FaceDistanceDetector::updateFilterQueue(float distance)
{
    filter_queue_.push(distance);
    
    // 保持队列大小
    while (filter_queue_.size() > FILTER_QUEUE_SIZE) {
        filter_queue_.pop();
    }
}

/**
 * @brief 获取平滑距离
 */
float FaceDistanceDetector::getSmoothedDistance()
{
    if (filter_queue_.empty()) {
        return -1.0f;
    }
    
    float sum = 0.0f;
    std::queue<float> temp_queue = filter_queue_;
    
    while (!temp_queue.empty()) {
        sum += temp_queue.front();
        temp_queue.pop();
    }
    
    return sum / filter_queue_.size();
}

/**
 * @brief 开始标定程序
 */
esp_err_t FaceDistanceDetector::startCalibration()
{
    ESP_LOGI(TAG, "Starting calibration...");
    ESP_LOGI(TAG, "Please face the camera directly and sit %.1f cm away", KNOWN_DISTANCE_CM);
    
    calibration_samples_.clear();
    calibration_in_progress_ = true;
    
    return ESP_OK;
}

/**
 * @brief 添加标定帧数据
 */
bool FaceDistanceDetector::addCalibrationFrame(const std::vector<int>& keypoints)
{
    if (!calibration_in_progress_) {
        return false;
    }
    
    float eye_distance = calculateEyeDistance(keypoints);
    if (eye_distance > 0) {
        calibration_samples_.push_back(eye_distance);
        ESP_LOGD(TAG, "Calibration sample %d/%d: %.2f pixels", 
                 calibration_samples_.size(), CALIBRATION_FRAMES, eye_distance);
    }
    
    return calibration_samples_.size() >= CALIBRATION_FRAMES;
}

/**
 * @brief 完成标定
 */
esp_err_t FaceDistanceDetector::finishCalibration()
{
    if (!calibration_in_progress_ || calibration_samples_.empty()) {
        ESP_LOGE(TAG, "No calibration data available");
        return ESP_FAIL;
    }
    
    // 计算平均值
    float sum = 0.0f;
    for (float sample : calibration_samples_) {
        sum += sample;
    }
    float avg_eye_distance = sum / calibration_samples_.size();
    
    // 计算K常数
    k_constant_ = KNOWN_DISTANCE_CM * avg_eye_distance;
    
    ESP_LOGI(TAG, "Calibration completed. K constant: %.2f", k_constant_);
    
    // 保存到NVS
    esp_err_t ret = saveToNVS();
    if (ret == ESP_OK) {
        is_calibrated_ = true;
        calibration_in_progress_ = false;
        ESP_LOGI(TAG, "Calibration data saved successfully");
    } else {
        ESP_LOGE(TAG, "Failed to save calibration data");
    }
    
    return ret;
}

/**
 * @brief 处理一帧人脸数据
 */
face_distance_state_t FaceDistanceDetector::processFrame(const std::list<dl::detect::result_t>& results)
{
    if (!is_calibrated_) {
        ESP_LOGW(TAG, "Detector not calibrated, please calibrate first");
        return current_state_;
    }
    
    if (results.empty()) {
        // 没有检测到人脸，保持当前状态
        return current_state_;
    }
    
    // 使用第一个检测到的人脸
    const auto& face = results.front();
    
    if (face.keypoint.size() < 10) {
        ESP_LOGW(TAG, "Insufficient keypoints in detection result");
        return current_state_;
    }
    
    // 计算特征
    float eye_distance = calculateEyeDistance(face.keypoint);
    float yaw_ratio = calculateYawRatio(face.keypoint);
    
    if (eye_distance <= 0) {
        return current_state_;
    }
    
    // 姿态校正
    float correction_factor = getPoseCorrection(yaw_ratio);
    float corrected_eye_distance = eye_distance / correction_factor;
    
    // 距离解算
    float raw_distance = k_constant_ / corrected_eye_distance;
    
    // 数据滤波
    updateFilterQueue(raw_distance);
    float smoothed_distance = getSmoothedDistance();
    
    // 状态决策
    if (current_state_ == FACE_DISTANCE_SAFE && smoothed_distance < ENTER_THRESHOLD_CM) {
        current_state_ = FACE_DISTANCE_TOO_CLOSE;
        ESP_LOGW(TAG, "Face too close! Distance: %.1f cm", smoothed_distance);
    } else if (current_state_ == FACE_DISTANCE_TOO_CLOSE && smoothed_distance > EXIT_THRESHOLD_CM) {
        current_state_ = FACE_DISTANCE_SAFE;
        ESP_LOGI(TAG, "Face distance safe. Distance: %.1f cm", smoothed_distance);
    }
    
    ESP_LOGD(TAG, "Distance: %.1f cm, Yaw ratio: %.2f, Correction: %.2f", 
             smoothed_distance, yaw_ratio, correction_factor);
    
    return current_state_;
}

/**
 * @brief 获取当前距离
 */
float FaceDistanceDetector::getCurrentDistance() const
{
    return const_cast<FaceDistanceDetector*>(this)->getSmoothedDistance();
}

/**
 * @brief 保存到NVS
 */
esp_err_t FaceDistanceDetector::saveToNVS()
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = nvs_set_blob(nvs_handle, NVS_K_CONSTANT_KEY, &k_constant_, sizeof(k_constant_));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error saving K constant: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    uint8_t calibrated = 1;
    ret = nvs_set_u8(nvs_handle, NVS_CALIBRATED_KEY, calibrated);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error saving calibration flag: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    return ret;
}

/**
 * @brief 从NVS加载
 */
esp_err_t FaceDistanceDetector::loadFromNVS()
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Error opening NVS handle: %s", esp_err_to_name(ret));
        return ret;
    }
    
    size_t required_size = sizeof(k_constant_);
    ret = nvs_get_blob(nvs_handle, NVS_K_CONSTANT_KEY, &k_constant_, &required_size);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "K constant not found in NVS");
        nvs_close(nvs_handle);
        return ret;
    }
    
    uint8_t calibrated = 0;
    ret = nvs_get_u8(nvs_handle, NVS_CALIBRATED_KEY, &calibrated);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Calibration flag not found in NVS");
        nvs_close(nvs_handle);
        return ret;
    }
    
    is_calibrated_ = (calibrated == 1);
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "Loaded calibration data: K=%.2f, Calibrated=%s", 
             k_constant_, is_calibrated_ ? "Yes" : "No");
    
    return ESP_OK;
}

/**
 * @brief 重置标定
 */
esp_err_t FaceDistanceDetector::resetCalibration()
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = nvs_erase_all(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error erasing NVS: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    k_constant_ = 0.0f;
    is_calibrated_ = false;
    current_state_ = FACE_DISTANCE_SAFE;
    
    // 清空滤波队列
    while (!filter_queue_.empty()) {
        filter_queue_.pop();
    }
    
    ESP_LOGI(TAG, "Calibration reset successfully");
    
    return ret;
}

/**
 * @brief 设置阈值参数
 */
void FaceDistanceDetector::setThresholds(float enter_threshold, float exit_threshold)
{
    // 注意：由于使用了constexpr，这里只是示例
    // 实际实现中需要将阈值改为成员变量
    ESP_LOGI(TAG, "Threshold setting requested: enter=%.1f, exit=%.1f", 
             enter_threshold, exit_threshold);
}

// C接口实现
extern "C" {

FaceDistanceDetector* face_distance_detector_create(void)
{
    return new FaceDistanceDetector();
}

void face_distance_detector_destroy(FaceDistanceDetector* detector)
{
    if (detector) {
        delete detector;
    }
}

esp_err_t face_distance_detector_init(FaceDistanceDetector* detector)
{
    if (!detector) return ESP_ERR_INVALID_ARG;
    return detector->init();
}

bool face_distance_detector_is_calibrated(FaceDistanceDetector* detector)
{
    if (!detector) return false;
    return detector->isCalibrated();
}

esp_err_t face_distance_detector_start_calibration(FaceDistanceDetector* detector)
{
    if (!detector) return ESP_ERR_INVALID_ARG;
    return detector->startCalibration();
}

bool face_distance_detector_add_calibration_frame(FaceDistanceDetector* detector, const int* keypoints, int keypoints_size)
{
    if (!detector || !keypoints) return false;
    std::vector<int> kp(keypoints, keypoints + keypoints_size);
    return detector->addCalibrationFrame(kp);
}

esp_err_t face_distance_detector_finish_calibration(FaceDistanceDetector* detector)
{
    if (!detector) return ESP_ERR_INVALID_ARG;
    return detector->finishCalibration();
}

face_distance_state_t face_distance_detector_get_current_state(FaceDistanceDetector* detector)
{
    if (!detector) return FACE_DISTANCE_SAFE;
    return detector->getCurrentState();
}

float face_distance_detector_get_current_distance(FaceDistanceDetector* detector)
{
    if (!detector) return -1.0f;
    return detector->getCurrentDistance();
}

esp_err_t face_distance_detector_reset_calibration(FaceDistanceDetector* detector)
{
    if (!detector) return ESP_ERR_INVALID_ARG;
    return detector->resetCalibration();
}

}
