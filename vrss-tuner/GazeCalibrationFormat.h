#pragma once

#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>

// Shared text format used by the gaze calibration consumer. Keep the parser
// local so building the tuner does not require a Pimax driver source tree.
struct GazeCalibration {
    struct Values {
        float left_deg = 0.0f;
        float right_deg = 0.0f;
        float x_gain = 1.0f;
        float y_gain = 1.0f;
    };

    static bool Parse(std::string_view text, Values& result) {
        std::string_view lines[4];
        size_t count = 0;
        while (!text.empty()) {
            if (count == 4) return false;
            const size_t end = text.find('\n');
            auto line = text.substr(0, end);
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            lines[count++] = line;
            if (end == std::string_view::npos) break;
            text.remove_prefix(end + 1);
        }
        Values parsed;
        if ((count != 2 && count != 4) ||
            !ParseField(lines[0], "left_deg=", parsed.left_deg) ||
            !ParseField(lines[1], "right_deg=", parsed.right_deg) ||
            std::abs(parsed.left_deg) > 5.0f || std::abs(parsed.right_deg) > 5.0f) return false;
        if (count == 4 &&
            (!ParseField(lines[2], "x_gain=", parsed.x_gain) ||
             !ParseField(lines[3], "y_gain=", parsed.y_gain) ||
             parsed.x_gain < 0.1f || parsed.x_gain > 2.0f ||
             std::abs(parsed.y_gain) < 0.1f || std::abs(parsed.y_gain) > 2.0f)) return false;
        result = parsed;
        return true;
    }

private:
    static bool ParseField(std::string_view line, std::string_view key, float& result) {
        if (line.substr(0, key.size()) != key) return false;
        std::string value(line.substr(key.size()));
        if (value.empty()) return false;
        char* end = nullptr;
        const float parsed = std::strtof(value.c_str(), &end);
        if (end == value.c_str() || *end != '\0' || !std::isfinite(parsed)) return false;
        result = parsed;
        return true;
    }
};
