# Spresense × LINE × Supabase ハンズオン教材

## 概要

このハンズオンでは、LINEからSpresenseのカメラを遠隔操作し、撮影した写真をLINEで受け取るシステムを構築します。

## システム構成

```
LINE <-> Supabase <-> MQTT Broker <-> Spresense
```

### 使用技術
- **LINE Messaging API**: リッチメニューとメッセージ送信
- **Supabase**: Edge Functions、Storage
- **MQTT Broker**: メッセージング
- **Spresense**: カメラ撮影

## 処理フロー

```
1. LINEのリッチメニューでボタンをタップ
   ↓
2. Supabase Edge Functionが起動（LINE認証）
   ↓
3. MQTT BrokerにPublish
   ↓
4. Spresenseが受信してカメラ撮影
   ↓
5. 撮影画像をSupabaseにHTTPでアップロード
   ↓
6. Supabase Storageに保存
   ↓
7. LINEに画像をプッシュ通知
```

## 実装するEdge Function

### 1. LINE認証Edge Function (`line-webhook`)

**役割**: LINEからのWebhookを受け取り、認証を行う

**エンドポイント**: `/functions/v1/line-webhook`

**処理内容**:
- LINEからのリクエストを検証
- Webhook署名の検証
- MQTT Publish Edge Functionを呼び出し

### 2. MQTT Publish Edge Function (`mqtt-publish`)

**役割**: MQTT Brokerにメッセージを送信

**エンドポイント**: `/functions/v1/mqtt-publish`

**処理内容**:
- MQTT Brokerに接続
- 指定したトピックにメッセージをPublish
- Spresenseにカメラ撮影指示を送信

### 3. 画像保存・LINE通知Edge Function (`image-upload`)

**役割**: Spresenseから画像を受け取り、LINEに送信

**エンドポイント**: `/functions/v1/image-upload`

**処理内容**:
- SpresenseからHTTPで画像を受信
- Supabase Storageに画像を保存
- 画像の公開URLを取得
- LINE Messaging APIで画像を送信

## セットアップ手順

### 1. 前提条件

- Supabaseプロジェクトの作成
- LINE Messaging APIチャネルの作成
- MQTT Brokerの準備
- Spresenseの準備

### 2. 環境変数の設定

`.env`ファイルを作成し、以下の情報を設定します：

```bash
# LINE
LINE_CHANNEL_ACCESS_TOKEN=your_channel_access_token
LINE_CHANNEL_SECRET=your_channel_secret

# MQTT Broker
MQTT_BROKER_URL=your-broker-host
MQTT_TOPIC=spresense/mqtt_sub

# Supabase
SUPABASE_URL=your_supabase_url
SUPABASE_ANON_KEY=your_anon_key
```

### 3. Supabaseプロジェクトの初期化とリンク

```bash
# Supabaseプロジェクトを初期化
supabase init

# Supabase CLIでログイン（アクセストークンが必要）
# https://supabase.com/dashboard/account/tokens でトークンを取得
supabase login --token your-access-token

# プロジェクトにリンク
# your-project-refはSupabaseのProject Settings > Generalから確認
supabase link --project-ref your-project-ref
```

**補足**: `supabase init`を実行すると、`supabase/config.toml`が生成されます。このファイルはプロジェクトの設定を管理します。

### 4. Supabase Storageのバケット作成

Supabase Dashboardで画像保存用のバケットを作成します：

1. Storage > Create a new bucketをクリック
2. バケット名: `spresense-images`
3. Public bucket: チェックを入れる（画像をLINEで表示するため）
4. Createをクリック

### 5. キャプチャリクエスト管理テーブルの作成

LINE user\_idをMQTTブローカーに直接流さないよう、リクエストトークンでユーザーを管理するテーブルを作成します。

Supabase DashboardのSQL Editorでテーブルを作成します：

1. Supabase Dashboardを開く
2. 左メニューから **SQL Editor** をクリック
3. 以下のSQLを貼り付けて **Run** をクリック

```sql
create table capture_requests (
  request_token uuid primary key default gen_random_uuid(),
  line_user_id text not null,
  status text not null default 'pending',
  error_message text,
  used_at timestamptz,
  created_at timestamptz not null default now()
);

create index idx_capture_requests_token on capture_requests(request_token);
```

SQLファイルは `migrations/create_capture_requests.sql` にも保存してあります。

