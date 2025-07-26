/**
 ****************************************************************************************************
 * @file        face_distance_detector.hpp
 * @author      AI Assistant
 * @version     V1.0
 * @date        2025-07-24
 * @brief       基于姿态感知的鲁棒型人脸过近检测系统头文件
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

#ifndef __FACE_DISTANCE_DETECTOR_HPP
#define __FACE_DISTANCE_DETECTOR_HPP

#include "face_distance_c_interface.h"

#ifdef __cplusplus
#include <vector>
#include <list>
#include <queue>
#include <cmath>
#include "dl_detect_define.hpp"
#endif

#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* C接口函数声明 */
typedef struct FaceDistanceDetector FaceDistanceDetector;

/**
 * @brief 创建距离检测器实例
 */
FaceDistanceDetector* face_distance_detector_create(void);

/**
 * @brief 销毁距离检测器实例
 */
void face_distance_detector_destroy(FaceDistanceDetector* detector);

/**
 * @brief 初始化检测器
 */
esp_err_t face_distance_detector_init(FaceDistanceDetector* detector);

/**
 * @brief 检查是否已标定
 */
bool face_distance_detector_is_calibrated(FaceDistanceDetector* detector);

/**
 * @brief 开始标定程序
 */
esp_err_t face_distance_detector_start_calibration(FaceDistanceDetector* detector);

/**
 * @brief 添加标定帧数据
 */
bool face_distance_detector_add_calibration_frame(FaceDistanceDetector* detector, const int* keypoints, int keypoints_size);

/**
 * @brief 完成标定
 */
esp_err_t face_distance_detector_finish_calibration(FaceDistanceDetector* detector);

/**
 * @brief 获取当前状态
 */
face_distance_state_t face_distance_detector_get_current_state(FaceDistanceDetector* detector);

/**
 * @brief 获取当前平滑距离
 */
float face_distance_detector_get_current_distance(FaceDistanceDetector* detector);

/**
 * @brief 重置标定
 */
esp_err_t face_distance_detector_reset_calibration(FaceDistanceDetector* detector);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

/**
 * @brief 校正模型参数结构体
 */
typedef struct {
    float min_ratio;      /*!< 最小比例值 */
    float max_ratio;      /*!< 最大比例值 */
    float min_correction; /*!< 最小校正系数 */
    float max_correction; /*!< 最大校正系数 */
} pose_correction_params_t;

/**
 * @brief 人脸距离检测器类
 */
class FaceDistanceDetector {
private:
    // 配置参数
    static constexpr float KNOWN_DISTANCE_CM = 50.0f;     /*!< 标定距离 */
    static constexpr float ENTER_THRESHOLD_CM = 45.0f;    /*!< 进入过近状态阈值 - 调整为45cm便于测试 */
    static constexpr float EXIT_THRESHOLD_CM = 48.0f;     /*!< 退出过近状态阈值 - 调整为48cm便于测试 */
    static constexpr int FILTER_QUEUE_SIZE = 7;           /*!< 滤波队列大小 */
    static constexpr int CALIBRATION_FRAMES = 20;         /*!< 标定帧数 */
    
    // NVS存储键
    static constexpr char NVS_NAMESPACE[] = "face_dist";
    static constexpr char NVS_K_CONSTANT_KEY[] = "k_const";
    static constexpr char NVS_CALIBRATED_KEY[] = "calibrated";
    
    // 内部状态
    float k_constant_;                    /*!< 标定常数 */
    bool is_calibrated_;                  /*!< 是否已标定 */
    face_distance_state_t current_state_; /*!< 当前系统状态 */
    std::queue<float> filter_queue_;      /*!< 滤波队列 */
    pose_correction_params_t correction_params_; /*!< 姿态校正参数 */
    
    // 内部方法
    float calculateEyeDistance(const std::vector<int>& keypoints);
    float calculateYawRatio(const std::vector<int>& keypoints);
    float getPoseCorrection(float yaw_ratio);
    float getSmoothedDistance();
    void updateFilterQueue(float distance);
    esp_err_t saveToNVS();
    esp_err_t loadFromNVS();
    
public:
    /**
     * @brief 构造函数
     */
    FaceDistanceDetector();
    
    /**
     * @brief 析构函数
     */
    ~FaceDistanceDetector();
    
    /**
     * @brief 初始化检测器
     * @retval ESP_OK 成功
     * @retval ESP_FAIL 失败
     */
    esp_err_t init();
    
    /**
     * @brief 检查是否已标定
     * @retval true 已标定
     * @retval false 未标定
     */
    bool isCalibrated() const { return is_calibrated_; }
    
    /**
     * @brief 开始标定程序
     * @retval ESP_OK 成功
     * @retval ESP_FAIL 失败
     */
    esp_err_t startCalibration();
    
    /**
     * @brief 添加标定帧数据
     * @param keypoints 人脸关键点数据
     * @retval true 标定完成
     * @retval false 需要更多帧
     */
    bool addCalibrationFrame(const std::vector<int>& keypoints);
    
    /**
     * @brief 完成标定
     * @retval ESP_OK 成功
     * @retval ESP_FAIL 失败
     */
    esp_err_t finishCalibration();
    
    /**
     * @brief 处理一帧人脸数据
     * @param results 人脸检测结果
     * @retval 当前系统状态
     */
    face_distance_state_t processFrame(const std::list<dl::detect::result_t>& results);
    
    /**
     * @brief 获取当前状态
     * @retval 当前系统状态
     */
    face_distance_state_t getCurrentState() const { return current_state_; }
    
    /**
     * @brief 获取当前平滑距离
     * @retval 平滑后的距离值(cm)
     */
    float getCurrentDistance() const;
    
    /**
     * @brief 重置标定
     * @retval ESP_OK 成功
     * @retval ESP_FAIL 失败
     */
    esp_err_t resetCalibration();
    
    /**
     * @brief 设置阈值参数
     * @param enter_threshold 进入阈值
     * @param exit_threshold 退出阈值
     */
    void setThresholds(float enter_threshold, float exit_threshold);

private:
    // 标定相关
    std::vector<float> calibration_samples_; /*!< 标定样本 */
    bool calibration_in_progress_;           /*!< 标定进行中标志 */
};

#endif /* __cplusplus */

#endif /* __FACE_DISTANCE_DETECTOR_HPP */
