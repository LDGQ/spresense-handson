// ------------------------------------------------------------
// SPRESENSE GNSS（位置情報） + シリアルコマンド応答（LINE連携の前段）
// ------------------------------------------------------------
// 目的：
//  ・SPRESENSE内蔵GNSSで位置情報（緯度・経度）を取得できることを確認する
//  ・PC等（将来はLINE連携側のブリッジ）からシリアル経由でコマンドを送り、
//    SPRESENSEが応答できることを確認する（簡易プロトコル）
//
// 位置づけ：
//  ・最終構成「SPRESENSE →（LTE等）→ LINE」を想定した “SPRESENSE側の機能確認” スケッチです。
//  ・本スケッチ単体ではLINE通信は行いません（LINE側は別実装）。
//
// 動作概要：
//  1) GNSSを初期化して測位開始（HOT_START）
//  2) GNSS更新に応じて、位置ログをシリアルへ継続出力（デバッグ）
//  3) シリアルからの1文字コマンドに応じて、LED操作や現在地応答を返す
//
// LEDチカ：
//  ・LED_PIN : ユーザーLED用GPIO（環境に合わせて変更）
//
// シリアルコマンド仕様（1文字 / 115200bps）
//  ・'0' : LED_PIN をON（応答：0/1）
//  ・'x' : LED_PIN をOFF（応答：0/1）
//  ・'1' : 現在地を応答（@RESP,GNSS,numSat,lat,lon）
//          ※Fixしていない/位置がない場合は "No Position" を返す
//
// LED表示（SPRESENSE基板のLED）
//  ・LED0 : 生存確認（loop内で点滅）
//  ・LED1 : 測位Fix状態（Fix=点灯 / No-Fix=消灯）
//  ・LED3 : エラー（点灯）
//  ・LED_PIN(0) : ユーザーLED（コマンド0/xでON/OFF）
//
// 注意：
//  ・GNSSは屋内だとFixしないことがあります（窓際/屋外推奨）。
//  ・時刻/衛星ログが大量に出ます（ログの見やすさは後で調整可能）。
//  ・ShowCurrentLocation() 内で未初期化のStringBufferを出力している可能性があります。
//    使用する場合は確認しておく必要があります
// ------------------------------------------------------------


/*
 *  gnss.ino - GNSS example application
 *  Copyright 2018 Sony Semiconductor Solutions Corporation
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * @file gnss.ino
 * @author Sony Semiconductor Solutions Corporation
 * @brief GNSS example application
 * @details Spresense has an built in GNSS receiver which supports GPS and other
 *          GNSS satellites. This skecth provides an example the GNSS operation.
 *          Simply upload the sketch, reset the board and check the USB serial
 *          output. After 3 seconds status information should start to appear.\n\n
 *
 *          This example code is in the public domain.
 */

// Lチカ用のGPIO LED
#define LED_PIN 0
// LEDの状態（0:OFF / 1:ON）
volatile bool isLEDstatus = 0;


// 関数プロトタイプ
void LedSet();
void TurnOnLed();
void TurnOffLed();
void HandleSerialCommand(char cmd);

/* include the GNSS library */
#include <GNSS.h>

#define STRING_BUFFER_SIZE  128       /**< %Buffer size */

#define RESTART_CYCLE       (60 * 5)  /**< positioning test term */

static SpGnss Gnss;                   /**< SpGnss object */

// 衛星データ用の構造体
SpNavData g_NavData;

/**
 * @enum ParamSat
 * @brief Satellite system
 */
// 使用する衛星システムの選択
enum ParamSat {
  eSatGps,            /**< GPS                     World wide coverage  */
  eSatGlonass,        /**< GLONASS                 World wide coverage  */
  eSatGpsSbas,        /**< GPS+SBAS                North America        */
  eSatGpsGlonass,     /**< GPS+Glonass             World wide coverage  */
  eSatGpsBeidou,      /**< GPS+BeiDou              World wide coverage  */
  eSatGpsGalileo,     /**< GPS+Galileo             World wide coverage  */
  eSatGpsQz1c,        /**< GPS+QZSS_L1CA           East Asia & Oceania  */
  eSatGpsGlonassQz1c, /**< GPS+Glonass+QZSS_L1CA   East Asia & Oceania  */
  eSatGpsBeidouQz1c,  /**< GPS+BeiDou+QZSS_L1CA    East Asia & Oceania  */
  eSatGpsGalileoQz1c, /**< GPS+Galileo+QZSS_L1CA   East Asia & Oceania  */
  eSatGpsQz1cQz1S,    /**< GPS+QZSS_L1CA+QZSS_L1S  Japan                */
};

