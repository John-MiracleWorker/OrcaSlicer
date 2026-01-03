#ifndef slic3r_AICore_hpp_
#define slic3r_AICore_hpp_

#include <string>
#include <vector>

namespace Slic3r {
class AppConfig;
class PresetBundle;
}

namespace Slic3r { namespace GUI {

struct AIObjectives {
    int quality  = 50;
    int speed    = 50;
    int strength = 50;
};

AIObjectives load_ai_objectives(AppConfig* config);
void         save_ai_objectives(AppConfig* config, const AIObjectives& objectives);

std::string build_objective_text(const AIObjectives& objectives);
std::string build_settings_snapshot(const PresetBundle& bundle, const AIObjectives& objectives);

std::string extract_response_text(const std::string& response);
std::string strip_json_fences(const std::string& text);

}} // namespace Slic3r::GUI

#endif // slic3r_AICore_hpp_
