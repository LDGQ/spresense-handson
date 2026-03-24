import { serve } from "https://deno.land/std@0.168.0/http/server.ts";
import mqtt from "npm:mqtt@5.3.4";

interface MqttPublishRequest {
  userId: string;
  command: string;
}

serve(async (req) => {
  try {
    // リクエストボディを取得
    const requestBody: MqttPublishRequest = await req.json();
    const { userId, command } = requestBody;

    // 環境変数からMQTT Broker情報を取得
    const mqttBrokerUrl = Deno.env.get("MQTT_BROKER_URL");
    const mqttTopic = Deno.env.get("MQTT_TOPIC") || "spresense/camera";

    if (!mqttBrokerUrl) {
      return new Response(
        JSON.stringify({ error: "MQTT_BROKER_URL not configured" }),
        { status: 500, headers: { "Content-Type": "application/json" } }
      );
    }

    // メッセージのペイロードを作成
    const payload = JSON.stringify({
      userId: userId,
      command: command,
      timestamp: new Date().toISOString(),
    });

    // MQTT Brokerに接続してPublish
    await new Promise<void>((resolve, reject) => {
      const client = mqtt.connect(`mqtt://${mqttBrokerUrl}:1883`);

      // 接続タイムアウト（5秒）
      const timeout = setTimeout(() => {
        client.end(true);
        reject(new Error("MQTT connection timeout"));
      }, 5000);

      // 接続成功時にPublish
      client.on("connect", () => {
        client.publish(mqttTopic, payload, { qos: 1 }, (err: Error | undefined) => {
          clearTimeout(timeout);
          client.end();
          if (err) {
            reject(err);
          } else {
            resolve();
          }
        });
      });

      // 接続エラー
      client.on("error", (err: Error) => {
        clearTimeout(timeout);
        client.end(true);
        reject(err);
      });
    });

    console.log(`Published to ${mqttTopic}:`, payload);

    return new Response(
      JSON.stringify({
        success: true,
        topic: mqttTopic,
        payload: payload
      }),
      { status: 200, headers: { "Content-Type": "application/json" } }
    );

  } catch (error) {
    console.error("Error:", error);
    return new Response(
      JSON.stringify({ error: error.message }),
      { status: 500, headers: { "Content-Type": "application/json" } }
    );
  }
});
