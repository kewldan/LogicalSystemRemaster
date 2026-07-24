#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct AppSettings {
    int width = 1280;
    int height = 720;
    int tps = 8;
    bool vsync = true;
    bool bloom = true;
    std::vector<std::string> recentFiles;
};

class AppStorage {
public:
    explicit AppStorage(std::filesystem::path directory = defaultDataDirectory());

    [[nodiscard]] AppSettings loadSettings() const;

    bool saveSettings(const AppSettings &settings) const;

    void rememberRecentFile(AppSettings &settings, const std::string &path) const;

    [[nodiscard]] bool hasRecovery() const;

    bool clearRecovery() const;

    [[nodiscard]] const std::filesystem::path &recoveryPath() const;

    [[nodiscard]] static std::filesystem::path defaultDataDirectory();

private:
    std::filesystem::path directory;
    std::filesystem::path settingsFile;
    std::filesystem::path recoveryFile;
};
