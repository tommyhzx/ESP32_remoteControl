#include <Arduino.h> // 添加这个头文件

#define SERVICE_UUID "4FAFC201-1FB5-459E-8FCC-C5C9C331914B"
#define CHARACTERISTIC_UUID "BEB5483E-36E1-4688-B7F5-EA07361B26A8"
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
// ESP-01s的状态指示灯 GPIO2, wifi正在连接时，led闪烁，无wifi连接，led点亮，连接上，led灭
const int LED_Status = 0;
// 蓝牙模块的全景变量
bool g_DeviceConfigReceived; // 是否接收到新的蓝牙配置信息

// 蓝牙接收的回调函数，供外部调用
typedef void (*OnWriteCallback)(const String &rcvData);
OnWriteCallback onWriteCallback = nullptr;

// 定义TCP回调函数
typedef void (*TCPMessageCallback)(const String &message);
TCPMessageCallback tcpMessageCallback = nullptr;

// 设置wifi的账号密码全局变量，在setup和loop中使用
String g_wifiSSID = "maoshushu";
String g_wifiPassword = "202401101";
String mac = "ESP32-1";        // 替换为实际的MAC地址
String deviceName = "ESP32-1"; // 替换为实际的设备名称

// TCP连接的全局变量
char *serverAddr = "bemfa.com";
int serverPort = 8344;
char *uid = "c2421290f7d14fa38251e5f77aac931a";
char *topic = "SN001001001";

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
  // 设置ADC分辨率为12位
  analogReadResolution(12);
  // 延迟15s，等待串口打印
  delay(15 * 1000);
  Serial.println("System start!");
  // 检查是否需要清除EEPROM
  checkAndClearEEPROM();
  // 等待3s，若没有复位，则认为正常启动，清除eeprom复位标志
  delay(3 * 1000);
  resetResetFlag();
  // 调试用
  // save_config_EEPROM(g_wifiSSID, g_wifiPassword);
  // 先从EEPROM读取WIFI相关配置信息
  read_SSID_eeprom(g_wifiSSID, g_wifiPassword);
  Serial.println("g_wifiSSID:" + String(g_wifiSSID));
  Serial.println("g_wifiPassword:" + String(g_wifiPassword));

  // 初始化状态机相关变量
  changeState(DEV_INIT);
}

