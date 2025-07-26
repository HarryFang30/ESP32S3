#ifndef __WIFI_CONFIG_H__
#define __WIFI_CONFIG_H__

/* WiFi配置 - 请修改为你的实际WiFi信息 */
#define WIFI_SSID      "Harry Fang"       // 修改为你的WiFi名称
#define WIFI_PASSWORD  "19895525707"   // 修改为你的WiFi密码

/* 服务器配置 - 请修改为你的电脑IP地址 */
#define SERVER_URL     "http://172.20.10.9:5001/upload"  // 修改为你的电脑IP地址

/* 
 * 如何获取你的电脑IP地址：
 * 
 * Windows:
 * 1. 按 Win+R 打开运行对话框
 * 2. 输入 cmd 并按回车
 * 3. 在命令行中输入 ipconfig
 * 4. 找到 "无线局域网适配器" 下的 IPv4 地址
 * 
 * Mac/Linux:
 * 1. 打开终端
 * 2. 输入 ifconfig
 * 3. 找到 en0 或 wlan0 接口下的 inet 地址
 * 
 * 然后将上面的 192.168.1.100 替换为你的实际IP地址
 */

#endif /* __WIFI_CONFIG_H__ */
