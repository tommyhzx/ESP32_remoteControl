#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// BLE Characteristic的接收回调函数
class MyCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        Serial.println("BLE device onWrite");
        String value = pCharacteristic->getValue();
        if (value.length() > 0)
        {
            // 接收到数据后，调用用户定义的回调函数，由用户处理
            if (onWriteCallback != nullptr)
            {
                onWriteCallback(value);
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
        Serial.println("BLE device disconnected");
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
    Serial.println("deinitBLE");
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
// 通过notify发送数据
void sendNotifyData(String data)
{
    if (pDeviceCharacteristic != nullptr)
    {
        pDeviceCharacteristic->setValue(data);
        pDeviceCharacteristic->notify();
    }
}
// 设置回调函数
void setOnWriteCallback(OnWriteCallback callback)
{
    onWriteCallback = callback;
}