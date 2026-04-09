// SPRESENSE × LINE の動作確認に使用するサンプルプログラム
// LINE → Webhook → MQTT → SPRESENSE の流れで、
// MQTTで撮影指示を受け取ったら写真を撮影し、指定したサーバーへHTTP送信します。
// 動作状態を把握するためのステータス送信機能も含みますが、今回は使用しません。
// クラウドからの指示例：{"requestToken":"cccccccc","command":"capture","timestamp":"yyyy-mm-ddThh:mm:ss.104Z"}
// VGA送信では環境によって失敗する場合があるため、QVGAに画質を落としています。
// MQTT通信にはPubSubClientではなくArduinoMqttClientライブラリを使用しています。
// ChatGPT を補助的に利用して作成し、動作確認と必要な調整を実施しています。
// コメント整理・配布用調整: S. Nakamura 2026-04-08

#include <LTE.h>
#include <Camera.h>
#include <ArduinoMqttClient.h>

// ===== LTE設定 =====
#define APP_LTE_APN       "your_apn"
#define APP_LTE_USER_NAME "your_user"
#define APP_LTE_PASSWORD  "your_pass"
#define APP_LTE_AUTH_TYPE LTE_NET_AUTHTYPE_CHAP

// ===== MQTT設定 =====
const char BROKER_HOST[] = "your_broker_server";   // まずは検証用
const int  BROKER_PORT   = 1883;

// 受信トピック：クラウド側からJSONで撮影指示が来る
const char SUB_TOPIC[]   = "your_subscribe_topic";

// 送信トピック：起動、撮影失敗、アップロード成功／失敗などを送信する
// 2026-04-11のハンズオンでは使用しません
const char PUB_TOPIC[]   = "your_publish_topic";

// ===== HTTP送信先 =====
const char HTTP_HOST[] = "your_http_server";  // サーバーIP
const int  HTTP_PORT   = 1880;
const char HTTP_PATH[] = "/your_path";

// ===== デバイス情報 =====
const char DEVICE_ID[] = "your_uniqueId";

// ===== グローバル =====
LTE lteAccess;                      // LTEアクセス管理オブジェクト
LTEClient netClient;                // MQTTの通信用クライアント
MqttClient mqttClient(netClient);

// HTTP送信用に別クライアントを使う
LTEClient httpClient;

// MQTTコールバックの撮影フラグ
volatile bool captureRequested = false;

// 最後に受信したメッセージ全体を保持（デバッグ用）
String lastCommand = "";

// 受信したrequestTokenを保持する
String pendingRequestToken = "";


// カメラエラーをシリアルに表示
void printCamErr(enum CamErr err);
// JSON文字列から "key":"value" の value を抜き出す簡易関数
String extractJsonStringValue(const String& json, const String& key);
// カメラ初期化
bool setupCamera();
// LTEの初期化と接続
bool setupLTE();
// brokerサーバーへの接続
bool connectMqtt();
// デバイスのステータスの送信
void publishStatus(const char* message);
// MQTTで受信したJSONを読み取り、capture命令なら撮影フラグを立てる
void onMqttMessage(int messageSize);
// httpサーバーへの写真の送信
bool sendJpegToHttpServer(uint8_t* buf, size_t len);
// 写真を撮影して送信
bool captureAndUpload();
// MQTT切断時の再接続処理
void reconnectMqttIfNeeded();


// カメラ初期化
bool setupCamera(){

  CamErr err = theCamera.begin();
  if (err != CAM_ERR_SUCCESS) {
    printCamErr(err);
    return false;
  }

  // 静止画フォーマット設定
  // QVGAサイズにする
  err = theCamera.setStillPictureImageFormat(
    CAM_IMGSIZE_QVGA_H,
    CAM_IMGSIZE_QVGA_V,
    CAM_IMAGE_PIX_FMT_JPG
  );
  
#if 0 // VGAサイズ、JPEG形式(こちらの方が画質はいいですが安定しません)
  err = theCamera.setStillPictureImageFormat(
    CAM_IMGSIZE_VGA_H,
    CAM_IMGSIZE_VGA_V,
    CAM_IMAGE_PIX_FMT_JPG
  );
#endif

  if (err != CAM_ERR_SUCCESS) {
    printCamErr(err);
    return false;
  }

  return true;
}


void setup(){

  Serial.begin(115200);
  while (!Serial) {}

  Serial.println("Spresense MQTT trigger + HTTP upload");

  // カメラのセット
  if (!setupCamera()) {
    Serial.println("Camera setup failed");
    while (1) delay(1000);
  }

  // LTEネットワークへの接続
  if (!setupLTE()) {
    Serial.println("LTE setup failed");
    while (1) delay(1000);
  }

  // mqttサーバーに接続
  if (!connectMqtt()) {
    Serial.println("Initial MQTT connect failed");
  }
}

void loop(){

  reconnectMqttIfNeeded();  // mqttが切断されていたら再接続を行う

  // MQTT受信処理
  mqttClient.poll();

  // 撮影フラグがたったら写真を撮影してアップロード
  if (captureRequested) {
    captureRequested = false;
    captureAndUpload();
  }

  delay(10);
}



// カメラエラーをシリアルに表示
void printCamErr(enum CamErr err){

  Serial.print("Camera Error: ");
  Serial.println((int)err);
}

// JSON文字列から "key":"value" の value を抜き出す簡易関数
// 例: extractJsonStringValue(payload, "requestToken") -> "cccccccc"
String extractJsonStringValue(const String& json, const String& key){

  String pattern = "\"" + key + "\"";
  int keyPos = json.indexOf(pattern);
  if (keyPos < 0) return "";

  int colonPos = json.indexOf(':', keyPos + pattern.length());
  if (colonPos < 0) return "";

  int firstQuote = json.indexOf('"', colonPos + 1);
  if (firstQuote < 0) return "";

  int secondQuote = json.indexOf('"', firstQuote + 1);
  if (secondQuote < 0) return "";

  return json.substring(firstQuote + 1, secondQuote);
}


