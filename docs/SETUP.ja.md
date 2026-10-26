# 導入と確認（日本語）

[English](SETUP.md) · [プロジェクト概要](../README.ja.md)

この手順は標準プロバイダー経路用です。NVIDIA 596.02のSteam Frameは[専用手順](STEAM-FRAME.md)を使ってください。

## 確認済み構成と制限

- 開発者が実機で視覚的に確認したHMD：**Pimax Dream Air**。
- 確認環境のGPU／ドライバー：GeForce RTX 5090、NVIDIA 610.88。プロバイダーは表示上のバージョンだけでなく、`nvapi64_impl.dll`と`nvwgf2umx.dll`のハッシュと視線ハンドラーのメソッド配置を検査します。別の実体を明示的に追加する実験用契約は[ドライバー契約](DRIVER-CONTRACTS.md)を参照してください。一致しない実体は拒否します。
- SteamVR/OpenVRのHMD投影情報、VRSSを許可するNVIDIAのVRChatプロファイル、Baballonia OSCからの左右視線、VRChat内のMSAAが必要です。
- 他のHMD、PC、ドライバービルド、OSC送信元は未検証です。導入診断に合格しても、視界内で高画質領域が追従する保証にはなりません。

## 導入手順

1. [Releases](https://github.com/challenger0303/VRSS-Tuner/releases)からポータブルZIPを取得し、全体を書き込み可能なフォルダーに展開します。`VRSS Tuner.exe`だけを取り出さず、スクリプトと`common-vrss-provider`も一緒に置いてください。
2. HMDを接続しSteamVRを起動します。VRChatは閉じておきます。Baballoniaから左右の視線OSCを`127.0.0.1:9020`に送るよう設定します。
3. `Test-Setup.cmd`を実行します。パッケージのハッシュ、NVIDIAドライバー、プロファイル、SteamVR/HMD投影情報、動作中のプロバイダーを読み取ります。ドライバー設定は変更しません。`readyForProviderLaunch: false`の場合、日英両方の理由が表示されます。
4. 不足が`vrssOn`だけなら、`VRSS Tuner.exe`の画質設定で**オン**にして保存し、Windowsの管理者確認を承認します。診断を再実行してください。`driverSupported`または`vrssAllowed`がfalseなら、この構成では共通プロバイダーを有効化できません。検査を飛ばさないでください。
5. `Start-Provider.cmd`を実行し、Windowsの管理者確認を承認します。Tunerの「視線入力」でソースに「Baballonia OSC」を選び、配信を開始します。プロバイダーはTunerからの左右視線をローカルUDPポート5556で受け取ります。TunerがBaballonia OSCを受ける既定のポートは9020です。「Pimax経路（純正優先）」を選んだ場合はTunerのOSC転送を使用しません。
6. VRChatを起動してグラフィック設定のMSAAを有効にします。HMD内で高画質領域が両目に現れ、視線の左右・上下に追従するか確認します。プロセスのログだけでは画質の確認になりません。
7. 終了時は`Stop-Provider.cmd`を実行します。これは正常終了を要求するもので、SteamVR、VRChat、ベンダーのプロバイダーを終了しません。

## Tunerの動作表示

「視線入力」タブには、OSC受信、Tunerからの転送、共通プロバイダーの登録、NVIDIAへの視線送信結果を別々に表示します。左右の点は**送信した視線位置**であり、高画質領域の輪郭ではありません。緑の「NVIDIA視線」は、VRアプリ側の照会があり視線更新APIが成功したことを示します。VRSSが実際に描画されたか、正しい位置にあるかはHMD内で確認してください。Pimax経路は詳細計測を公開しないため、TunerにはNVIDIAプロバイダーの起動状態のみ表示します。

## Pimax経路

「Pimax経路（純正優先）」はPimax純正XR5ランタイムが動いている間はその視線を優先します。SRanibroがカメラを占有する設定に切り替わった場合、別途導入済みのPimax視線ブリッジが新鮮な両眼視線を受け取れたときだけ代わりに登録します。TunerはSRanibroを自動起動せず、純正経路への切替を妨げません。検証済みのブリッジバイナリはポータブルZIPの`pimax-eye-bridge/`に含めましたが、自動導入はしません。

検証に使用したSRanibro 0.1.10-betaの`pimax_xr5`設定は純正サービスを停止・無効化しました。SRanibroを終了してもサービスが自動復帰しなかった場合は、Tunerの「純正サービスを復帰」を押してWindowsの管理者確認を承認してください。このボタンはSRanibro起動中には使用できません。サービスだけを復帰してもPimax側が視線デバイスを再登録しない場合があります。その場合はPimaxアプリの「サービスを再起動」を実行します（VRアプリが終了することがあります）。純正サービスの起動とプロバイダーの起動表示だけでは視線の有効性は証明できないため、最後はHMD内で高画質領域の追従を確認します。

## PSVR2の入力経路（未検証）

PSVR2では二つの入力経路を選べます。**PSVR2 Toolkit（直接）**はインストール済みToolkitのCAPIから両目の視線を読み、共通プロバイダーへ送ります。Tunerで選択し、`Start-PSVR2-Toolkit.cmd`を実行します。`psvr2_toolkit_capi.dll`を提供するToolkit版が必要です。Toolkit本体は同梱しません。**SRanibro（共通）**はローカルのBrokenEye形式の両目視線ストリーム（5555番ポート）を読みます。Tunerで選択し、SRanibroを起動してから`Start-SRanibro.cmd`を実行します。このSRanibro経路はPSVR2専用ではなく、接続中HMDのOpenVR投影情報を使用します。ただしHMDごとの視線座標系と補正は実機で確認が必要です。二つの入力元を同時には起動しません。「視線補正」タブでは共通の補正ファイルを使うため、HMD別の値はプロファイルに保存します。PSVR2での視線受信、NVIDIA登録、VRChatの高画質領域の追従は**実機未検証**です。[PSVR2Toolkit導入手順](https://github.com/BnuuySolutions/PSVR2Toolkit/wiki/Installation)で対応版の要件も確認してください。

## VIVEの入力経路（実機未検証）

**VIVE Pro Eye**は純正SRanipalを導入して視線を校正し、TunerでPro Eyeを選んで`Start-Vive-Pro-Eye.cmd`を実行します。**VIVE Streaming**はベンダーのPC視線転送を有効にし、対応する入力元と`Start-Vive-Streaming.cmd`を使います。DLLを自動検出できない場合は、`VRSS_VIVE_SDK_DLL`に導入済みのDLLのフルパスを指定してください。ベンダーDLLは同梱しません。両経路ともビルドと模擬座標テストのみ済んでおり、VIVE実機での視線受信とVRChatのVRSS2描画は未検証です。[HMD別の対応状況](HMD-GAZE-COMPATIBILITY.md)も参照してください。

## 位置補正

同梱の`common-vrss-provider/calibration.txt`は左右オフセット0、横・縦倍率1から始まります。HMDでコントラストの高い対象を見ながら、Tunerの視線補正で調整してください。視線と描画領域を比較するときは、できるだけ頭を止めてください。初期値は汎用値であり、**Dream Air専用の補正値ではありません**。

## 困ったとき

| 診断結果・症状 | 確認すること |
| --- | --- |
| `driverSupported: false` | NVIDIAの実ファイルまたは視線ハンドラーのメソッド位置が、内蔵契約・明示指定した実験用契約のどちらにも一致しません。`Audit-Driver.cmd`で読み取り専用の報告を採取できます。 |
| `vrssAllowed: false` | NVIDIAのVRChatプロファイルにVRSS許可設定がありません。このパッケージはプロファイルの強制作成や置換を行いません。 |
| `steamVrRunning: false`／`hmdGeometryValid: false` | HMDを接続してSteamVRを起動し、再診断します。 |
| 公式プロバイダーが動作中 | その経路を使うか、ベンダーの通常操作で停止してから共通経路を試してください。スクリプトは自動停止しません。 |
| プロバイダーは動くが領域が追従しない | Baballoniaが新しい左右視線を出しているか、Tunerの配信が開始したか、VRChatのMSAAが有効か、両目の実映像を確認してください。 |

診断結果と実行ログは`%LOCALAPPDATA%\VRSS Tuner`に保存します。共有前にPCやHMDの情報が含まれていないか確認してください。NVIDIA／Pimaxドライバー本体は同梱しておらず、未検証構成での動作は保証していません。
