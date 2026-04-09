// カメラで撮影した画像をhttpで送信します
// chat GPTで出力したものを動作確認したものになります
// 成功すると、サーバー側で画像が受信できます
// シリアルモニタに "Upload done" と表示されればOKです
// 2026-04-04 S.Nakamura

#include <Camera.h>
#include <LTE.h>


// ====== LTE設定 ======
// MEEQ SIMを前提にしている
#define APP_LTE_APN      "your_apn"
#define APP_LTE_USER_NAME "your_user"
#define APP_LTE_PASSWORD  "your_pass"

// 認証方式は環境に合わせて変更
#define APP_LTE_AUTH_TYPE LTE_NET_AUTHTYPE_CHAP

// ====== 送信先設定 ======
// まずは HTTPでサーバーに送信することを推奨
// ここだけ書き換えてください
// ==============================
const char* SERVER_HOST = "your_server";    // サーバーの設定（アップロードしたいサーバーを指定する）
const int   SERVER_PORT = 1880;             // 送信したいポートを指定する（今回はNode-REDのポート）
const char* SERVER_PATH = "/your_pass";     // サーバーのパスを指定する

const char* DEVICE_ID   = "spresense01";    // デバイスIDを任意につける

// ====== ネットワークの実体 ======
LTE lteAccess;
LTEClient client;

// カメラのエラー表示
void printCamErr(enum CamErr err){

  Serial.print("Camera Error: ");
  Serial.println((int)err);
}

// カメラの準備
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

// LTEをスタートする
bool setupLTE(){

  Serial.println("Starting LTE...");

  if (lteAccess.begin() != LTE_SEARCHING) {
    Serial.println("LTE begin failed");
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

// 撮影した写真をサーバーに送信する
bool sendJpegToServer(uint8_t* buf, size_t len){

  Serial.println("Connecting to server...");

  if (!client.connect(SERVER_HOST, SERVER_PORT)) {
    Serial.println("Connect failed");
    return false;
  }

  // HTTPヘッダ送信
  client.print("POST ");
  client.print(SERVER_PATH);
  client.println(" HTTP/1.1");

  client.print("Host: ");
  client.println(SERVER_HOST);

  client.println("Connection: close");
  client.println("Content-Type: image/jpeg");

  client.print("x-device-id: ");
  client.println(DEVICE_ID);

  client.print("Content-Length: ");
  client.println(len);

  client.println();

  // JPEG本体送信
  size_t sent = client.write(buf, len);
  if (sent != len) {
    Serial.print("Write size mismatch: ");
    Serial.print(sent);
    Serial.print(" / ");
    Serial.println(len);
  }

  // 応答表示
  Serial.println("----- response -----");
  unsigned long start = millis();
  while (client.connected() || client.available()) {
    while (client.available()) {
      char c = client.read();
      Serial.write(c);
      start = millis();
    }
    if (millis() - start > 10000) {
      Serial.println("\nResponse timeout");
      break;
    }
  }
  Serial.println("\n--------------------");

  client.stop();
  return true;
}

void setup(){

  Serial.begin(115200);
  // while (!Serial) {}
  delay(500);
  Serial.println("Spresense camera upload test");

  if (!setupCamera()) {
    Serial.println("Camera setup failed");
    while (1) delay(1000);
  }

  if (!setupLTE()) {
    Serial.println("LTE setup failed");
    while (1) delay(1000);
  }

  Serial.println("Ready");
}

void loop(){

  Serial.println("Taking picture...");
  CamImage img = theCamera.takePicture();

  //  画像の取得に失敗したら5秒待つ
  if (!img.isAvailable()) {
    Serial.println("Failed to capture image");
    delay(5000);
    return;
  } 

  uint8_t* jpgBuf = img.getImgBuff();
  size_t jpgSize = img.getImgSize();

  // デバッグ情報
  Serial.print("Captured JPEG size: ");
  Serial.println(jpgSize);

  bool ok = sendJpegToServer(jpgBuf, jpgSize);
  Serial.println(ok ? "Upload done" : "Upload failed");

  delay(10000); // 10秒ごとに撮影
}