/* Set this parameter depending on your current region. */
// 地域に合わせて変更
// 日本想定：GPS + QZSS(L1CA/L1S)
// static enum ParamSat satType =  eSatGps;
static enum ParamSat satType = eSatGpsQz1cQz1S;


/**
 * @brief Turn on / off the LED0 for CPU active notification.
 */
static void Led_isActive(void)
{
  static int state = 1;
  if (state == 1)
  {
    ledOn(PIN_LED0);
    state = 0;
  }
  else
  {
    ledOff(PIN_LED0);
    state = 1;
  }
}

/**
 * @brief Turn on / off the LED1 for positioning state notification.
 *
 * @param [in] state Positioning state
 */
//  即位fix状態の表示（LED1）fix：点灯、 未fix：消灯
static void Led_isPosfix(bool state)
{
  if (state)
  {
    ledOn(PIN_LED1);
  }
  else
  {
    ledOff(PIN_LED1);
  }
}

/**
 * @brief Turn on / off the LED3 for error notification.
 *
 * @param [in] state Error state
 */
//  エラー表示（エラーの時はLED3が点灯）
static void Led_isError(bool state)
{
  if (state)
  {
    ledOn(PIN_LED3);
  }
  else
  {
    ledOff(PIN_LED3);
  }
}

void allLedOn(){

  ledOn(PIN_LED0);
  ledOn(PIN_LED1);
  ledOn(PIN_LED2);
  ledOn(PIN_LED3);
}

void allLedOff(){

  ledOff(PIN_LED0);
  ledOff(PIN_LED1);
  ledOff(PIN_LED2);
  ledOff(PIN_LED3);
}
/**
 * @brief Activate GNSS device and start positioning.
 */
void setup() {
  /* put your setup code here, to run once: */

  // エラーフラグの初期化
  int error_flag = 0;

  /* Set serial baudrate. */
  Serial.begin(115200);

  /* Wait HW initialization done. */
  sleep(3);

  allLedOn();

  /* Set Debug mode to Info */
  // デバッグ表示のレベルを選択
  Gnss.setDebugMode(PrintInfo);

  int result;

  /* Activate GNSS device */
  result = Gnss.begin();

  if (result != 0)
  {
    Serial.println("Gnss begin error!!");
    error_flag = 1;
  }
  else
  {
    /* Setup GNSS
     *  It is possible to setup up to two GNSS satellites systems.
     *  Depending on your location you can improve your accuracy by selecting different GNSS system than the GPS system.
     *  See: https://developer.sony.com/develop/spresense/developer-tools/get-started-using-nuttx/nuttx-developer-guide#_gnss
     *  for detailed information.
    */
    // 衛星システムの選択  
    switch (satType)
    {
    case eSatGps:
      Gnss.select(GPS);
      break;

    case eSatGpsSbas:
      Gnss.select(GPS);
      Gnss.select(SBAS);
      break;

    case eSatGlonass:
      Gnss.select(GLONASS);
      Gnss.deselect(GPS);
      break;

    case eSatGpsGlonass:
      Gnss.select(GPS);
      Gnss.select(GLONASS);
      break;

    case eSatGpsBeidou:
      Gnss.select(GPS);
      Gnss.select(BEIDOU);
      break;

    case eSatGpsGalileo:
      Gnss.select(GPS);
      Gnss.select(GALILEO);
      break;

    case eSatGpsQz1c:
      Gnss.select(GPS);
      Gnss.select(QZ_L1CA);
      break;

    case eSatGpsQz1cQz1S:
      Gnss.select(GPS);
      Gnss.select(QZ_L1CA);
      Gnss.select(QZ_L1S);
      break;

    case eSatGpsBeidouQz1c:
      Gnss.select(GPS);
      Gnss.select(BEIDOU);
      Gnss.select(QZ_L1CA);
      break;

    case eSatGpsGalileoQz1c:
      Gnss.select(GPS);
      Gnss.select(GALILEO);
      Gnss.select(QZ_L1CA);
      break;

    case eSatGpsGlonassQz1c:
    default:
      Gnss.select(GPS);
      Gnss.select(GLONASS);
      Gnss.select(QZ_L1CA);
      break;
    }

    /* Start positioning */
    // 即位開始
    // HOt_STARTに変更した
    result = Gnss.start(HOT_START);
    if (result != 0)
    {
      Serial.println("Gnss start error!!");
      error_flag = 1;
    }
    else
    {
      Serial.println("Gnss setup OK");
    }
  }

  /* Start 1PSS output to PIN_D02 */
  //Gnss.start1PPS();

  // Turn off all LED:Setup done.
  allLedOff();

  // エラーがあればLEDを点灯して終了
  if (error_flag == 1)
  {
    Led_isError(true);
    exit(0);
  }
}

