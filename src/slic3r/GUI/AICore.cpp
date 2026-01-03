#include "AICore.hpp"
#include "libslic3r/AppConfig.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/Preset.hpp"
#include "libslic3r/PrintConfig.hpp"
#include <boost/property_tree/json_parser.hpp>
#include <sstream>

namespace Slic3r { namespace GUI {

static int clamp_objective(int value)
{
    if (value < 0)
        return 0;
    if (value > 100)
        return 100;
    return value;
}

AIObjectives load_ai_objectives(AppConfig* config)
{
    AIObjectives objectives;
    if (!config)
        return objectives;

    try {
        std::string value = config->get("ai_objective_quality");
        if (!value.empty())
            objectives.quality = clamp_objective(std::stoi(value));
    } catch (...) {
        ;
    }
    try {
        std::string value = config->get("ai_objective_speed");
        if (!value.empty())
            objectives.speed = clamp_objective(std::stoi(value));
    } catch (...) {
        ;
    }
    try {
        std::string value = config->get("ai_objective_strength");
        if (!value.empty())
            objectives.strength = clamp_objective(std::stoi(value));
    } catch (...) {
        ;
    }

    return objectives;
}

void save_ai_objectives(AppConfig* config, const AIObjectives& objectives)
{
    if (!config)
        return;

    config->set("ai_objective_quality", std::to_string(clamp_objective(objectives.quality)));
    config->set("ai_objective_speed", std::to_string(clamp_objective(objectives.speed)));
    config->set("ai_objective_strength", std::to_string(clamp_objective(objectives.strength)));
    config->save();
}

std::string build_objective_text(const AIObjectives& objectives)
{
    std::stringstream ss;
    ss << "Objectives (0-100): "
       << "quality=" << clamp_objective(objectives.quality) << ", "
       << "speed=" << clamp_objective(objectives.speed) << ", "
       << "strength=" << clamp_objective(objectives.strength) << ". "
       << "Prioritize higher values and explain trade-offs when objectives conflict.\n";
    return ss.str();
}

static void append_section(std::stringstream& ss,
                           const std::string& title,
                           const DynamicPrintConfig& config,
                           const std::vector<std::string>& keys)
{
    ss << title << " Total " << keys.size() << "\n";
    for (const auto& key : keys) {
        if (config.has(key)) {
            ss << key << ": " << config.opt_serialize(key) << "\n";
        } else {
            ss << key << ": <unset>\n";
        }
    }
}

static void collect_advanced_keys(const std::vector<std::string>& keys, std::vector<std::string>& out)
{
    for (const auto& key : keys) {
        if (const ConfigOptionDef* def = print_config_def.get(key); def && def->mode >= comAdvanced) {
            out.push_back(key);
        }
    }
}

static void append_enum_values(std::stringstream& ss, const std::vector<std::string>& keys)
{
    for (const auto& key : keys) {
        const ConfigOptionDef* def = print_config_def.get(key);
        if (!def || (def->type != coEnum && def->type != coEnums) || def->enum_values.empty())
            continue;
        ss << key << ": ";
        for (size_t i = 0; i < def->enum_values.size(); ++i) {
            ss << def->enum_values[i];
            if (i + 1 < def->enum_values.size())
                ss << ", ";
        }
        ss << "\n";
    }
}

std::string build_settings_snapshot(const PresetBundle& bundle, const AIObjectives& objectives)
{
    std::stringstream ss;

    ss << build_objective_text(objectives) << "\n";

    const Preset& print_preset    = bundle.prints.get_edited_preset();
    const Preset& filament_preset = bundle.filaments.get_edited_preset();
    const Preset& printer_preset  = bundle.printers.get_edited_preset();

    ss << "Context\n";
    ss << "print_preset: " << print_preset.name << "\n";
    if (!bundle.filament_presets.empty()) {
        ss << "filament_presets: ";
        for (size_t i = 0; i < bundle.filament_presets.size(); ++i) {
            ss << bundle.filament_presets[i];
            if (i + 1 < bundle.filament_presets.size())
                ss << ", ";
        }
        ss << "\n";
    } else {
        ss << "filament_preset: " << filament_preset.name << "\n";
    }
    ss << "printer_preset: " << printer_preset.name << "\n";

    const DynamicPrintConfig& printer_config  = printer_preset.config;
    const DynamicPrintConfig& filament_config = filament_preset.config;

    if (printer_config.has("printer_model"))
        ss << "printer_model: " << printer_config.opt_serialize("printer_model") << "\n";
    if (printer_config.has("printer_variant"))
        ss << "printer_variant: " << printer_config.opt_serialize("printer_variant") << "\n";

    if (printer_config.has("gcode_flavor"))
        ss << "gcode_flavor: " << printer_config.opt_serialize("gcode_flavor") << "\n";
    if (printer_config.has("nozzle_diameter"))
        ss << "nozzle_diameter: " << printer_config.opt_serialize("nozzle_diameter") << "\n";

    int extruder_count = bundle.get_printer_extruder_count();
    if (extruder_count > 0)
        ss << "extruder_count: " << extruder_count << "\n";

    if (bundle.project_config.has("curr_bed_type"))
        ss << "curr_bed_type: " << bundle.project_config.opt_serialize("curr_bed_type") << "\n";

    if (filament_config.has("filament_type"))
        ss << "filament_type: " << filament_config.opt_serialize("filament_type") << "\n";
    if (filament_config.has("filament_diameter"))
        ss << "filament_diameter: " << filament_config.opt_serialize("filament_diameter") << "\n";

    ss << "\n";

    const auto& print_keys    = Preset::print_options();
    const auto& filament_keys = Preset::filament_options();
    const auto& printer_keys  = Preset::printer_options();

    append_section(ss, "Print settings (all current values, including advanced options).",
                   print_preset.config, print_keys);
    ss << "\n";
    append_section(ss, "Filament settings (all current values).",
                   filament_config, filament_keys);
    ss << "\n";
    append_section(ss, "Printer settings (all current values).",
                   printer_config, printer_keys);

    const auto project_keys = bundle.project_config.keys();
    if (!project_keys.empty()) {
        ss << "\n";
        append_section(ss, "Project overrides (current values).", bundle.project_config, project_keys);
    }

    std::vector<std::string> advanced_keys;
    advanced_keys.reserve(print_keys.size() + filament_keys.size() + printer_keys.size());
    collect_advanced_keys(print_keys, advanced_keys);
    collect_advanced_keys(filament_keys, advanced_keys);
    collect_advanced_keys(printer_keys, advanced_keys);

    ss << "\nAdvanced settings (mode advanced or develop; must consider and optimize). Total " << advanced_keys.size() << "\n";
    for (const auto& key : advanced_keys) {
        if (print_preset.config.has(key)) {
            ss << key << ": " << print_preset.config.opt_serialize(key) << "\n";
        } else if (filament_config.has(key)) {
            ss << key << ": " << filament_config.opt_serialize(key) << "\n";
        } else if (printer_config.has(key)) {
            ss << key << ": " << printer_config.opt_serialize(key) << "\n";
        } else if (bundle.project_config.has(key)) {
            ss << key << ": " << bundle.project_config.opt_serialize(key) << "\n";
        } else {
            ss << key << ": <unset>\n";
        }
    }

    ss << "\nAllowed enum values (advanced only, use these exact values):\n";
    append_enum_values(ss, advanced_keys);
    ss << "Never use 'grid' for sparse_infill_pattern.\n";

    return ss.str();
}

std::string extract_response_text(const std::string& response)
{
    try {
        std::stringstream           ss(response);
        boost::property_tree::ptree pt;
        boost::property_tree::read_json(ss, pt);

        std::string content_text;
        if (pt.get_child_optional("candidates")) {
            for (auto& candidate : pt.get_child("candidates")) {
                for (auto& part : candidate.second.get_child("content.parts")) {
                    content_text += part.second.get<std::string>("text");
                }
            }
        } else if (pt.get_child_optional("choices")) {
            for (auto& choice : pt.get_child("choices")) {
                if (auto msg = choice.second.get_optional<std::string>("message.content"))
                    content_text += *msg;
                else if (auto text = choice.second.get_optional<std::string>("text"))
                    content_text += *text;
            }
        }
        return content_text;
    } catch (...) {
        return "";
    }
}

std::string strip_json_fences(const std::string& text)
{
    std::string out = text;
    size_t      json_start = out.find("```json");
    if (json_start != std::string::npos) {
        out = out.substr(json_start + 7);
        size_t json_end = out.rfind("```");
        if (json_end != std::string::npos) {
            out = out.substr(0, json_end);
        }
        return out;
    }

    json_start = out.find("```");
    if (json_start != std::string::npos) {
        out = out.substr(json_start + 3);
        size_t json_end = out.rfind("```");
        if (json_end != std::string::npos) {
            out = out.substr(0, json_end);
        }
    }
    return out;
}

}} // namespace Slic3r::GUI
