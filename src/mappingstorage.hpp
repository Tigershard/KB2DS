#pragma once
#include "mapping.hpp"
#include <QStringList>

namespace MappingStorage {
    // Current mapping (auto-saved to ~/.config/KB2DS/mappings.json)
    void save(const kb::Config& config);
    kb::Config load();

    // Named profiles (stored in ~/.config/KB2DS/profiles/<name>.json)
    QStringList listProfiles();
    void saveProfile(const kb::Config& config, const QString& name);
    kb::Config loadProfile(const QString& name);
    kb::Config loadFromPath(const QString& path);  // load any .json by absolute path
    void deleteProfile(const QString& name);   // also removes cover image

    // Path for the cover image associated with a profile (<name>.png)
    QString profileCoverPath(const QString& name);

    // Translate evdev KEY_* code to a human-readable string (e.g. KEY_R → "R")
    QString keyName(int code);
    // Translate a human-readable key name back to evdev code
    int keyCode(const QString& name);
}