/**
 * @brief %Print position information.
 */
// 位置情報の表示
static void print_pos(SpNavData *pNavData)
{
  char StringBuffer[STRING_BUFFER_SIZE];

  /* print time */
  snprintf(StringBuffer, STRING_BUFFER_SIZE, "%04d/%02d/%02d ", pNavData->time.year, pNavData->time.month, pNavData->time.day);
  Serial.print(StringBuffer);

  snprintf(StringBuffer, STRING_BUFFER_SIZE, "%02d:%02d:%02d.%06ld, ", pNavData->time.hour, pNavData->time.minute, pNavData->time.sec, pNavData->time.usec);
  Serial.print(StringBuffer);

  /* print satellites count */
  snprintf(StringBuffer, STRING_BUFFER_SIZE, "numSat:%2d, ", pNavData->numSatellites);
  Serial.print(StringBuffer);

  /* print position data */
  if (pNavData->posFixMode == FixInvalid)
  {
    Serial.print("No-Fix, ");
  }
  else
  {
    Serial.print("Fix, ");
  }
  if (pNavData->posDataExist == 0)
  {
    Serial.print("No Position");
  }
  else
  {
    Serial.print("Lat=");
    Serial.print(pNavData->latitude, 6);
    Serial.print(", Lon=");
    Serial.print(pNavData->longitude, 6);
  }

  Serial.println("");
}

/**
 * @brief %Print satellite condition.
 */
static void print_condition(SpNavData *pNavData)
{
  char StringBuffer[STRING_BUFFER_SIZE];
  unsigned long cnt;

  /* Print satellite count. */
  snprintf(StringBuffer, STRING_BUFFER_SIZE, "numSatellites:%2d\n", pNavData->numSatellites);
  Serial.print(StringBuffer);

  for (cnt = 0; cnt < pNavData->numSatellites; cnt++)
  {
    const char *pType = "---";
    SpSatelliteType sattype = pNavData->getSatelliteType(cnt);

    /* Get satellite type. */
    /* Keep it to three letters. */
    switch (sattype)
    {
      case GPS:
        pType = "GPS";
        break;

      case GLONASS:
        pType = "GLN";
        break;

      case QZ_L1CA:
        pType = "QCA";
        break;

      case SBAS:
        pType = "SBA";
        break;

      case QZ_L1S:
        pType = "Q1S";
        break;

      case BEIDOU:
        pType = "BDS";
        break;

      case GALILEO:
        pType = "GAL";
        break;

      default:
        pType = "UKN";
        break;
    }

    /* Get print conditions. */
    unsigned long Id  = pNavData->getSatelliteId(cnt);
    unsigned long Elv = pNavData->getSatelliteElevation(cnt);
    unsigned long Azm = pNavData->getSatelliteAzimuth(cnt);
    float sigLevel = pNavData->getSatelliteSignalLevel(cnt);

    /* Print satellite condition. */
    snprintf(StringBuffer, STRING_BUFFER_SIZE, "[%2ld] Type:%s, Id:%2ld, Elv:%2ld, Azm:%3ld, CN0:", cnt, pType, Id, Elv, Azm );
    Serial.print(StringBuffer);
    Serial.println(sigLevel, 6);
  }
}