| カラム | 説明 |
|---|---|
| `request_token` | UUID。MQTTに送信するトークン（user\_idの代わり） |
| `line_user_id` | LINEのユーザーID。サーバー側でのみ参照 |
| `status` | 処理状態（`pending` → `sent` または `failed`） |
| `error_message` | 失敗時のエラーメッセージ |
| `used_at` | 画像アップロード・LINE送信完了時に記録 |
| `created_at` | リクエスト作成日時 |

**確認方法**: Supabase DashboardのTable Editorで`capture_requests`テーブルが作成されていることを確認します。

### 6. Edge Functionsへの環境変数設定

```bash
# 環境変数をEdge Functionsにデプロイ
supabase secrets set LINE_CHANNEL_ACCESS_TOKEN=your_token
supabase secrets set LINE_CHANNEL_SECRET=your_secret
supabase secrets set MQTT_BROKER_URL=your_broker_host
supabase secrets set MQTT_TOPIC=spresense/mqtt_sub
supabase secrets set SUPABASE_URL=your_supabase_url
supabase secrets set SUPABASE_ANON_KEY=your_anon_key
supabase secrets set SUPABASE_SERVICE_ROLE_KEY=your_service_role_key
```

### 7. Edge Functionsのデプロイ

```bash
# Edge Functionsをデプロイ
# line-webhookとimage-uploadは外部（LINE/Spresense）から直接呼ばれるため、JWT検証を無効化する
supabase functions deploy line-webhook --no-verify-jwt
supabase functions deploy mqtt-publish --no-verify-jwt
supabase functions deploy image-upload --no-verify-jwt
```

### 8. LINE Webhook URLの設定

LINE Developers Consoleで、Webhook URLを設定します：

```
https://your-project.supabase.co/functions/v1/line-webhook
```

**手順**:
1. LINE Developers Consoleにログイン
2. 作成したチャネルを選択
3. Messaging API設定タブを開く
4. Webhook URLに上記URLを入力
5. Webhook利用をオンにする
6. 検証ボタンで接続テスト

## テスト手順

### 1. mqtt-publishの動作確認

mosquitto_subでMQTTメッセージの受信を確認します。

```bash
# ターミナルでsubscribeを開始（待ち受け状態になる）
mosquitto_sub -h your-broker-host -p 1883 -t "spresense/mqtt_sub"
```

subscribe状態のまま、LINEのリッチメニューからボタンをタップします。
以下のようなメッセージが表示されれば成功です。

```json
{"requestToken":"550e8400-e29b-41d4-a716-446655440000","command":"capture","timestamp":"2026-04-11T06:00:00.000Z"}
```

MQTTメッセージにはLINEのユーザーIDではなく、`requestToken`（UUID）が含まれます。

### 2. image-uploadの動作確認（Spresenseなしで確認）

PCからcurlコマンドでSpresenseの動作をシミュレートし、画像アップロード〜LINE通知の流れを確認します。

```bash
# testdataディレクトリにテスト用画像（neko.jpg）を配置してから実行
# YOUR_REQUEST_TOKENはMQTTメッセージのrequestTokenに置き換え
curl -X POST https://your-project.supabase.co/functions/v1/image-upload -F "image=@testdata/neko.jpg" -F "requestToken=YOUR_REQUEST_TOKEN"
```

**requestTokenの確認方法**: 手順1のmosquitto_subで受信したMQTTメッセージから取得できます。

成功すると、Supabase Storageに画像が保存され、LINEに画像がプッシュ通知されます。

### 2. LINE Botの友だち追加

QRコードまたはLINE IDでBotを友だち追加します。

### 3. リッチメニューのボタンをタップ

撮影開始ボタンをタップします。

### 4. 結果の確認

数秒後、LINEに撮影された画像が送信されます。

## トラブルシューティング

### Edge Functionのログ確認

```bash
supabase functions logs line-webhook
supabase functions logs mqtt-publish
supabase functions logs image-upload
```

### よくある問題

1. **LINE認証エラー**: Channel SecretとAccess Tokenを確認
2. **MQTT接続エラー**: Broker URLとトピック名を確認
3. **画像アップロードエラー**: Storageのバケット設定を確認

## 参考資料

- [Supabase Edge Functions ドキュメント](https://supabase.com/docs/guides/functions)
- [LINE Messaging API リファレンス](https://developers.line.biz/ja/reference/messaging-api/)
- [MQTT プロトコル仕様](https://mqtt.org/)
