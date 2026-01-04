#ifndef slic3r_AIPrintMonitor_hpp_
#define slic3r_AIPrintMonitor_hpp_

#include <wx/timer.h>
#include <wx/event.h>
#include <memory>
#include <string>

#include "GeminiClient.hpp"

namespace Slic3r {
class MachineObject;
class DeviceManager;
} // namespace Slic3r

namespace Slic3r { namespace GUI {

class AIPrintMonitor : public wxEvtHandler
{
public:
    AIPrintMonitor(DeviceManager* device_manager);
    ~AIPrintMonitor();

    void start_monitoring(int interval_ms = 120000); // Default 2 minutes
    void stop_monitoring();
    bool is_monitoring() const { return m_is_monitoring; }

    void                            set_active_adjustment(bool enabled) { m_active_adjustment_enabled = enabled; }
    bool                            is_active_adjustment_enabled() const { return m_active_adjustment_enabled; }
    const std::vector<std::string>& get_action_log() const { return m_action_log; }

    // Status tracking for UI
    std::string get_status_string() const { return m_current_status; }
    std::string get_last_result() const { return m_last_result; }
    int         get_interval_ms() const { return m_interval_ms; }

    void set_model_name(const std::string& model_name);

    // Event handler for timer
    void on_timer(wxTimerEvent& event);

    // Force an immediate check (for testing)
    void force_check_now();

private:
    DeviceManager*           m_device_manager;
    GeminiClient             m_gemini_client;
    wxTimer                  m_timer;
    bool                     m_is_monitoring             = false;
    bool                     m_active_adjustment_enabled = false;
    std::vector<std::string> m_action_log;

    // Status tracking
    std::string m_current_status = "Idle";
    std::string m_last_result    = "No checks yet";
    int         m_interval_ms    = 120000;

    // Helper to fetch snapshot
    void fetch_snapshot_and_analyze();
    // Internal analysis logic
    void perform_analysis(MachineObject* machine);
    void perform_analysis_with_ip(const std::string& ip);

    // Callback handling
    void on_analysis_success(const std::string& response);
    void on_analysis_error(const std::string& error);

    // Snapshot fetching (simple HTTP get)
    void get_snapshot(const std::string& url);

    // Moonraker G-code sender (for Klipper printers)
    void send_gcode_moonraker(const std::string& gcode);

    // Stored IP for Moonraker commands
    std::string m_moonraker_ip;
};

}} // namespace Slic3r::GUI

#endif // slic3r_AIPrintMonitor_hpp_
