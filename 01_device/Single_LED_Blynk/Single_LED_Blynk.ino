// SPRESENSEの基板に実装されているLEDを順番に点灯させるだけのシンプルなプログラムです
// ビルトインLEDを使用したLチカ
// 2026-04-03 S.Nakamura

void setup() {

  // シリアル通信を開始する
  Serial.begin(115200); 
  delay(500);
  Serial.println("Blynk LED start");

  // 使用するLEDを初期化する
  pinMode(LED0, OUTPUT);
}

void loop() {

  // LEDをオンにする
  Serial.println("LED on");
  digitalWrite(LED0, HIGH);
  delay(1000);

  // LEDをオフにする
  Serial.println("LED off");
  digitalWrite(LED0, LOW);
  delay(1000);
}




