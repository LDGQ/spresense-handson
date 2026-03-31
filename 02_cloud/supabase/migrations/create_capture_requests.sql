-- キャプチャリクエスト管理テーブル
-- LINE user_idをMQTTに流さず、トークンで管理するためのテーブル
create table capture_requests (
  request_token uuid primary key default gen_random_uuid(),
  line_user_id text not null,
  status text not null default 'pending',
  image_path text,
  error_message text,
  used_at timestamptz,
  created_at timestamptz not null default now()
);

-- トークン検索用インデックス
create index idx_capture_requests_token on capture_requests(request_token);
