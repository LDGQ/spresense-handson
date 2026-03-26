import { serve } from "https://deno.land/std@0.168.0/http/server.ts";
import { createClient } from "https://esm.sh/@supabase/supabase-js@2.38.4";

serve(async (req) => {
  try {
    // マルチパートフォームデータを解析
    const formData = await req.formData();
    const imageFile = formData.get("image") as File;
    const userId = formData.get("userId") as string;

    if (!imageFile || !userId) {
      return new Response(
        JSON.stringify({ error: "Missing image or userId" }),
        { status: 400, headers: { "Content-Type": "application/json" } }
      );
    }

    // Supabaseクライアントの初期化
    const supabaseUrl = Deno.env.get("SUPABASE_URL")!;
    const supabaseServiceKey = Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!;
    const supabase = createClient(supabaseUrl, supabaseServiceKey);

    // ファイル名を生成（タイムスタンプ付き）
    const timestamp = new Date().getTime();
    const fileName = `${userId}_${timestamp}.jpg`;

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
    console.error("Error:", error);
    return new Response(
      JSON.stringify({ error: error.message }),
      { status: 500, headers: { "Content-Type": "application/json" } }
    );
  }
});
