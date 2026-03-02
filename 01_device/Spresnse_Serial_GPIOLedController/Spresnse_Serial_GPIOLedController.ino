// シリアルから入ってきた信号に応じてGPIOのLEDを操作するプログラム
// Arduinoのシリアルコンソールからの操作を想定している

// LINEとの連携を想定している。
// ローカルPCにNode-REDサーバーを立ててLINEを受信することでLINEとの連携が可能
// webhook →シリアルでSPRESENSEを操作という形になっている
// @author S.Nakamura 2026-02-03

// 0：GPIOのLEDが点灯
// x：LEDをオフにする
// s：ステータスを返す

#define LED_PIN 0

volatile bool isLEDstatus = 0;


void LedSet();
void TurnOnLed();
void TurnOffLed();
void HandleSerialCommand(char cmd);


void setup() {

  Serial.begin(115200);
  delay(500);
  Serial.println("Serial LED Controller start!");

  // LEDの初期化
  LedSet();
  TurnOffLed();
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

  pinMode(LED_PIN, OUTPUT);
}

// 指定したLEDをオフにする
void TurnOffLed() {

  digitalWrite(LED_PIN, LOW);
  isLEDstatus = 0;
}

// 指定したLEDをオンにする
void TurnOnLed() {
  
  digitalWrite(LED_PIN, HIGH);
  isLEDstatus = 1;
}

// コマンドに応じた処理をする
// 引数：コマンド
void HandleSerialCommand(char cmd) {
  switch (cmd) {
    case '0': 
      TurnOnLed();
      // オンになったことを伝える
      Serial.println(isLEDstatus);    
      Serial.println(""); 
      break;

    case 'x': 
      TurnOffLed(); 
      // オフになったことを伝える
      Serial.println(isLEDstatus);
      Serial.println("");
      break;

    case 's': // all on
      // 状態の確認
      if(isLEDstatus){
        Serial.println("on");
        Serial.println("");
      }else{
        Serial.println("off");
        Serial.println("");
      }

      break;

    case 'h': // ヘルプ機能
      // 未実装
      break;

    default:
      // Serial.print("Unknown command: ");
      // Serial.println(cmd);
      break;
  }
}



