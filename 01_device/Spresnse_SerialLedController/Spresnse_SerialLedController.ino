// シリアルから入ってきた信号に応じて光るLEDを変更するプログラム
// Arduinoのシリアルコンソールからの操作を想定している

// LINEとの連携を想定している。
// ローカルPCにNode-REDサーバーを立ててLINEを受信することでLINEとの連携が可能
// webhook →シリアルでSPRESENSEを操作という形になっている
// @author S.Nakamura 2026-02-03

// 0,1,2.3：それに応じたLEDが点灯
// a：すべてのLEDをONにする
// x：LEDをオフにする


void LedSet();                        // LEDの準備
void AllLedOff();                     // LEDのオフ
void TurnOnLed(uint8_t ledNo);        // 指示に応じたLEDを点灯する
void HandleSerialCommand(char cmd);   // シリアルからの指示に応じた処理


void setup() {

  Serial.begin(115200);
  delay(500);
  Serial.println("Serial LED Controller start!");

  // LEDの初期化
  LedSet();
  AllLedOff();
}

void loop() {

  // シリアルからの入力に応じてSPRESENSEを操作する
  if (Serial.available() > 0) {
    
    char cmd = Serial.read();
    HandleSerialCommand(cmd);
  }   
}


// LEDの準備
void LedSet(){

  pinMode(LED0, OUTPUT);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
}

// LEDのオフ
void AllLedOff() {
  digitalWrite(LED0, LOW);
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
}


// 指示に応じたLEDを点灯する
// 引数：点灯したいLED
void TurnOnLed(uint8_t ledNo) {

  AllLedOff();  // 1つだけ光らせたいので一旦全部消す

  switch (ledNo) {
    case 0: digitalWrite(LED0, HIGH); break;
    case 1: digitalWrite(LED1, HIGH); break;
    case 2: digitalWrite(LED2, HIGH); break;
    case 3: digitalWrite(LED3, HIGH); break;
  }
}


// シリアルからの指示に応じた処理
// 引数：コマンド
void HandleSerialCommand(char cmd) {
  switch (cmd) {
    case '0': TurnOnLed(0); break;
    case '1': TurnOnLed(1); break;
    case '2': TurnOnLed(2); break;
    case '3': TurnOnLed(3); break;

    case 'a': // all on
      digitalWrite(LED0, HIGH);
      digitalWrite(LED1, HIGH);
      digitalWrite(LED2, HIGH);
      digitalWrite(LED3, HIGH);
      break;

    case 'x': // all off
      AllLedOff();
      break;

    default:
      Serial.print("Unknown command: ");
      Serial.println(cmd);
      break;
  }
}



