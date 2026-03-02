// PCとのシリアル通信確認用サンプル
// 115200bpsで接続し、1秒ごとにカウントアップします。
// Arduinoシリアルモニタで確認してください。

int count;

void setup() {
  
  Serial.begin(115200);
  delay(500);
  Serial.println("count up start!");

}

void loop() {
  
  Serial.printf("count is %d\n", count);

  count += 1;

  delay(1000);
}
