// MQTTの受信のシンプルな確認
// https://github.com/TomonobuHayakawa/Spresense-Playground/tree/master/sketches/MQTT/viaLTE/simple_sub
// 早川さんが用意しているものを使用
// 日本語コメントは中村

#include <LTE.h>
#include <ArduinoMqttClient.h>

// APN name
// #define APP_LTE_APN "iijmio.jp" // replace your APN

// /* APN authentication settings
//  * Ignore these parameters when setting LTE_NET_AUTHTYPE_NONE.
//  */
// #define APP_LTE_USER_NAME "mio@iij"     // replace with your username
// #define APP_LTE_PASSWORD  "iij" // replace with your password

// APN設定はMEEQシムを前提にしている
#define APP_LTE_APN "meeq.io" 

#define APP_LTE_USER_NAME "meeq"
#define APP_LTE_PASSWORD  "meeq"


// APN IP type
#define APP_LTE_IP_TYPE (LTE_NET_IPTYPE_V4V6) // IP : IPv4v6
// #define APP_LTE_IP_TYPE (LTE_NET_IPTYPE_V4) // IP : IPv4
// #define APP_LTE_IP_TYPE (LTE_NET_IPTYPE_V6) // IP : IPv6

// APN authentication type
#define APP_LTE_AUTH_TYPE (LTE_NET_AUTHTYPE_CHAP) // Authentication : CHAP
// #define APP_LTE_AUTH_TYPE (LTE_NET_AUTHTYPE_PAP) // Authentication : PAP
// #define APP_LTE_AUTH_TYPE (LTE_NET_AUTHTYPE_NONE) // Authentication : NONE

/* RAT to use
 * Refer to the cellular carriers information
 * to find out which RAT your SIM supports.
 * The RAT set on the modem can be checked with LTEModemVerification::getRAT().
 */

#define APP_LTE_RAT (LTE_NET_RAT_CATM) // RAT : LTE-M (LTE Cat-M1)
// #define APP_LTE_RAT (LTE_NET_RAT_NBIOT) // RAT : NB-IoT

// MQTT broker
#define BROKER_NAME "your broker ip"        // ブローカーサーバーのipアドレス
#define BROKER_PORT 1883                    // port 8883 is the default for MQTT over TLS.
                                            // for this client, if required by the server.

// MQTT topic
#define MQTT_TOPIC "your_topic_sub"             // topicを指定します

LTE lteAccess;
LTEClient client;
MqttClient mqttClient(client);

// 変数宣言
char broker[] = BROKER_NAME;    // MQTT Brokerアドレス
int port = BROKER_PORT;         // MQTTポート
char topic[]  = MQTT_TOPIC;     // MQTTトピック

// LTEネットワークへの接続
void doAttach()
{
  while (true) {

    /* Power on the modem and Enable the radio function. */
    // LTEネットワークへの接続に失敗したらここで止まる
    if (lteAccess.begin() != LTE_SEARCHING) {
      Serial.println("Could not transition to LTE_SEARCHING.");
      Serial.println("Please check the status of the LTE board.");
      for (;;) {
        sleep(1);
      }
    }

    /* The connection process to the APN will start.
     * If the synchronous parameter is false,
     * the return value will be returned when the connection process is started.
     */
    if (lteAccess.attach(APP_LTE_RAT,
                         APP_LTE_APN,
                         APP_LTE_USER_NAME,
                         APP_LTE_PASSWORD,
                         APP_LTE_AUTH_TYPE,
                         APP_LTE_IP_TYPE,
                         false) == LTE_CONNECTING) {
      Serial.println("Attempting to connect to network.");
      break;
    }

    /* If the following logs occur frequently, one of the following might be a cause:
     * - APN settings are incorrect
     * - SIM is not inserted correctly
     * - If you have specified LTE_NET_RAT_NBIOT for APP_LTE_RAT,
     *   your LTE board may not support it.
     */
    Serial.println("An error has occurred. Shutdown and retry the network attach preparation process after 1 second.");
    lteAccess.shutdown();
    sleep(1);
  }
}

void setup()
{
  // Open serial communications and wait for port to open
  Serial.begin(115200);
#if 0   // USBシリアル接続待ち。PCとの接続限定
  while (!Serial) {
      ; // wait for serial port to connect. Needed for native USB port only
  }
#endif
  delay(1000);    // この時間が妥当か検証していない
  Serial.println("Starting .");

  /* Connect LTE network */
  doAttach();

  int result;

  // Wait for the modem to connect to the LTE network.
  Serial.println("Waiting for successful attach.");
  LTEModemStatus modemStatus = lteAccess.getStatus();
  // LTEモデムのLTEネットワークへの接続状態の監視
  while(LTE_READY != modemStatus) {
    if (LTE_ERROR == modemStatus) {

      /* If the following logs occur frequently, one of the following might be a cause:
       * - Reject from LTE network
       */
      Serial.println("An error has occurred. Shutdown and retry the network attach process after 1 second.");
      lteAccess.shutdown();
      sleep(1);
      doAttach();
    }
    sleep(1);
    modemStatus = lteAccess.getStatus();
  }

  Serial.println("attach succeeded.");

  // MQTT Brokerサーバーへの接続（確認用にシリアルコンソールに表示）
  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(broker);

  // Brokerサーバーへの接続挑戦（失敗したらここで止まる）
  if (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());
    // do nothing forevermore:
    for (;;)
      sleep(1);
  }

  Serial.println("You're connected to the MQTT broker!");
  Serial.println();

  Serial.print("Subscribing to topic: ");
  Serial.println(topic);
  Serial.println();
  
  mqttClient.subscribe(topic);
}

// メインループ
void loop()
{
  // Brokerサーバーからメッセージを受信していないか確認
  // 受信していればメッセージサイズ(byte数)が返る
  int messageSize = mqttClient.parseMessage();

  // メッセージの受信
  if (messageSize) {
    // we received a message, print out the topic and contents
    Serial.print("Received a message with topic '");
    Serial.print(mqttClient.messageTopic());
    Serial.print("', length ");
    Serial.print(messageSize);
    Serial.println(" bytes:");

    // use the Stream interface to print the contents
    // 受信メッセージを一文字ずつ読みだして表示
    while (mqttClient.available()) {
      Serial.print((char)mqttClient.read());
    }
    Serial.println();

    Serial.println();
  }

  // 少しの時間待機
  usleep(10*1000);

}