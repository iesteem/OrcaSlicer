#ifndef slic3r_PlaceholderParser_hpp_
#define slic3r_PlaceholderParser_hpp_

#include "libslic3r.h"
#include <map>
#include <random>
#include <string>
#include <string_view>
#include <vector>
#include "PrintConfig.hpp"

namespace Slic3r {

class PlaceholderParser
{
public:
    // 在多次执行 PlaceholderParser 期间共享的上下文。
    // 上下文保持在 PlaceholderParser 外部，以便同一个 PlaceholderParser
    // 可以从多个线程安全地调用。
    // 将来，上下文可以保存由 PlaceholderParser 创建和修改的变量，
    // 并在 PlaceholderParser::process() 调用之间共享。
    struct ContextData {
        std::mt19937                    rng;
        // 如果定义，此字典由脚本用于定义用户变量并在
        // PlaceholderParser 评估之间持久保存它们。
        std::unique_ptr<DynamicConfig>  global_config;
    };

    PlaceholderParser(const DynamicConfig *external_config = nullptr);
    
    void clear_config() { m_config.clear(); }
    // 返回应更改 m_config 中来自 rhs 的键列表。
    // 包含在 rhs 中找到但不在 m_config 中的键。
    std::vector<std::string> config_diff(const DynamicPrintConfig &rhs);
    // 如果已修改则返回 true。
    bool apply_config(const DynamicPrintConfig &config);
    void apply_config(DynamicPrintConfig &&config);
    // 在 PlaceholderParser::config_diff() 返回的值上调用。
    // 这些键应该已经是有效的。
    void apply_only(const DynamicPrintConfig &config, const std::vector<std::string> &keys);
    void apply_env_variables();

    // 向 m_config 添加新的 ConfigOption 值。
    void set(const std::string &key, const std::string &value)  { this->set(key, new ConfigOptionString(value)); }
    void set(const std::string &key, std::string_view value)    { this->set(key, new ConfigOptionString(std::string(value))); }
    void set(const std::string &key, const char *value)         { this->set(key, new ConfigOptionString(value)); }
    void set(const std::string &key, int value)                 { this->set(key, new ConfigOptionInt(value)); }
    void set(const std::string &key, unsigned int value)        { this->set(key, int(value)); }
    void set(const std::string &key, bool value)                { this->set(key, new ConfigOptionBool(value)); }
    void set(const std::string &key, double value)              { this->set(key, new ConfigOptionFloat(value)); }
    void set(const std::string &key, const std::vector<std::string> &values) { this->set(key, new ConfigOptionStrings(values)); }
    void set(const std::string &key, ConfigOption *opt)         { m_config.set_key_value(key, opt); }
	DynamicConfig&			config_writable()					{ return m_config; }
	const DynamicConfig&    config() const                      { return m_config; }
    const ConfigOption*     option(const std::string &key) const { return m_config.option(key); }
    // 外部配置不被 PlaceholderParser 拥有。在查找选项时具有最低优先级。
	const DynamicConfig*	external_config() const  			{ return m_external_config; }

    // 使用宏处理语言填充模板。
    // 在语法或运行时错误时抛出 Slic3r::PlaceholderParserError。
    std::string process(const std::string &templ, unsigned int current_extruder_id, const DynamicConfig *config_override, DynamicConfig *config_outputs, ContextData *context) const;
    std::string process(const std::string &templ, unsigned int current_extruder_id = 0, const DynamicConfig *config_override = nullptr, ContextData *context = nullptr) const
        { return this->process(templ, current_extruder_id, config_override, nullptr /* config_outputs */, context); }

    // 使用 PlaceholderParser 布尔表达式语法的全部表达能力评估布尔表达式。
    // 在语法或运行时错误时抛出 Slic3r::PlaceholderParserError。
    static bool evaluate_boolean_expression(const std::string &templ, const DynamicConfig &config, const DynamicConfig *config_override = nullptr);

    // 更新提供的配置中的 timestamp, year, month, day, hour, minute, second 变量。
    static void update_timestamp(DynamicConfig &config);
    // 更新 m_config 中的 timestamp, year, month, day, hour, minute, second 变量。
    void update_timestamp() { update_timestamp(m_config); }

    static void update_user_name(DynamicConfig &config);
    void update_user_name() { update_user_name(m_config); }

private:
	// config 在查找符号时比 external_config 具有更高优先级。
    DynamicConfig 			 m_config;
    const DynamicConfig 	*m_external_config;
};

}

#endif
