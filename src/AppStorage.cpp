#include "AppStorage.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>
#include <plog/Log.h>

namespace {
constexpr std::size_t RECENT_FILE_LIMIT = 8;

std::filesystem::path normalizedPath(const std::string &value) {
    std::error_code error;
    auto result = std::filesystem::absolute(value, error);
    if (error) result = value;
    return result.lexically_normal();
}

void clampSettings(AppSettings &settings) {
    settings.width = std::clamp(settings.width, 640, 7680);
    settings.height = std::clamp(settings.height, 480, 4320);
    settings.tps = std::clamp(settings.tps, 1, 65536);
    if (settings.recentFiles.size() > RECENT_FILE_LIMIT) {
        settings.recentFiles.resize(RECENT_FILE_LIMIT);
    }
}
}

AppStorage::AppStorage(std::filesystem::path directory)
        : directory(std::move(directory)),
          settingsFile(this->directory / "settings.json"),
          recoveryFile(this->directory / "recovery.bson") {
}

std::filesystem::path AppStorage::defaultDataDirectory() {
#ifdef _WIN32
    if (const char *appData = std::getenv("APPDATA")) {
        return std::filesystem::path(appData) / "LogicalSystem";
    }
#elif defined(__APPLE__)
    if (const char *userHome = std::getenv("HOME")) {
        return std::filesystem::path(userHome) / "Library" / "Application Support" / "LogicalSystem";
    }
#else
    if (const char *xdgData = std::getenv("XDG_DATA_HOME")) {
        return std::filesystem::path(xdgData) / "logical-system";
    }
    if (const char *userHome = std::getenv("HOME")) {
        return std::filesystem::path(userHome) / ".local" / "share" / "logical-system";
    }
#endif
    return std::filesystem::temp_directory_path() / "LogicalSystem";
}

AppSettings AppStorage::loadSettings() const {
    AppSettings settings;
    std::filesystem::path source = settingsFile;
    if (!std::filesystem::exists(source) && std::filesystem::exists("settings.json")) {
        source = "settings.json";
    }

    try {
        std::ifstream file(source);
        if (file.good()) {
            const auto json = nlohmann::json::parse(file);
            settings.width = json.value("width", settings.width);
            settings.height = json.value("height", settings.height);
            settings.tps = json.value("tps", settings.tps);
            settings.vsync = json.value("vsync", settings.vsync);
            settings.bloom = json.value("bloom", settings.bloom);
            settings.recentFiles = json.value("recentFiles", settings.recentFiles);
        }
    } catch (const std::exception &error) {
        PLOGW << source.string() << " is invalid: " << error.what();
    }

    clampSettings(settings);
    return settings;
}

bool AppStorage::saveSettings(const AppSettings &settings) const {
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        PLOGE << "Failed to create settings directory: " << error.message();
        return false;
    }

    const nlohmann::json json{
            {"width",       settings.width},
            {"height",      settings.height},
            {"tps",         settings.tps},
            {"vsync",       settings.vsync},
            {"bloom",       settings.bloom},
            {"recentFiles", settings.recentFiles}
    };
    std::ofstream file(settingsFile);
    if (!file.good()) return false;
    file << json.dump(2);
    return file.good();
}

void AppStorage::rememberRecentFile(AppSettings &settings, const std::string &path) const {
    const std::string normalized = normalizedPath(path).string();
    settings.recentFiles.erase(
            std::remove_if(settings.recentFiles.begin(), settings.recentFiles.end(),
                           [&](const std::string &entry) {
                               return normalizedPath(entry) == normalizedPath(normalized);
                           }),
            settings.recentFiles.end());
    settings.recentFiles.insert(settings.recentFiles.begin(), normalized);
    if (settings.recentFiles.size() > RECENT_FILE_LIMIT) {
        settings.recentFiles.resize(RECENT_FILE_LIMIT);
    }
}

bool AppStorage::hasRecovery() const {
    std::error_code error;
    return std::filesystem::is_regular_file(recoveryFile, error) &&
           std::filesystem::file_size(recoveryFile, error) > 0;
}

bool AppStorage::clearRecovery() const {
    std::error_code error;
    const bool removed = std::filesystem::remove(recoveryFile, error);
    if (error) return false;
    if (removed) return true;
    return !std::filesystem::exists(recoveryFile, error) && !error;
}

const std::filesystem::path &AppStorage::recoveryPath() const {
    return recoveryFile;
}
