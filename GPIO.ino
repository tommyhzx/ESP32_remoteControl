
void SwitchSet(int status)
{
  digitalWrite(SWITCH_Pin, status);
}
// 控制指示灯
void setLEDStatus(int status)
{
  digitalWrite(ledPin, status);
}