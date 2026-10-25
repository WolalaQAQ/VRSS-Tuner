# 導入と確認（日本語）

[English](SETUP.md) · [プロジェクト概要](../README.ja.md)

## 確認済み構成と制限

- 開発者が実機で視覚的に確認したHMD：**Pimax Dream Air**。
- 確認環境のGPU／ドライバー：GeForce RTX 5090、NVIDIA 610.88。プロバイダーは表示上のバージョンだけでなく、`nvapi64_impl.dll`と`nvwgf2umx.dll`の監査済みハッシュとの一致を検査します。別のビルドは起動を拒否します。
- SteamVR/OpenVRのHMD投影情報、VRSSを許可するNVIDIAのVRChatプロファイル、Baballonia OSCからの左右視線、VRChat内のMSAAが必要です。
- 他のHMD、PC、ドライバービルド、OSC送信元は未検証です。導入診断に合格しても、視界内で高画質領域が追従する保証にはなりません。

## 導入手順

1. [Releases](https://github.com/challenger0303/VRSS-Tuner/releases)からポータブルZIPを取得し、全体を書き込み可能なフォルダーに展開します。`VRSS Tuner.exe`だけを取り出さず、スクリプトと`common-vrss-provider`も一緒に置いてください。
2. HMDを接続しSteamVRを起動します。VRChatは閉じておきます。Baballoniaから左右の視線OSCを`127.0.0.1:9020`に送るよう設定します。
3. `Test-Setup.cmd`を実行します。パッケージのハッシュ、NVIDIAドライバー、プロファイル、SteamVR/HMD投影情報、動作中のプロバイダーを読み取ります。ドライバー設定は変更しません。`readyForProviderLaunch: false`の場合、日英両方の理由が表示されます。
4. 不足が`vrssOn`だけなら、`VRSS Tuner.exe`の画質設定で**オン**にして保存し、Windowsの管理者確認を承認します。診断を再実行してください。`driverSupported`または`vrssAllowed`がfalseなら、この構成では共通プロバイダーを有効化できません。検査を飛ばさないでください。
5. `Start-Provider.cmd`を実行し、Windowsの管理者確認を承認します。Tunerで共通プロバイダーを選び、視線入力を開始します。プロバイダーはTunerからの左右視線をローカルUDPポート5556で受け取ります。TunerがBaballonia OSCを受ける既定のポートは9020です。
6. VRChatを起動してグラフィック設定のMSAAを有効にします。HMD内で高画質領域が両目に現れ、視線の左右・上下に追従するか確認します。プロセスのログだけでは画質の確認になりません。
7. 終了時は`Stop-Provider.cmd`を実行します。これは正常終了を要求するもので、SteamVR、VRChat、ベンダーのプロバイダーを終了しません。

## 位置補正

同梱の`common-vrss-provider/calibration.txt`は左右オフセット0、横・縦倍率1から始まります。HMDでコントラストの高い対象を見ながら、Tunerの視線補正で調整してください。視線と描画領域を比較するときは、できるだけ頭を止めてください。初期値は汎用値であり、**Dream Air専用の補正値ではありません**。

## 困ったとき

| 診断結果・症状 | 確認すること |
| --- | --- |
| `driverSupported: false` | NVIDIAの実ファイルが監査済み610.88ビルドと違います。この版は未確認の非公開インターフェース呼び出しを拒否します。 |
| `vrssAllowed: false` | NVIDIAのVRChatプロファイルにVRSS許可設定がありません。このパッケージはプロファイルの強制作成や置換を行いません。 |
| `steamVrRunning: false`／`hmdGeometryValid: false` | HMDを接続してSteamVRを起動し、再診断します。 |
| 公式プロバイダーが動作中 | その経路を使うか、ベンダーの通常操作で停止してから共通経路を試してください。スクリプトは自動停止しません。 |
| プロバイダーは動くが領域が追従しない | Baballoniaが新しい左右視線を出しているか、Tunerの配信が開始したか、VRChatのMSAAが有効か、両目の実映像を確認してください。 |

診断結果と実行ログは`%LOCALAPPDATA%\VRSS Tuner`に保存します。共有前にPCやHMDの情報が含まれていないか確認してください。NVIDIA／Pimaxドライバー本体は同梱しておらず、未検証構成での動作は保証していません。
