# VRSS Tuner ポータブル版

WindowsのNVIDIA VRSS2設定と視線入力を扱う試験版です。日本語／英語はアプリ右上で切り替えられます。ZIPを任意の書き込み可能なフォルダーに展開してください。インストーラーやVisual C++ランタイムの別途導入は不要です。

**開発者の確認環境はPimax Dream Air**です（GeForce RTX 5090／NVIDIAドライバー610.88）。貢献者が整理前ビルドでSteam Frame／596.02を確認しています。Frameは下記の専用手順を使ってください。[詳細な日本語ガイド](https://github.com/challenger0303/VRSS-Tuner/blob/main/docs/SETUP.ja.md)も参照してください。

## Steam Frame（596.02・実験対応）

Frameでは旧来のEDID診断やStart-Providerを使わず、[Frame専用手順](STEAM-FRAME.md)を使ってください。SteamVRの視線共有を有効にし、VRレイアウト4・MSAAを設定します。VRChatとUnityを閉じてTunerの「Steam Frame」→「接続 / 再試行」を押し、登録後に「VRChatを起動（Frame用）」を使います。上下変換は内蔵済みなので共通補正の初期値はX=1・Y=1です。元のPimax/Baballonia経路は維持しています。

## 初回の使い方

1. NVIDIA GPU、VRChat、SteamVR、視線データを出せるソフトを用意します。Baballoniaを使う場合は、左右の視線OSCを `127.0.0.1:9020` に送る設定にします。
2. HMDを接続してSteamVRを起動し、VRChatは閉じておきます。`Test-Setup.cmd` で必要条件を確認します。診断はドライバーやVRChatの設定を変更しません。
3. VRSSがオフと出たら、`VRSS Tuner.exe` の画質タブでオンに保存します。Windowsの管理者確認が出ます。
4. `Start-Provider.cmd` を実行し、Windowsの管理者確認を承認します。Tunerの「視線入力」で「配信を開始」を押し、Baballonia OSCを受けていることを確認してからVRChatを起動します。VRChatのグラフィック設定でMSAAを有効にします。
5. HMD内で両目の高画質領域が視線に追従するか確認します。位置がずれる場合は視線補正を調整します。終了時は `Stop-Provider.cmd` を実行します。

Pimax等の公式視線プロバイダーが動作中は、この共通プロバイダーは自動で置き換えません。公式経路を使うか、ベンダーの通常操作で停止してから診断してください。

## 対応条件

共通プロバイダーは、監査済みNVIDIAドライバー画像のSHA-256が一致するときだけ起動します。別のドライバー版は拒否します。NVIDIAのVRChatプロファイルにはVRSS許可フラグも必要です。診断で未許可の場合、このパッケージは無理にプロファイルを作りません。MSAA有効化、BaballoniaのOSC設定、HMDごとの視線補正は手動です。実機での動作は各PCで確認してください。

`common-vrss-provider/calibration.txt` は共通経路専用の初期値です。配布時の値は横・縦倍率1、左右オフセット0です。Tunerから変更できます。診断結果と実行ログは `%LOCALAPPDATA%\VRSS Tuner` に保存されます。NVIDIA/Pimaxドライバー本体は同梱していません。

本プロジェクトのコードはMITライセンスです。本文は`LICENSE.txt`にあります。同梱のOpenVRとNVAPIにはそれぞれ`OpenVR-LICENSE.txt`、`NVAPI-LICENSE.txt`の条件が適用されます。NVIDIA、Pimax、Valve、VRChatの公式製品ではありません。
