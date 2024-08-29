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
                if (ssidStartIndex != -1 && ssidEndIndex != -1 
                && passwordStartIndex != -1 && passwordEndIndex != -1)
                {
                    receivedSSID = value.substring(ssidStartIndex, ssidEndIndex);
                    receivedPassword = value.substring(passwordStartIndex, passwordEndIndex);
                    Serial.println("receivedSSID:" + receivedSSID + " receivedPassword:" + receivedPassword);
                    // 标记新配置已接收
                    DeviceConfigReceived = true;
                }
            }
        }
    }
};

// 蓝牙初始化
void initBLE(String deviceName)
{
    BLEDevice::init(deviceName);
    BLEServer *pServer = BLEDevice::createServer();

    BLEService *pService = pServer->createService(SERVICE_UUID);
    BLECharacteristic *pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE);
    pCharacteristic->setCallbacks(new MyCallbacks());
    pCharacteristic->setValue("Hello World");
    pService->start();
    // 设置广播，使手机可以搜索到
    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->start();
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