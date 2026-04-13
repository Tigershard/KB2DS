#pragma once
#include "mapping.hpp"

namespace MappingStorage {
    void save(const kb::Config& config);
    kb::Config load();

    // Translate evdev KEY_* code to a human-readable string (e.g. KEY_R → "R")
    QString keyName(int code);
    // Translate a human-readable key name back to evdev code
    int keyCode(const QString& name);
}
