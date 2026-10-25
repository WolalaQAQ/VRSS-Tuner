#include "Localization.h"
#include <iostream>

int main() {
    if (language::Text(L"視線補正") != L"視線補正") return 1;
    language::english = true;
    if (language::Text(L"視線補正") != L"Gaze calibration") return 2;
    if (language::Text(L"視線補正を反映") != L"Apply calibration") return 7;
    if (language::Text(L"色計算：1.15倍（＋15.0%）") != L"Pixel work: 1.15x (+15.0%)") return 8;
    if (language::Text(L"● 要再起動 = 対象アプリのみ。保存時に確認します。") !=
        L"● Restart required for the target app. You will be asked before saving.") return 3;
    if (language::Text(L"導入状態: 実行ファイル単体です。視線補正と共通プロバイダーは未接続です。") !=
        L"Setup: Standalone EXE. Gaze calibration and the common provider are not connected.") return 4;
    const auto status = language::Text(L"保存値: オン / 大きさ 50 / 縦横比 1");
    for (wchar_t ch : status) if (ch >= 0x3040 && ch <= 0x9fff) return 5;
    language::english = false;
    if (language::Text(L"画質設定を保存") != L"画質設定を保存") return 6;
    std::cout << "LOCALIZATION_PASS ja en dynamic_status" << std::endl;
    return 0;
}