// 問い合わせに対する位置情報の表示
// 中村作成 202602-05
static void ShowCurrentLocation(SpNavData *pNavData)
{
  char StringBuffer[STRING_BUFFER_SIZE];

  // @RESP,GNSS,7,lat,lon CSVライクに送る
  Serial.print("@RESP,GNSS,");
  // snprintf(StringBuffer, STRING_BUFFER_SIZE, "%2d, ", pNavData->numSatellites);
  Serial.print(pNavData->numSatellites);
  Serial.print(",");
  // ここについては検証が必要
  // snprintf(StringBuffer, STRING_BUFFER_SIZE, "numSat:%2d, ", pNavData->numSatellites);
  // Serial.print(StringBuffer);

  
  if (pNavData->posDataExist == 0)
  {
    Serial.print("No Position");
  }
  else
  {
    // Serial.print("Lat=");
    Serial.print(pNavData->latitude, 6);
    // Serial.print(", Lon=");
    Serial.print(", ");
    Serial.print(pNavData->longitude, 6);
  }

  Serial.println("");
}

// シリアルからの指示への応答
void HandleSerialCommand(char cmd) {
  switch (cmd) {
    case '0': 
      TurnOnLed(); 
      // オンになったことを伝える
      Serial.println(isLEDstatus);
      Serial.println("");
      break;

    case '1': 
      // TurnOnLed();
      // GPSのデータを返す
      ShowCurrentLocation(&g_NavData);
      break;

    case 'x': 
      TurnOffLed(); 
      // オフになったことを伝える
      // Serial.print("@RESP,LED,");
      Serial.println(isLEDstatus);
      Serial.println("");
      break;
    
    // case '2': TurnOnLed(2); break;
    // case '3': TurnOnLed(3); break;

    case 's': // all on
      // 未実装

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

/**
 * @brief %Print position information and satellite condition.
 *
 * @details When the loop count reaches the RESTART_CYCLE value, GNSS device is
 *          restarted.
 */
void loop()
{
  /* put your main code here, to run repeatedly: */

  static int LoopCount = 0;       // 再起動確認用カウンタ
  static int LastPrintMin = 0;    // 最後に衛星ログを出した時間

  /* Blink LED. */
  // CPUの生存確認用のLEDの点滅
  Led_isActive();

  // シリアルからの入力に応じてSPRESENSEを操作する
  if (Serial.available() > 0) {
    
    char cmd = Serial.read();
    HandleSerialCommand(cmd);
  }

  /* Check update. */
  // GNSSの更新待ち
  if (Gnss.waitUpdate(-1))
  {
    /* Get NaviData. */
    // SpNavData g_NavData;
    Gnss.getNavData(&g_NavData);

    /* Set posfix LED. */
    bool LedSet = (g_NavData.posDataExist && (g_NavData.posFixMode != FixInvalid));
    Led_isPosfix(LedSet);

    /* Print satellite information every minute. */
    if (g_NavData.time.minute != LastPrintMin)
    {
      print_condition(&g_NavData);
      LastPrintMin = g_NavData.time.minute;
    }

    /* Print position information. */
    print_pos(&g_NavData);
    // Serial.println("");
    // Serial.println("");
   
  }
  else
  {
    /* Not update. */
    Serial.println("data not update");
  }

  /* Check loop count. */
  LoopCount++;
  if (LoopCount >= RESTART_CYCLE)
  {
    int error_flag = 0;

    /* Turn off LED0 */
    ledOff(PIN_LED0);

    /* Set posfix LED. */
    Led_isPosfix(false);

    /* Restart GNSS. */
    if (Gnss.stop() != 0)
    {
      Serial.println("Gnss stop error!!");
      error_flag = 1;
    }
    else if (Gnss.end() != 0)
    {
      Serial.println("Gnss end error!!");
      error_flag = 1;
    }
    else
    {
      Serial.println("Gnss stop OK.");
    }

    if (Gnss.begin() != 0)
    {
      Serial.println("Gnss begin error!!");
      error_flag = 1;
    }
    else if (Gnss.start(HOT_START) != 0)
    {
      Serial.println("Gnss start error!!");
      error_flag = 1;
    }
    else
    {
      Serial.println("Gnss restart OK.");
    }

    LoopCount = 0;

    /* Set error LED. */
    if (error_flag == 1)
    {
      Led_isError(true);
      exit(0);
    }
  }
}


void LedSet(){

  pinMode(LED_PIN, OUTPUT);
}

void TurnOffLed() {

  digitalWrite(LED_PIN, LOW);
  isLEDstatus = 0;
}



void TurnOnLed() {
  
  digitalWrite(LED_PIN, HIGH);
  isLEDstatus = 1;
}
