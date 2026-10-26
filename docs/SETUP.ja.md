# 導入と確認（日本語）

[English](SETUP.md) · [プロジェクト概要](../README.ja.md)

Pimax純正経路と共通プロバイダー経路の起動手順です。NVIDIA 596.02のSteam Frameは[専用手順](STEAM-FRAME.md)を使ってください。

## 確認済み構成と制限

- 開発者が実機で視覚的に確認したHMD：**Pimax Dream Air**。
- 確認環境のGPU／ドライバー：GeForce RTX 5090、NVIDIA 610.88。プロバイダーは表示上のバージョンだけでなく、`nvapi64_impl.dll`と`nvwgf2umx.dll`のハッシュと視線ハンドラーのメソッド配置を検査します。別の実体を明示的に追加する実験用契約は[ドライバー契約](DRIVER-CONTRACTS.md)を参照してください。一致しない実体は拒否します。
- 共通経路にはSteamVR/OpenVRのHMD投影情報、VRSSを許可するNVIDIAのVRChatプロファイル、選択した入力元からの有効な視線、VRChat内のMSAAが必要です。Pimax純正経路ではPimaxの視線機能を使用します。
- 他のHMD、PC、ドライバービルドは実機未検証です。導入診断に合格しても、視界内で高画質領域が追従する保証にはなりません。

## 導入手順

1. [Releases](https://github.com/challenger0303/VRSS-Tuner/releases)からポータブルZIPを取得し、**全体を**書き込み可能なフォルダーに展開します。`VRSS Tuner.exe`だけを取り出さないでください。Tunerを開き、右上で言語を選びます。
2. **Pimax純正XR5を使う場合：** HMDとPimaxの視線機能を有効にし、Tunerの「視線入力」で「Pimax経路（純正優先）」を選びます。「画質」でVRSSをオンにして適用し、SteamVRとVRChatを通常どおり起動します。共通プロバイダー用の起動スクリプトは使いません。
3. **SRanibro・PSVR2 Toolkitなどの共通経路を使う場合：** HMDと対応する視線ソフトを起動し、SteamVRを接続します。Tunerの「視線入力」で入力元を一つ選び、「画質」でVRSSをオンにして適用します。「視線経路だけ起動」を押すと、導入診断とプロバイダー起動だけを行い、VRChatには触れません。Windowsの管理者確認は手動で承認してください。VRChatは後から自分で起動できます。
4. **Steam Frameの場合：** 上記の共通起動ボタンではなく、[Frame専用手順](STEAM-FRAME.md)に従います。
5. VRChatの「設定 → グラフィック」でMSAAを有効にします。比較にはx4から始め、HMD内で高画質領域が両目に現れ、視線の左右・上下に追従するか確認します。Tunerの緑の表示だけでは実際の画質変化は証明できません。
6. 共通経路の終了時は`Stop-Provider.cmd`を実行します。VRChat、SteamVR、ベンダーのプロバイダーは終了しません。Pimax純正経路ではこのスクリプトは不要です。

共通経路の診断だけを手動で行う場合は`Test-Setup.cmd`を使えます。`driverSupported`または`vrssAllowed`がfalseなら、この構成では共通プロバイダーを起動できません。`Start-Provider.cmd`はプロバイダー単体の手動診断用です。Baballonia OSCは既定で`127.0.0.1:9020`に左右視線を送ります。

固定HMDキーを使う構成でVRChatを後から起動する場合は、先にプロバイダーを起動し、`Start-VRChat-HMD-Key.cmd`を実行します。Steamの通常の起動ボタンを使いたい場合は、`Install-Steam-Launch-Bridge.cmd`の表示する起動オプションをSteamの「VRChat → プロパティ → 一般」に一度設定します。このSteam起動ブリッジは模擬起動のみ確認済みで、保護付きVRChatでの実動作は未検証です。IDはVRChat起動時に決まり、実行中には書き換えられません。視線ソフトの一時停止からの復帰はプロバイダーが登録を保っている間なら再起動なしで可能ですが、プロバイダー自体を停止・再登録した場合のVRChat再接続は未確認です。

SteamVRを開いたときにTunerと選択中の共通視線経路も起動したい場合は、展開先を固定してSteamVRを起動し、`Install-SteamVR-Autostart.cmd`を一度実行します。登録直後にSteamVRがアプリを認識しない場合は、SteamVRを通常終了・再起動してから同じコマンドを再実行します。以後、TunerはSteamVR起動時に開き、選択中の視線入力に合わせたプロバイダー起動を試みます。HMDと視線ソフトの準備が必要で、管理者確認が出る場合があります。Tunerの状態表示でプロバイダーが稼働したことを確認してからVRChatを起動してください。解除には`Remove-SteamVR-Autostart.cmd`を使います。このSteamVR自動起動経路はコードを追加した段階で、実機での起動順は未検証です。

## Tunerの動作表示

「視線入力」タブは入力元、Tunerの転送、プロバイダー登録、NVIDIAへの視線送信を別々に表示します。灰色は停止・データ待ち、橙は追加確認が必要、緑はその段階の観測成功です。左右の点は**送信した視線位置**であり、高画質領域の輪郭ではありません。VRSSが実際に描画されたか、正しい位置にあるかはHMD内で確認してください。Pimax純正経路では取得できるテレメトリーが異なります。

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
| プロバイダーは動くが領域が追従しない | 選んだ視線ソフトが新しい左右視線を出しているか、Tunerの入力・プロバイダー・NVIDIAの各段階、VRChatのMSAA、両目の実映像を順に確認してください。BaballoniaではTunerの配信開始も必要です。 |

診断結果と実行ログは`%LOCALAPPDATA%\VRSS Tuner`に保存します。共有前にPCやHMDの情報が含まれていないか確認してください。NVIDIA／Pimaxドライバー本体は同梱しておらず、未検証構成での動作は保証していません。