// LTEの初期化と接続
bool setupLTE(){
  
  Serial.println("Starting LTE...");

  if (lteAccess.begin() != LTE_SEARCHING) {
    Serial.println("lteAccess.begin() failed");
    return false;
  }

  Serial.println("Attaching LTE...");
  if (!lteAccess.attach(APP_LTE_APN,
                        APP_LTE_USER_NAME,
                        APP_LTE_PASSWORD,
                        APP_LTE_AUTH_TYPE)) {
    Serial.println("LTE attach failed");
    return false;
  }

  Serial.println("LTE attached");
  return true;
}

// brokerサーバーへの接続
bool connectMqtt(){

  Serial.print("Connecting MQTT broker: ");
  Serial.println(BROKER_HOST);

  if (!mqttClient.connect(BROKER_HOST, BROKER_PORT)) {
    Serial.print("MQTT connect failed, error code = ");
    Serial.println(mqttClient.connectError());
    return false;
  }

  Serial.println("MQTT connected");

  mqttClient.onMessage(onMqttMessage);

  mqttClient.subscribe(SUB_TOPIC);
  Serial.print("Subscribed: ");
  Serial.println(SUB_TOPIC);

  publishStatus("booted");
  return true;
}

// デバイスのステータスの送信
void publishStatus(const char* message){

  mqttClient.beginMessage(PUB_TOPIC);
  mqttClient.print("{\"device\":\"");
  mqttClient.print(DEVICE_ID);
  mqttClient.print("\",\"status\":\"");
  mqttClient.print(message);
  mqttClient.print("\"}");
  mqttClient.endMessage();
}

// MQTT受信コールバック
// 今回は JSON を読んで command が capture のときだけ撮影フラグを立てる
void onMqttMessage(int messageSize){

  String payload = "";

  while (mqttClient.available()) {
    char c = (char)mqttClient.read();
    payload += c;
  }

  payload.trim();
  lastCommand = payload;

  Serial.print("MQTT received: ");
  Serial.println(payload);

  // JSONから必要な項目を抜き出す
  String requestToken = extractJsonStringValue(payload, "requestToken");
  String command = extractJsonStringValue(payload, "command");
  String timestamp = extractJsonStringValue(payload, "timestamp");

  // requestTokenを保存
  pendingRequestToken = requestToken;

  // デバッグ表示
  Serial.print("Parsed requestToken: ");
  Serial.println(requestToken);

  Serial.print("Parsed command: ");
  Serial.println(command);

  Serial.print("Parsed timestamp: ");
  Serial.println(timestamp);

  // commandがcaptureのときだけ撮影する
  if (command == "capture") {
    captureRequested = true;    // 撮影フラグを立てる
    Serial.println("capture command accepted");
  } else {
    Serial.println("command is not capture");
  }
}

// httpサーバーへの写真の送信
bool sendJpegToHttpServer(uint8_t* buf, size_t len){

  Serial.println("Connecting HTTP server...");

  if (!httpClient.connect(HTTP_HOST, HTTP_PORT)) {
    Serial.println("HTTP connect failed");
    return false;
  }

  httpClient.print("POST ");
  httpClient.print(HTTP_PATH);
  httpClient.println(" HTTP/1.1");

  httpClient.print("Host: ");
  httpClient.println(HTTP_HOST);

  httpClient.println("Connection: close");
  httpClient.println("Content-Type: image/jpeg");

  // デバイスIDをHTTPヘッダに付与
  httpClient.print("x-device-id: ");
  httpClient.println(DEVICE_ID);

  // MQTTで受け取ったrequestTokenをHTTPヘッダに引き継ぐ
  httpClient.print("x-requesttoken: ");
  httpClient.println(pendingRequestToken);

  httpClient.print("Content-Length: ");
  httpClient.println(len);

  httpClient.println();

  size_t sent = httpClient.write(buf, len);
  if (sent != len) {
    Serial.print("Write mismatch: ");
    Serial.print(sent);
    Serial.print(" / ");
    Serial.println(len);
  }

  Serial.println("----- HTTP response -----");
  unsigned long start = millis();
  while (httpClient.connected() || httpClient.available()) {
    while (httpClient.available()) {
      char c = httpClient.read();
      Serial.write(c);
      start = millis();
    }
    if (millis() - start > 10000) {
      Serial.println("\nHTTP response timeout");
      break;
    }
  }
  Serial.println("\n-------------------------");

  httpClient.stop();
  return true;
}

// 写真を撮影して送信
bool captureAndUpload(){

  Serial.println("Taking picture...");
  // 写真の撮影
  CamImage img = theCamera.takePicture();

  if (!img.isAvailable()) {
    Serial.println("Capture failed");
    publishStatus("capture_failed");
    return false;
  }

  uint8_t* jpgBuf = img.getImgBuff();
  size_t jpgSize = img.getImgSize();

  Serial.print("JPEG size: ");
  Serial.println(jpgSize);

  // requestTokenを表示
  Serial.print("Uploading with requestToken: ");
  Serial.println(pendingRequestToken);

  bool ok = sendJpegToHttpServer(jpgBuf, jpgSize);
  publishStatus(ok ? "upload_ok" : "upload_failed");
  return ok;
}

// MQTT切断時の再接続処理
void reconnectMqttIfNeeded(){

  if (mqttClient.connected()) return;

  Serial.println("MQTT disconnected. Reconnecting...");
  while (!mqttClient.connected()) {
    if (connectMqtt()) {
      break;
    }
    delay(3000);
  }
}
