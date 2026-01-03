#ifndef slic3r_AIPrintMonitor_hpp_
#define slic3r_AIPrintMonitor_hpp_

#include <wx/timer.h>
#include <wx/event.h>
#include <memory>
#include <string>

#include "GeminiClient.hpp"

namespace Slic3r {
class DeviceManager;

namespace GUI {

class AIPrintMonitor : public wxEvtHandler
{
public:
    AIPrintMonitor(DeviceManager* device_manager);
    ~AIPrintMonitor();

    void start_monitoring(int interval_ms = 120000); // Default 2 minutes
    void stop_monitoring();
    bool is_monitoring() const { return m_is_monitoring; }

    void set_model_name(const std::string& model_name);

    // Event handler for timer
    void on_timer(wxTimerEvent& event);

private:
    DeviceManager* m_device_manager;
    GeminiClient   m_gemini_client;
    wxTimer        m_timer;
    bool           m_is_monitoring = false;

    // Helper to fetch snapshot
    void fetch_snapshot_and_analyze();

    // Callback handling
    void on_analysis_success(const std::string& response);
    void on_analysis_error(const std::string& error);

    // Snapshot fetching (simple HTTP get)
    void get_snapshot(const std::string& url);
};

} // namespace GUI
} // namespace Slic3r

#endif // slic3r_AIPrintMonitor_hpp_
