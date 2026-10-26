# VRSS Tuner

[English](README.md) · [日本語セットアップガイド](docs/SETUP.ja.md) · [English setup guide](docs/SETUP.md)

VRSS Tunerは、NVIDIA VRSS2の設定と、左右の視線データを共通VRSSプロバイダーへ送るためのWindows用試験ツールです。BaballoniaのOSC視線データを受け取り、OpenVRのHMD投影情報を使って左右別に位置を計算します。VRChatのファイル変更やゲームへのDLL注入は行いません。

**確認環境：Pimax Dream Air（開発者・610.88）、Steam Frame（貢献者の整理前ビルド・596.02）。** その他のHMDやPCは未確認です。NVIDIA、Pimax、Valve、VRChatの公式製品ではありません。

ポータブル版にはTuner、共通視線プロバイダー、OpenVR投影情報取得ツール、日英ガイド、読み取り専用の導入診断を同梱しています。監査済みのNVIDIAドライバー（確認環境は610.88）、VRSSを許可するVRChatプロファイル、SteamVR、視線入力、VRChat内のMSAAが必要です。共通プロバイダーは非公開のドライバーインターフェースを使うため、未対応ドライバーでの起動を拒否します。診断合格だけではHMD内の視線追従を証明できません。

## Steam Frame（596.02・実験対応）

Frameでは旧来のEDID診断やStart-Providerを使わず、[Frame専用手順](docs/STEAM-FRAME.md)を使ってください。SteamVRの視線共有を有効にし、VRレイアウト4・MSAAを設定します。VRChatとUnityを閉じてTunerの「Steam Frame」→「接続 / 再試行」を押し、登録後に「VRChatを起動（Frame用）」を使います。上下変換は内蔵済みなので共通補正の初期値はX=1・Y=1です。元のPimax/Baballonia経路は維持しています。

## 使い始める

1. [Releases](https://github.com/challenger0303/VRSS-Tuner/releases)からZIPを取得し、書き込み可能なフォルダーへ展開します。
2. HMDを接続し、SteamVRを起動してVRChatを閉じた状態で`Test-Setup.cmd`を実行します。
3. [日本語セットアップガイド](docs/SETUP.ja.md)に沿ってVRSSと視線プロバイダーを起動し、VRChat内のMSAAを有効にします。

ドライバー非対応やVRSS許可設定の不足が表示された場合、強制起動しないでください。診断結果、ドライバー版、HMD機種を添えてIssueへ報告できます。個人情報は投稿前に削除してください。

## ソースからのビルド

Visual Studio 2022の「C++によるデスクトップ開発」を入れてください。PowerShellから`vrss-tuner\test.ps1`で独立テスト、`vrss-tuner\Build-Portable.ps1`で配布ZIPを作れます。ビルドツールは`vswhere`で検出し、必要なら`VCVARS64_BAT`で指定できます。NVAPIとOpenVRのビルド用ファイルは各ライセンス表示とともに`third_party`にあります。

本プロジェクトのコードはMITライセンスです。[LICENSE](LICENSE)と[第三者ライセンス](THIRD_PARTY_NOTICES.md)を参照してください。NVIDIAとValveのコンポーネントはそれぞれのライセンスに従います。
