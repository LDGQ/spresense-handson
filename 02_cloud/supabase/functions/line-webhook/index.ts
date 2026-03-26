import { serve } from "https://deno.land/std@0.168.0/http/server.ts";
import { createHmac } from "https://deno.land/std@0.168.0/node/crypto.ts";

// LINE Messaging APIの型定義
interface LineWebhookEvent {
  type: string;
  message?: {
    type: string;
    text?: string;
  };
  postback?: {
    data: string;
  };
  replyToken: string;
  source: {
    userId: string;
  };
}

interface LineWebhookBody {
  events: LineWebhookEvent[];
  destination: string;
}

serve(async (req) => {
  try {
    // LINE署名の検証
    const signature = req.headers.get("x-line-signature");
    const channelSecret = Deno.env.get("LINE_CHANNEL_SECRET");

    if (!signature || !channelSecret) {
      return new Response(
        JSON.stringify({ error: "Missing signature or channel secret" }),
        { status: 401, headers: { "Content-Type": "application/json" } }
      );
    }

    // リクエストボディを取得
    const body = await req.text();

    // 署名を検証
    const hash = createHmac("sha256", channelSecret)
      .update(body)
      .digest("base64");

    if (hash !== signature) {
      return new Response(
        JSON.stringify({ error: "Invalid signature" }),
        { status: 401, headers: { "Content-Type": "application/json" } }
      );
    }

    // JSONとしてパース
    const webhookBody: LineWebhookBody = JSON.parse(body);

    // イベント処理
    for (const event of webhookBody.events) {
      // メッセージイベントまたはポストバックイベントの場合
      if (event.type === "message" || event.type === "postback") {
        const userId = event.source.userId;

        // MQTT Publish Edge Functionを呼び出し
        const supabaseUrl = Deno.env.get("SUPABASE_URL");
        const supabaseAnonKey = Deno.env.get("SUPABASE_ANON_KEY");

        await fetch(`${supabaseUrl}/functions/v1/mqtt-publish`, {
          method: "POST",
          headers: {
            "Content-Type": "application/json",
            "Authorization": `Bearer ${supabaseAnonKey}`,
          },
          body: JSON.stringify({
            userId: userId,
            command: "capture",
          }),
        });
      }
    }

    // LINEには200 OKを返す
    return new Response(JSON.stringify({ success: true }), {
      status: 200,
      headers: { "Content-Type": "application/json" },
    });

  } catch (error) {
    console.error("Error:", error);
    return new Response(
      JSON.stringify({ error: error.message }),
      { status: 500, headers: { "Content-Type": "application/json" } }
    );
  }
});