void loop()
{
  currentMillis = millis();

  performStateActions(); // 执行状态动作逻辑
  // put your main code here, to run repeatedly:
  // read the value from the sensor:
  // int sensorValue = analogRead(sensorPin);
  // Serial.println(sensorValue);
}
// 状态机在当前状态需要执行的动作
static unsigned long lastPrintTime = 0; // 上次打印时间
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
      Serial.println("连接超时，请重新配置wifi账号密码");
      changeState(DEV_IDLE);
    }
    // 每1秒打印一次WiFi.status()
    if (millis() - lastPrintTime >= 1000)
    {
      lastPrintTime = millis();
      Serial.println("Connecting...");
    }
    break;

  /* 空闲状态：配置AP模式和蓝牙广播
   * 等待接收AP的配网信息和蓝牙的配网信息 */
  case DEV_IDLE:
    // AP模式
    if (enterAPMode(deviceName, g_wifiSSID, g_wifiPassword))
    {
      Serial.println("完成配网，返回STA模式");
      changeState(DEV_CONFIGURING);
    }
    // 检查蓝牙是否接收到新的WiFi配置信息
    if (g_DeviceConfigReceived == true)
    {
      // 重置标志
      g_DeviceConfigReceived = false;
      changeState(DEV_CONFIGURING);
    }
    break;

  // 判断WIFI是否连接，连接则进入运行状态，否则进入idle状态
  case DEV_CONFIGURING:
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
    static unsigned long runningStartTime = 0;
    static bool bleClosed = false; // 新增变量，标记蓝牙是否已关闭

    // TCP重连时间
    static uint32_t lastReconnectAttempt = 0;

    if (runningStartTime == 0)
    {
      runningStartTime = millis(); // 记录进入DEV_RUNNING状态的时间
      bleClosed = false;           // 重置蓝牙关闭标志
    }

    if (isWifiConnected())
    {
      // 检查是否已经运行了10分钟，超过10分钟关闭蓝牙
      if (!bleClosed && millis() - runningStartTime >= 10 * 1 * 1000) // 检查是否已经运行了10分钟
      {
        Serial.println("10分钟已到，关闭蓝牙");
        deinitBLE();      // 关闭蓝牙
        bleClosed = true; // 设置蓝牙关闭标志
      }
      // 判断TCP是否连接，若未连接则重连
      if (!isTCPConnected())
      {
        // 每秒尝试重连一次
        uint32_t currentMillis = millis();
        if (currentMillis - lastReconnectAttempt > 1 * 1000)
        {
          lastReconnectAttempt = currentMillis;
          Serial.println("TCP Client is not connected. Attempting to connect...");
          // 调用 startTCPClient 函数
          startTCPClient(serverAddr, serverPort, uid, topic);
        }
      }
      else
      {
        // 处理TCP消息
        handleTCPMessage();
      }
    }
    else
    {
      // wifi断开，进入idle状态
      changeState(DEV_INIT);
      runningStartTime = 0; // 重置计时器
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
    startWifiStation(g_wifiSSID, g_wifiPassword); // 启动wifi STA模式
    initBLE(deviceName);                          // 初始化蓝牙
    setOnWriteCallback(BLERecvCallback);          // 设置蓝牙收到消息的回调函数
    // 注册TCP回调函数
    setTCPMessageCallback(handleTCPMessageFromMain);
    Serial.println("初始化");
    break;

  case DEV_IDLE:
    Serial.println("进入空闲");
    break;

  // 当状态从idle切换到configuring时，需要将wifi设置为STA模式
  case DEV_CONFIGURING:
    Serial.println("进入配置中");
    // 启动wifi STA模式
    startWifiStation(g_wifiSSID, g_wifiPassword);
    break;

  case DEV_RUNNING:
    Serial.println("进入运行中");
    // 向蓝牙发送WIFI连接成功的消息
    sendNotifyData("WIFI_CONNECT");
    break;

  case DEV_SLEEP:
    Serial.println("睡眠");
    break;
  }
}
// 蓝牙接收数据的回调函数
void BLERecvCallback(const String &rcvData)
{
  Serial.println("BLERecvCallback");
  // 解析配置信息,格式为CONFIG:SSID:ssdi-PASSWORD:password
  if (rcvData.startsWith("CONFIG:"))
  {
    int ssidStartIndex = rcvData.indexOf("SSID:") + 5; // "SSID:" 后面即为 ssid 的起始位置
    int ssidEndIndex = rcvData.indexOf("-PASSWORD:");
    int passwordStartIndex = ssidEndIndex + 10; // "-PASSWORD:" 后面即为 password 的起始位置
    int passwordEndIndex = rcvData.indexOf(";");

    int separatorIndex = rcvData.indexOf(':', 7); // 从第7个字符开始查找下一个 ':'
    if (ssidStartIndex != -1 && ssidEndIndex != -1 && passwordStartIndex != -1 && passwordEndIndex != -1)
    {
      // 设置新的wifi账号密码
      g_wifiSSID = rcvData.substring(ssidStartIndex, ssidEndIndex);
      g_wifiPassword = rcvData.substring(passwordStartIndex, passwordEndIndex);
      Serial.println("g_wifiSSID:" + g_wifiSSID + " g_wifiPassword:" + g_wifiPassword);
      // 标记新配置已接收
      g_DeviceConfigReceived = true;
      // 发送确认消息
      sendNotifyData("CONFIG_DONE");
    }
  }
}

// 处理TCP接收到的消息
void handleTCPMessageFromMain(const String &message)
{
  // 有时会连续收到包，若接收到的最后的数据中包含&msg=，则说明是最终数据
  int lastIndex = message.lastIndexOf("&msg=");
  if (lastIndex != -1)
  {
    // 去除掉&msg=这5个数据
    String msgValue = message.substring(lastIndex + 5);
    msgValue.trim();
    Serial.println("msgValue is " + msgValue);
    if (msgValue.equals("on"))
    {
      SwitchSet(SWITCH_ON);
      // switch_on_tick = millis();
      // switch_on = true;
    }
    else if (msgValue.equals("off"))
    {
      SwitchSet(SWITCH_OFF);
      // switch_on = false;
    }
  }
}
