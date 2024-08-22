#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
// #include <WiFi.h>


#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
/*

*/
// 4----->板子的4引脚 GPIO4
// 3----->板子的3引脚 GPIO3
//
// 定义引脚
#define ADC_humidity 3
#define ledPin 8
#define SWITCH_Pin 2
#define sensorPin 0
// 定义状态
#define LED_ON 0
#define LED_OFF 1
#define SWITCH_ON 1
#define SWITCH_OFF 0

class MyCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    String value = pCharacteristic->getValue();

    if (value.length() > 0)
    {
      Serial.println("*********");
      Serial.print("New value: ");
      for (int i = 0; i < value.length(); i++)
        Serial.print(value[i]);

      Serial.println();
      Serial.println("*********");
    }
  }
};
// 设置wifi的账号密码全局变量，在setup和loop中使用
String g_wifiSSID = "";
String g_wifiPassword = "";
String mac = "your_mac_address"; // 替换为实际的MAC地址
void setup()
{
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);

  // 初始化ADC引脚
  pinMode(ADC_humidity, INPUT); // declare the sensorPin as an INPUT
  // 初始化IO引脚
  pinMode(ledPin, OUTPUT);
  pinMode(SWITCH_Pin, OUTPUT);
  setLEDStatus(LED_OFF);
  SwitchSet(SWITCH_OFF);
  delay(15 * 1000);
  Serial.println("System start!");
  // 检查是否需要清除EEPROM
  checkAndClearEEPROM();
  // 等待3s，若没有复位，则认为正常启动，清除eeprom复位标志
  delay(3 * 1000);
  resetResetFlag();
  // 先从EEPROM读取WIFI相关配置信息
  read_SSID_eeprom(g_wifiSSID, g_wifiPassword);
  Serial.println("g_wifiSSID:" + String(g_wifiSSID));
  Serial.println("g_wifiPassword:" + String(g_wifiPassword));
  // 初始化WiFi,将wifi配置传递进wifi模块
  startWifiTask(g_wifiSSID, g_wifiPassword);
  // WiFi.mode(WIFI_OFF);

  analogReadResolution(12);

  BLEDevice::init("MyESP32");
  BLEServer *pServer = BLEDevice::createServer();

  BLEService *pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
          BLECharacteristic::PROPERTY_WRITE);
  pCharacteristic->setCallbacks(new MyCallbacks());
  pCharacteristic->setValue("Hello World");
  pService->start();
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->start();
}

void loop()
{
  wifiTask();

  // put your main code here, to run repeatedly:
  // read the value from the sensor:
  // int sensorValue = analogRead(sensorPin);
  // // turn the ledPin on
  // digitalWrite(ledPin, HIGH);
  // // stop the program for <sensorValue> milliseconds:
  // delay(1000);
  // // turn the ledPin off:
  // digitalWrite(ledPin, LOW);
  // // stop the program for for <sensorValue> milliseconds:
  // delay(1000);

  // Serial.println(sensorValue);
}
