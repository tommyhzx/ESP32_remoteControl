#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

// WiFi状态机的状态
enum WiFiState
{
    WIFI_IDLE,
    WIFI_AP_CONFIG,
    WIFI_STA_MODE,
    WIFI_CONNECTING,
    WIFI_CONNECTED
};
// WiFi设备结构体
struct WiFiDevice
{
    // WiFi配置信息
    String ssid;
    String password;
    WiFiState state;
    String AP_name;
    // MQTT设备信息
    String type;
    String Name;
    String proto;
};
WiFiDevice device = {"", "", WIFI_IDLE, "defalut_mac", "002", "wifi设备", "3"}; // 初始化WiFi设备

WiFiUDP Udp;
// 根据需要修改的信息
String type = "002";  // 设备类型，001插座设备，002灯类设备，003风扇设备,005空调，006开关，009窗帘
String Name = "台灯"; // 设备昵称，可随意修改
String proto = "3";   // 3是tcp设备端口8344,1是MQTT设备

unsigned long previousMillis = 0;
const long interval = 1000; // 1秒

// AP配网
bool config_AP(String AP_name, String &wifi_ssid, String &wifi_pwd)
{
    char packetBuffer[255]; // 发送数据包
    String topic = AP_name;
    // 不是AP模式时，打开AP热点
    // 不是WIFI_AP，也不是WIFI_AP_STA

    if (WiFi.getMode() != WIFI_AP && WiFi.getMode() != WIFI_AP_STA)
    {
        Serial.println("StartAP wifimode is : " + String(WiFi.getMode()));
        WiFi.softAP("ESP32" + AP_name);
        Udp.begin(8266);
    }
    else
    { // 已经连接，等待配网信息
        int packetSize = Udp.parsePacket();
        if (packetSize)
        {
            Serial.println("Received packet of size " + String(packetSize));
            IPAddress remoteIp = Udp.remoteIP();

            int len = Udp.read(packetBuffer, 255);
            if (len > 0)
            {
                packetBuffer[len] = 0;
            }
            Serial.println("UDP packet contents: " + String(packetBuffer));
            // 解析json数据
            StaticJsonDocument<200> doc;
            DeserializationError error = deserializeJson(doc, packetBuffer);
            if (error)
            {
                Serial.print(F("deserializeJson() failed: "));
                return false;
            }
            int cmdType = doc["cmdType"].as<int>();
            if (cmdType == 1)
            {
                const char *ssid = doc["ssid"];
                const char *password = doc["password"];
                const char *token = doc["token"];

                wifi_ssid = ssid;
                wifi_pwd = password;
                Serial.println("wifi_ssid:" + wifi_ssid + " wifi_pwd:" + wifi_pwd);
                save_config_EEPROM(ssid, password);
                // 收到信息，并回复
                String ReplyBuffer = "{\"cmdType\":2,\"productId\":\"" + topic + "\",\"deviceName\":\"" + String(device.Name) + "\",\"protoVersion\":\"" + String(device.proto) + "\"}";
                Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
                // Udp.write(ReplyBuffer.c_str()); /* ESP8266使用*/
                Udp.write((const uint8_t *)ReplyBuffer.c_str(), ReplyBuffer.length());
                Udp.endPacket();
            }
            else if (cmdType == 3)
            {
                // 配网信息传递结束
                WiFi.softAPdisconnect(true);
                return true;
            }
        }
    }
    // 未完成配网
    return false;
}
// 初始化wifi状态机，让wifi模块进入开始状态
void startWifiTask(const String &ssid, const String &password)
{
    device.ssid = ssid;
    device.password = password;
    device.state = WIFI_STA_MODE;
}
static uint32_t startConnectTime;
void wifiTask()
{
    unsigned long currentMillis = millis();

    switch (device.state)
    {
    case WIFI_IDLE:
        // 等待指令，进入其它状态
        break;

    case WIFI_AP_CONFIG:
        // 循环等待配网，直到配置完成，进入STA模式
        if (config_AP(device.AP_name, device.ssid, device.password))
        {
            Serial.println("change to WIFI_STA_MODE");
            device.state = WIFI_STA_MODE;
        }
        break;
    case WIFI_STA_MODE:
        // 不确定是否需要？？
        WiFi.disconnect();
        // 设置wifi为STA模式
        WiFi.mode(WIFI_STA);
        Serial.println("enter WIFI_STA_MODE! mode:" + String(WiFi.getMode()));       
        // 开始计时
        startConnectTime = millis();
        Serial.println("123startConnectTime:" + String(startConnectTime));
        // 连接WiFi
        WiFi.begin(device.ssid.c_str(), device.password.c_str());
        device.state = WIFI_CONNECTING;
        break;
    case WIFI_CONNECTING:
        static uint32_t ConnectingTime = millis();
        // 持续10s未连接，则重新进入AP模式
        if (millis() - startConnectTime >= 10000)
        {
            Serial.println("enter WIFI_AP_CONFIG! millis:" + String(millis()));
            Serial.println("startConnectTime:" + String(startConnectTime));
            device.state = WIFI_AP_CONFIG;
        }
        // 连接成功，进入已连接状态
        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.println("\r\nWiFi connected successfully!");
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
            device.state = WIFI_CONNECTED;
        }
        // 每秒打印一次连接信息
        if (millis() - ConnectingTime >= 1000)
        {
            Serial.println("Connecting...");
            ConnectingTime = millis();
        }
        break;

    case WIFI_CONNECTED:
        // 持续判断wifi是否连接，若断开，则进入未连接状态
        if (WiFi.status() != WL_CONNECTED)
        {
            // 当wifi断开，开始计时
            previousMillis = currentMillis;
            device.state = WIFI_CONNECTING;
        }
        break;

    default:
        break;
    }
}

// 对外接口，获取wifi状态
int getWifiState()
{
    return device.state;
}

bool isWifiConfigured()
{
    // 检查WiFi配置是否可用
    // 你可以在这里实现自己的逻辑
    // 例如，检查g_wifiSSID和g_wifiPassword是否为空
    return !g_wifiSSID.isEmpty() && !g_wifiPassword.isEmpty();
}
