#include <WebServer.h>
#include <WiFi.h>

// tcp客户端相关初始化，默认即可
WiFiClient TCPclient;
#define WIFI_CONNECT_TIMEOUT 15000 // 定义 WiFi 连接超时时间为 10 秒
#define TCP_RCV_PERIOD 100         // tcp接收数据的时间
// 最大字节数
#define MAX_PACKETSIZE 512
// 下面的2代表上传间隔是2秒
#define upDataTime 2 * 1000

// 重连时间
static uint32_t lastReconnectAttempt = 0;
// 心跳计数
static unsigned long preHeartTick = 0;
// 接收数据间隔
static unsigned long TcpClient_preTick = 0;
// 接收数据index
static unsigned int TcpClient_BuffIndex = 0;
static String TcpClient_Buff = "";
void setTCPMessageCallback(TCPMessageCallback callback)
{
  tcpMessageCallback = callback;
}
/* ***************************************** *
 * 初始化TCP客户端
 * ***************************************** */
void initTCPClient()
{

  // TCPclient.stop();
  // TCPclient.setTimeout(1000);
  // TCPclient.setNoDelay(true);
  // TCPclient.setSync(true);
}

/*******************************************
 *发送数据到TCP服务器
 *******************************************/
void sendtoTCPServer(String p)
{

  if (!TCPclient.connected())
  {
    Serial.println("Client is not readly");
    return;
  }
  TCPclient.print(p);
}
/*****************************************
 *初始化和服务器建立连接
 *****************************************/
void startTCPClient(char *serverAddr, int serverPort, char *uid, char *topic)
{
  TCPclient.stop();
  if (TCPclient.connect(serverAddr, serverPort))
  {
    Serial.println("Connected to server:");
    Serial.printf("%s:%d\r\n", serverAddr, serverPort);
    // 连接上TCP时，先发布一次订阅
    String tcpTemp = "";
    tcpTemp = "cmd=1&uid=" + String(uid) + "&topic=" + String(topic) + "\r\n";
    sendtoTCPServer(tcpTemp);
    Serial.println("sbscrib topic " + String(topic));

    TCPclient.setNoDelay(true);
  }
}

/* ****************************************
 * 处理TCP消息
 * *************************************** */
void handleTCPMessage()
{
  if (TCPclient.available())
  {
    // 读取数据
    char c = TCPclient.read();
    TcpClient_Buff += c;
    TcpClient_BuffIndex++;
    TcpClient_preTick = millis();
    // 当接收到的数据大于最大数据时，清空数据
    if (TcpClient_BuffIndex >= MAX_PACKETSIZE - 1)
    {
      TcpClient_BuffIndex = MAX_PACKETSIZE - 2;
      TcpClient_preTick = TcpClient_preTick - 200;
    }
    preHeartTick = millis();
  }
  // 上传数据
  if (millis() - preHeartTick >= upDataTime)
  {
    preHeartTick = millis();
    // 发送心跳
    sendtoTCPServer("ping\r\n");
  }
  // 当接收到数据时，且间隔大于200ms时，处理数据
  if ((TcpClient_Buff.length() >= 1) && (millis() - TcpClient_preTick >= 200))
  { // 当订阅后，其它设备发布信息将会自动接收
    TCPclient.flush();
    // 处理main的回调函数
    tcpMessageCallback(TcpClient_Buff);
    // 清空字符串，以便下次接收
    TcpClient_Buff = "";
    TcpClient_BuffIndex = 0;
  }
  // 检查开关是否超时
  // checkSwitchTimeout();
}

// TCP是否连接
bool isTCPConnected()
{
  return TCPclient.connected();
}