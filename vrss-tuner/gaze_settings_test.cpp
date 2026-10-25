#include "GazeSettings.h"
#include "../common-vrss-provider/config_io.h"
#include <atomic>
#include <iostream>
#include <thread>
#include <stdexcept>
static void Require(bool ok, const char* name) { if (!ok) throw std::runtime_error(name); }
int wmain(int argc, wchar_t** argv) {
    try {
        Require(argc == 2, "fixture directory required");
        live::fs::path root = argv[1];
        Require(!live::fs::exists(root), "fixture must be new");
        live::fs::create_directories(root);
        live::Store store{root / L"pimax.txt", root / L"common.txt", root / L"backups"};
        DWORD error{};
        std::string legacy = "left_deg=-2\r\nright_deg=2\r\n";
        Require(live::WriteNew(store.pimax, legacy, error), "create legacy");
        live::Values baseline{-2, 2, .5f, .55f};
        Require(live::WriteNew(store.common, live::Serialize(baseline), error), "create common");
        Require(live::Sha("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA256");
        live::Snapshot initial; Require(store.Read(live::Target::Pimax, initial), "read 2-line legacy");
        Require(initial.values.x_gain == 1 && initial.values.y_gain == 1, "legacy defaults");
        auto change = store.Apply(live::Target::Pimax, initial.hash, baseline);
        Require(change.code == 0 && change.changed, "apply live file");
        live::Snapshot changed; Require(store.Read(live::Target::Pimax, changed), "readback");
        Require(changed.values.x_gain == .5f && changed.values.y_gain == .55f, "gain readback");
        GazeCalibration::Values consumer;
        Require(GazeCalibration::Parse(changed.bytes, consumer), "production consumer accepts output");
        live::Snapshot common; Require(store.Read(live::Target::Common, common), "common input preserved");
        Require(common.bytes == live::Serialize(baseline), "target isolation");
        Require(store.Apply(live::Target::Pimax, initial.hash, baseline).code == 61, "stale read rejected");
        auto invalid = baseline; invalid.x_gain = 0;
        Require(store.Apply(live::Target::Pimax, changed.hash, invalid).code == 60, "zero gain rejected");
        invalid = baseline; invalid.left_deg = 6;
        Require(store.Apply(live::Target::Pimax, changed.hash, invalid).code == 60, "angle rejected");
        Require(store.ApplyText(live::Target::Pimax, changed.hash, "left_deg=nan\nright_deg=0\n").code == 60, "NaN rejected");
        Require(store.Restore(live::Target::Pimax, changed.hash).code == 0, "restore");
        live::Snapshot restored; Require(store.Read(live::Target::Pimax, restored), "restored read");
        Require(restored.bytes == legacy, "byte exact legacy restore");
        Require(store.Restore(live::Target::Pimax, restored.hash).code == 0, "undo restore");
        Require(store.Read(live::Target::Pimax, changed) && changed.bytes == live::Serialize(baseline), "restore toggle");
        SetFileAttributesW(store.pimax.c_str(), FILE_ATTRIBUTE_READONLY);
        invalid = baseline; invalid.x_gain = .6f;
        auto denied = store.Apply(live::Target::Pimax, changed.hash, invalid);
        Require(denied.code != 0 && !denied.changed, "failed replacement refused");
        SetFileAttributesW(store.pimax.c_str(), FILE_ATTRIBUTE_NORMAL);
        Require(store.Read(live::Target::Pimax, restored) && restored.bytes == changed.bytes, "failed write preserves target");
        std::atomic<bool> run{true}, readerOk{true};
        std::thread reader([&] { while (run) { live::Snapshot s; if (!store.Read(live::Target::Common, s)) readerOk = false; } });
        std::thread streamReader([&] { while (run) {
            // Exercise the exact common-provider settings reader as well.
            try {
                auto fields = ReadSharedConfig(store.common);
                if (fields.size() != 4 || fields.at("x_gain") != "0.500000" ||
                    (fields.at("y_gain") != "0.550000" && fields.at("y_gain") != "-0.550000")) readerOk = false;
            } catch (...) { readerOk = false; }
        } });
        for (int i = 0; i < 20; ++i) {
            Require(store.Read(live::Target::Common, common), "concurrent pre-read");
            baseline.y_gain = i % 2 ? .55f : -.55f;
            auto updated = store.Apply(live::Target::Common, common.hash, baseline);
            if (updated.code) { run = false; reader.join(); streamReader.join(); throw std::runtime_error("atomic update failed code=" + std::to_string(updated.code) + " winError=" + std::to_string(updated.winError)); }
        }
        run = false; reader.join(); streamReader.join(); Require(readerOk, "reader never observes partial file");
        std::cout << "GAZE_SETTINGS_PASS legacy production_parser isolation validation conflict backup byte_exact_restore readonly_failure atomic_reader\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
