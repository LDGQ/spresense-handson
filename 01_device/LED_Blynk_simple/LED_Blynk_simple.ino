// SPRESENSEの基板に実装されているLEDを順番に点灯させるだけのシンプルなプログラムです
// 公式のSpresense Arduinoスタートガイドより流用
// https://developer.spresense.sony-semicon.com/development-guides/index?page=arduino_set_up&lang=ja#_led_%E3%81%AE%E3%82%B9%E3%82%B1%E3%83%83%E3%83%81%E3%82%92%E5%8B%95%E3%81%8B%E3%81%97%E3%81%A6%E3%81%BF%E3%82%8B


#if 1 // ビルトインLEDを使用したLチカ
void setup() {
    pinMode(LED0, OUTPUT);
    pinMode(LED1, OUTPUT);
    pinMode(LED2, OUTPUT);
    pinMode(LED3, OUTPUT);
}

void loop() {
    digitalWrite(LED0, HIGH);
    delay(100);
    digitalWrite(LED1, HIGH);
    delay(100);
    digitalWrite(LED2, HIGH);
    delay(100);
    digitalWrite(LED3, HIGH);
    delay(1000);

    digitalWrite(LED0, LOW);
    delay(100);
    digitalWrite(LED1, LOW);
    delay(100);
    digitalWrite(LED2, LOW);
    delay(100);
    digitalWrite(LED3, LOW);
    delay(1000);
}
#endif


#if 0   // Arduino拡張基板のGPIOを使用したLチカ
const int LED_PIN = 0;  // Arduino拡張基板のD0

void setup() {
  pinMode(LED_PIN, OUTPUT);   // GPIOを出力に設定
}

void loop() {
  digitalWrite(LED_PIN, HIGH); // GPIO High
  delay(1000);
  digitalWrite(LED_PIN, LOW);  // GPIO Low
  delay(1000);
}
#endif


