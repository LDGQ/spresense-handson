import { serve } from "https://deno.land/std@0.168.0/http/server.ts";
import { createClient } from "https://esm.sh/@supabase/supabase-js@2.38.4";

serve(async (req) => {
  try {
    // マルチパートフォームデータを解析
    const formData = await req.formData();
    const imageFile = formData.get("image") as File;
    const requestToken = formData.get("requestToken") as string;

    if (!imageFile || !requestToken) {
      return new Response(
        JSON.stringify({ error: "Missing image or requestToken" }),
        { status: 400, headers: { "Content-Type": "application/json" } }
      );
    }

    // Supabaseクライアントの初期化
    const supabaseUrl = Deno.env.get("SUPABASE_URL")!;
    const supabaseServiceKey = Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!;
    const supabase = createClient(supabaseUrl, supabaseServiceKey);

    // トークンからLINEユーザーIDを取得
    const { data: requestData, error: queryError } = await supabase
      .from("capture_requests")
      .select("line_user_id, used_at")
      .eq("request_token", requestToken)
      .single();

    if (queryError || !requestData) {
      return new Response(
        JSON.stringify({ error: "Invalid request token" }),
        { status: 400, headers: { "Content-Type": "application/json" } }
      );
    }

    // 使用済みトークンの再利用を防止
    if (requestData.used_at) {
      return new Response(
        JSON.stringify({ error: "Request token already used" }),
        { status: 400, headers: { "Content-Type": "application/json" } }
      );
    }

    const userId = requestData.line_user_id;

    // ファイル名を生成（タイムスタンプ付き）
    const timestamp = new Date().getTime();
    const fileName = `${requestToken}_${timestamp}.jpg`;

    // Storageに画像を保存
    const { data: uploadData, error: uploadError } = await supabase.storage
      .from("spresense-images")
      .upload(fileName, imageFile, {
        contentType: "image/jpeg",
        upsert: false,
      });

    if (uploadError) {
      throw new Error(`Upload failed: ${uploadError.message}`);
    }

    // 公開URLを取得
    const { data: urlData } = supabase.storage
      .from("spresense-images")
      .getPublicUrl(fileName);

    const imageUrl = urlData.publicUrl;

    // LINE Messaging APIで画像を送信
    const lineChannelAccessToken = Deno.env.get("LINE_CHANNEL_ACCESS_TOKEN");

    if (!lineChannelAccessToken) {
      throw new Error("LINE_CHANNEL_ACCESS_TOKEN not configured");
    }

    const lineApiUrl = "https://api.line.me/v2/bot/message/push";
    const lineMessage = {
      to: userId,
      messages: [
        {
          type: "image",
          originalContentUrl: imageUrl,
          previewImageUrl: imageUrl,
        },
      ],
    };

    const lineResponse = await fetch(lineApiUrl, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        "Authorization": `Bearer ${lineChannelAccessToken}`,
      },
      body: JSON.stringify(lineMessage),
    });

    if (!lineResponse.ok) {
      const errorText = await lineResponse.text();
      throw new Error(`LINE API error: ${errorText}`);
    }

    // LINE送信成功時にステータスを更新
    await supabase
      .from("capture_requests")
      .update({ status: "sent", used_at: new Date().toISOString() })
      .eq("request_token", requestToken);

    console.log(`Image uploaded and sent to user ${userId}: ${imageUrl}`);

    return new Response(
      JSON.stringify({
        success: true,
        imageUrl: imageUrl,
        fileName: fileName
      }),
      { status: 200, headers: { "Content-Type": "application/json" } }
    );

  } catch (error) {
    // 失敗時にステータスとエラーメッセージを記録
    const errorMsg = error instanceof Error ? error.message : String(error);
    try {
      const supabaseUrl = Deno.env.get("SUPABASE_URL")!;
      const supabaseServiceKey = Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!;
      const supabase = createClient(supabaseUrl, supabaseServiceKey);
      const formData = await req.clone().formData().catch(() => null);
      const requestToken = formData?.get("requestToken") as string | null;
      if (requestToken) {
        await supabase
          .from("capture_requests")
          .update({ status: "failed", error_message: errorMsg, used_at: new Date().toISOString() })
          .eq("request_token", requestToken);
      }
    } catch (_) {
      // ステータス更新失敗は無視
    }

    console.error("Error:", error);
    return new Response(
      JSON.stringify({ error: errorMsg }),
      { status: 500, headers: { "Content-Type": "application/json" } }
    );
  }
});
