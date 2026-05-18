#pragma once
#include <string>
#include <fstream>
#include <filesystem>
#include <map>

class Config {
public:
    Config() {
        path_ = std::string(getenv("HOME") ? getenv("HOME") : "/home/matt")
                + "/.config/aeromcp/config.ini";
        std::filesystem::create_directories(
            std::filesystem::path(path_).parent_path());
        load();
    }

    std::string get(const std::string& key, const std::string& def = "") const {
        auto it = data_.find(key);
        return it != data_.end() ? it->second : def;
    }

    void set(const std::string& key, const std::string& val) {
        data_[key] = val;
        save();
    }

private:
    std::string path_;
    std::map<std::string, std::string> data_;

    void load() {
        std::ifstream f(path_);
        std::string line;
        while (std::getline(f, line)) {
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string k = line.substr(0, eq);
            std::string v = line.substr(eq + 1);
            data_[k] = v;
        }
    }

    void save() {
        std::ofstream f(path_);
        for (const auto& [k, v] : data_)
            f << k << "=" << v << "\n";
    }
};
