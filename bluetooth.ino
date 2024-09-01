#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// 设备配网数据接收标志
// 声明全局变量
String receivedSSID = "";
String receivedPassword = "";
bool DeviceConfigReceived = false;

class MyCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        Serial.println("BLE device onWrite");
        String value = pCharacteristic->getValue();

        if (value.length() > 0)
        {
            Serial.println("*********");
            for (int i = 0; i < value.length(); i++)
                Serial.print(value[i]);

            Serial.println();
            Serial.println("*********");
            // 解析配置信息,格式为CONFIG:SSID:ssdi-PASSWORD:password
            if (value.startsWith("CONFIG:"))
            {
                int ssidStartIndex = value.indexOf("SSID:") + 5; // "SSID:" 后面即为 ssid 的起始位置
                int ssidEndIndex = value.indexOf("-PASSWORD:");
                int passwordStartIndex = ssidEndIndex + 10; // "-PASSWORD:" 后面即为 password 的起始位置
                int passwordEndIndex = value.indexOf(";");

                int separatorIndex = value.indexOf(':', 7); // 从第7个字符开始查找下一个 ':'
                if (ssidStartIndex != -1 && ssidEndIndex != -1 && passwordStartIndex != -1 && passwordEndIndex != -1)
                {
                    receivedSSID = value.substring(ssidStartIndex, ssidEndIndex);
                    receivedPassword = value.substring(passwordStartIndex, passwordEndIndex);
                    Serial.println("receivedSSID:" + receivedSSID + " receivedPassword:" + receivedPassword);
                    // 标记新配置已接收
                    DeviceConfigReceived = true;

                    // 发送确认消息
                    // pCharacteristic->setValue("CONFIG_DONE");
                    // pCharacteristic->notify(); // 发送通知
                }
            }
        }
    };

    void onRead(BLECharacteristic *pCharacteristic)
    {
        Serial.println("BLE device read");
    };
};
// BLEServer 回调类
class ServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer)
    {
        Serial.println("BLE device connected");
    }

    void onDisconnect(BLEServer *pServer)
    {
        Serial.print("BLE device disconnected server is ");
        Serial.println(pServer->getConnId());
        // 重新启动蓝牙广播
        BLEAdvertising *pAdvertising = pServer->getAdvertising();
        pAdvertising->start();
    }
};
// 蓝牙初始化
BLEServer *pDevice_Service = nullptr;
BLECharacteristic *pDeviceCharacteristic = nullptr;
void initBLE(String deviceName)
{
    BLEDevice::init(deviceName);
    pDevice_Service = BLEDevice::createServer();
    // 设置服务器回调
    pDevice_Service->setCallbacks(new ServerCallbacks());
    // 创建服务
    BLEService *pService = pDevice_Service->createService(SERVICE_UUID);
    // 创建特征
    pDeviceCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_NOTIFY);
    // 设置回调
    pDeviceCharacteristic->setCallbacks(new MyCallbacks());
    pDeviceCharacteristic->setValue("Hello World");
    pService->start();
    // 设置广播，使手机可以搜索到
    BLEAdvertising *pAdvertising = pDevice_Service->getAdvertising();
    pAdvertising->start();
}
/*
 *   关闭蓝牙
*/
void deinitBLE()
{
    if (pDevice_Service != nullptr)
    {
        // 停止广播
        BLEAdvertising *pAdvertising = pDevice_Service->getAdvertising();
        if (pAdvertising != nullptr)
        {
            pAdvertising->stop();
        }
        // 停止服务
        BLEService *pService = pDevice_Service->getServiceByUUID(SERVICE_UUID);
        if (pService != nullptr)
        {
            pService->stop();
        }
        // 删除特征
        if (pDeviceCharacteristic != nullptr)
        {
            pDeviceCharacteristic->setCallbacks(nullptr);
            pDeviceCharacteristic = nullptr;
        }
        // 删除服务
        pDevice_Service->removeService(pService);
        // 删除服务器
        pDevice_Service->setCallbacks(nullptr);
        pDevice_Service = nullptr;
        // 释放蓝牙设备
        BLEDevice::deinit();
    }
}

// 检查蓝牙是否接收到数据
bool isBluetoothDataReceived()
{
    return DeviceConfigReceived;
}
// 清除蓝牙数据接收标志
void clearBluetoothReceivedDataFlag()
{
    DeviceConfigReceived = false;
}

// 获取接收到的wifi配置信息
void bluetoothGetWifiConfig(String &ssid, String &password)
{
    ssid = receivedSSID;
    password = receivedPassword;
}
// 通过notify发送数据
void sendNotifyData(String data)
{
    if (pDeviceCharacteristic != nullptr)
    {
        pDeviceCharacteristic->setValue(data);
        pDeviceCharacteristic->notify();
    }
}