#!/bin/bash

# Supabase CLIのインストール（GitHubリリースからバイナリを取得）
curl -fsSL https://github.com/supabase/cli/releases/latest/download/supabase_linux_amd64.tar.gz -o /tmp/supabase.tar.gz
tar -xzf /tmp/supabase.tar.gz -C /tmp
sudo mv /tmp/supabase /usr/local/bin/supabase
rm /tmp/supabase.tar.gz

# MQTTクライアント（mosquitto-clients）のインストール
sudo apt-get update && sudo apt-get install -y mosquitto-clients
