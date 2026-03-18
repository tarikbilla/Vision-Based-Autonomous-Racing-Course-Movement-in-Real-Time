#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <string>
#include <map>

namespace rc_car {

class ConfigManager {
private:
    std::map<std::string, std::string> config_;
    std::string config_file_path_;
    
    void loadDefaults();
    void parseConfigFile(const std::string& filepath);
    
public:
    ConfigManager();
    explicit ConfigManager(const std::string& config_file);
    
    void load(const std::string& config_file);
    void save(const std::string& config_file = "");
    
    // Getters with defaults
    std::string getString(const std::string& key, const std::string& default_val = "") const;
    int getInt(const std::string& key, int default_val = 0) const;
    double getDouble(const std::string& key, double default_val = 0.0) const;
    bool getBool(const std::string& key, bool default_val = false) const;
    
    // Setters
    void setString(const std::string& key, const std::string& value);
    void setInt(const std::string& key, int value);
    void setDouble(const std::string& key, double value);
    void setBool(const std::string& key, bool value);
};

} // namespace rc_car

#endif // CONFIG_MANAGER_H
