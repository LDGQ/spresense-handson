// ------------------------------------------------------------
// SPRESENSE JPEG シリアル送信（動作確認用）
// ------------------------------------------------------------
// 目的：SPRESENSEで撮影したJPEG(320x240)をシリアルでPCへ送信し、PC側で受信・表示できることを確認する。
// 動作確認：921600bps / 1fps で連続送信し、PC側で受信・表示できることを確認済。
// ※本コードはChatGPTを用いて作成し、中村がコメント追記・動作確認を行っています（詳細な耐久検証は今後）。
//
// 位置づけ：本スケッチは「SPRESENSE → PC → LINE」のうち「SPRESENSE → PC」部分の検証用。
//          最終構成では、PCの部分がLTE通信モジュールに置き換わります。
//          LINE連携はPC側で画像保存・転送を実装します（ngrok等でPCへ到達できる必要があります）。
//
// 送信フレーム形式：
//   [SIG 4byte 'S''P''J''G'] + [size 4byte little-endian想定] + [jpeg size byte]
//
// 注意：
//  ・ボーレートは 921600 に設定すること
//  ・PC側はバイナリ（Buffer）で受け取ること（改行区切り/文字列扱いはNG）
//  ・LED0点滅が続く場合は、カメラ初期化またはJPEG設定に失敗しています
//  ・FPSを上げると転送が追いつかずフレームが崩れることがあります
//  ・Serial.flush() は送信完了待ちになるため、速度を上げたい場合は外す選択もあります
// ------------------------------------------------------------


#include <Camera.h>

// シリアルボーレート
// シリアルボーレート（転送速度）
// 921600: 速いが環境差あり / 460800: 安定寄り
static const uint32_t BAUDRATE = 921600;  // 動作確認済
// フレーム同期用シグネチャ（PC側はこの4byteを探してフレーム開始を検出）
static const uint8_t SIG[4] = {'S','P','J','G'};

// カメラの初期化の失敗時にはLEDの点滅
static void BlinkErrorForever() {
  pinMode(LED0, OUTPUT);
  while (1) {
    digitalWrite(LED0, HIGH); delay(200);
    digitalWrite(LED0, LOW);  delay(200);
  }
}


void setup() {
  pinMode(LED0, OUTPUT);
  digitalWrite(LED0, LOW);

  Serial.begin(BAUDRATE);
  while (!Serial) { delay(10); }  // PCとの接続を待つ（PCで使用する前提です）
  // delay(500);

  // カメラの初期化
  if (theCamera.begin() != CAM_ERR_SUCCESS) BlinkErrorForever();

  // JPEGの設定
  if (theCamera.setStillPictureImageFormat(
        CAM_IMGSIZE_QVGA_H,   // 320
        CAM_IMGSIZE_QVGA_V,   // 240
        CAM_IMAGE_PIX_FMT_JPG // JPEG
      ) != CAM_ERR_SUCCESS) BlinkErrorForever();
}

void loop() {

  // 撮影
  CamImage img = theCamera.takePicture();
  if (!img.isAvailable()) {
    // たまに失敗しても次へ（止めない）
    delay(50);
    return;
  }

  const uint32_t size = (uint32_t)img.getImgSize();
  const uint8_t* buf  = (const uint8_t*)img.getImgBuff();

  // フレーム送信：SIG + size + jpeg
  // エンディアンは合わせる必要があります。(Node-redの参考フローでは合わせています)
  Serial.write(SIG, 4);
  Serial.write((uint8_t*)&size, 4);
  Serial.write(buf, size);
  // 送信完了待ち
  Serial.flush();

  // FPS調整（まずは低めで安定させる）
  delay(1000);  // 1秒に一回更新（LINE用のPCへの書き込みも動作確認済）
  // delay(150); // 約6fps相当（環境で調整）
  // delay(80);  // → 約10〜12fps
  // delay(30);  // → もっと上（ただし転送が追いつかないと破綻）
}





