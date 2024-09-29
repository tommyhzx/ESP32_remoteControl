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

// 进入AP模式，等待配网完成
bool enterAPMode(String AP_name, String &wifi_ssid, String &wifi_pwd)
{
    char packetBuffer[255]; // 发送数据包
    String topic = AP_name;

    // 不是AP模式时，打开AP热点，不是WIFI_AP，也不是WIFI_AP_STA
    if (WiFi.getMode() != WIFI_AP && WiFi.getMode() != WIFI_AP_STA)
    {
        WiFi.softAP(AP_name);
        Udp.begin(8266);
        Serial.println("WiFi.getMode()" + WiFi.getMode());
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
bool isWifiConnected()
{
    // 检查WiFi是否连接
    return WiFi.status() == WL_CONNECTED;
}

// 启动station模式
void startWifiStation(const String &ssid, const String &password)
{
    // WiFi.disconnect();
    // Serial.println("WiFi.disconnect");
    WiFi.mode(WIFI_STA);
    wl_status_t res = WiFi.begin(ssid.c_str(), password.c_str());
    Serial.print("res:");
    Serial.println(res);
    // Serial.println("wifi begin ssid:" + ssid + " password:" + password);
    // scanAndPrintDevice();
}

void printWiFiStatus()
{
    int status = WiFi.status();
    switch (status)
    {
    case WL_IDLE_STATUS:
        Serial.println("WL_IDLE_STATUS: WiFi is in idle status.");
        break;
    case WL_NO_SSID_AVAIL:
        Serial.println("WL_NO_SSID_AVAIL: Cannot find the specified SSID.");
        break;
    case WL_SCAN_COMPLETED:
        Serial.println("WL_SCAN_COMPLETED: Scan completed.");
        break;
    case WL_CONNECTED:
        Serial.println("WL_CONNECTED: Successfully connected to WiFi.");
        break;
    case WL_CONNECT_FAILED:
        Serial.println("WL_CONNECT_FAILED: Connection failed.");
        break;
    case WL_CONNECTION_LOST:
        Serial.println("WL_CONNECTION_LOST: Connection lost.");
        break;
    case WL_DISCONNECTED:
        Serial.println("WL_DISCONNECTED: Disconnected from WiFi.");
        break;
    default:
        Serial.println("Unknown status code.");
        break;
    }
}
// 扫描可用的WiFi网络并打印
void scanAndPrintDevice()
{
    // 搜索可用的WiFi网络
    int n = WiFi.scanNetworks();
    Serial.println("扫描完成");
    if (n == 0)
    {
        Serial.println("没有找到可用的网络");
    }
    else
    {
        Serial.print(n);
        Serial.println(" 个网络被找到");
        for (int i = 0; i < n; ++i)
        {
            // 打印每个网络的详细信息
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(WiFi.SSID(i));
            Serial.print(" (");
            Serial.print(WiFi.RSSI(i));
            Serial.print(")");
            Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
            delay(10);
        }
    }
    Serial.println("");
}