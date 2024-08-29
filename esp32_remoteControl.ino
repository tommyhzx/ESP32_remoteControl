
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

// 蓝牙模块的全景变量
extern String receivedSSID;
extern String receivedPassword;
extern bool DeviceConfigReceived;

// 设置wifi的账号密码全局变量，在setup和loop中使用
String g_wifiSSID = "maoshushu";
String g_wifiPassword = "20240110111";
String mac = "ESP32";        // 替换为实际的MAC地址
String deviceName = "ESP32"; // 替换为实际的设备名称
// 定义状态机状态
// WiFi状态机的状态
enum DeviceState
{
  DEV_INIT,
  DEV_IDLE,
  DEV_CONFIGURING,
  DEV_RUNNING,
  DEV_SLEEP
};
DeviceState currentState = DEV_INIT;
DeviceState previousState = DEV_INIT;
unsigned long lastStateChange = 0;
unsigned long currentMillis = 0;
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
  // 调试用
  save_config_EEPROM(g_wifiSSID, g_wifiPassword);
  // 先从EEPROM读取WIFI相关配置信息
  read_SSID_eeprom(g_wifiSSID, g_wifiPassword);
  Serial.println("g_wifiSSID:" + String(g_wifiSSID));
  Serial.println("g_wifiPassword:" + String(g_wifiPassword));
  // 初始化WiFi,将wifi配置传递进wifi模块
  // startWifiTask(g_wifiSSID, g_wifiPassword);
  // 初始化蓝牙
  // initBLE(deviceName);

  analogReadResolution(12);
  // 初始化状态机相关变量
  changeState(DEV_INIT);
  // lastStateChange = millis();
}

void loop()
{
  currentMillis = millis();

  performStateActions(); // 执行状态动作逻辑
  // wifi循环
  // wifiTask();

  // 检查是否接收到新的WiFi配置信息
  // if (DeviceConfigReceived)
  // {
  //   // 保存到EEPROM
  //   save_config_EEPROM(receivedSSID, receivedPassword);
  //   // 重新启动WiFi连接
  //   startWifiTask(receivedSSID, receivedPassword);
  //   // 重置标志
  //   DeviceConfigReceived = false;
  // }
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
// 状态机在当前状态需要执行的动作
void performStateActions()
{
  switch (currentState)
  {
  /* 设备初始化，先尝试连接wifi，若10s未连接则进入idle状态 */
  case DEV_INIT:
    // 检查是否连接成功
    if (isWifiConnected())
    {
      changeState(DEV_RUNNING);
    }
    else if (millis() - lastStateChange >= 10000) // 超过10秒未连接成功
    {
      Serial.println("lastStateChange" + String(lastStateChange));
      changeState(DEV_IDLE);
    }
    break;
  /* 空闲状态：配置AP模式和蓝牙广播
   * 等待接收AP的配网信息和蓝牙的配网信息*/
  case DEV_IDLE:
    // AP模式
    if (enterAPMode(deviceName, g_wifiSSID, g_wifiPassword))
    {
      Serial.println("change to WIFI_STA_MODE");
      changeState(DEV_CONFIGURING);
    }
    // 检查蓝牙是否接收到新的WiFi配置信息
    if (isBluetoothDataReceived())
    {
      bluetoothGetWifiConfig(g_wifiSSID, g_wifiPassword);
      Serial.println("bluetoothGetWifiConfig g_wifiSSID:" + g_wifiSSID + " g_wifiPassword:" + g_wifiPassword);
      // 重置标志
      clearBluetoothReceivedDataFlag();
      changeState(DEV_CONFIGURING);
    }
    break;
  // 判断WIFI是否连接，连接则进入运行状态，否则进入idle状态
  case DEV_CONFIGURING:
    static unsigned long lastPrintTime = 0;
    // 每秒打印一次 "Connecting..."
    if (currentMillis - lastPrintTime >= 1000)
    {
      Serial.println("Connecting...");
      lastPrintTime = currentMillis;
    }
    // 检查是否连接成功
    if (isWifiConnected())
    {
      Serial.println("WIFI连接成功");
      // 连接成功后，将wifi账号密码保存到EEPROM
      save_config_EEPROM(g_wifiSSID, g_wifiPassword);
      changeState(DEV_RUNNING);
    }
    else if (currentMillis - lastStateChange >= 10000) // 超过10秒未连接成功
    {
      Serial.println("WIFI连接超时");
      changeState(DEV_IDLE);
    }
    break;
  // 一直检测wifi是否连接，若断开则进入idle状态
  case DEV_RUNNING:
    if (!isWifiConnected())
    {
      changeState(DEV_IDLE);
    }
    break;
  case DEV_SLEEP:
    break;
  default:
    break;
  }
}
/* 状态机切换时执行的动作 */
void changeState(DeviceState newState)
{
  currentState = newState;
  lastStateChange = millis(); // 更新状态切换时间
  switch (currentState)
  {
  // 进入初始化状态，初始化蓝牙和wifi
  case DEV_INIT:

    startWifiStation(g_wifiSSID, g_wifiPassword);
    Serial.println("初始化");
    break;
  case DEV_IDLE:
    // 若wifi没有连接，则打开蓝牙
    initBLE(deviceName);
    Serial.println("空闲");
    break;
  // 当状态从idle切换到configuring时，需要将wifi设置为STA模式
  case DEV_CONFIGURING:
    Serial.println("配置中");
    // 启动wifi STA模式
    startWifiStation(g_wifiSSID, g_wifiPassword);
    break;
  case DEV_RUNNING:
    Serial.println("运行中");
    break;
  case DEV_SLEEP:
    Serial.println("睡眠");
    break;
  }
}