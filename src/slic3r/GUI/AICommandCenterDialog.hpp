#ifndef slic3r_AICommandCenterDialog_hpp_
#define slic3r_AICommandCenterDialog_hpp_

#include "GUI_Utils.hpp"
#include <wx/dialog.h>
#include <wx/notebook.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/panel.h>
#include "Widgets/SwitchButton.hpp"

namespace Slic3r { namespace GUI {

class AICommandCenterDialog : public DPIDialog
{
public:
    AICommandCenterDialog(wxWindow* parent);
    ~AICommandCenterDialog() = default;

protected:
    void on_dpi_changed(const wxRect& suggested_rect) override;

private:
    void build();

    // Tab builders
    wxPanel* build_settings_tab(wxWindow* parent);
    wxPanel* build_tuner_tab(wxWindow* parent);
    wxPanel* build_monitor_tab(wxWindow* parent);
    wxPanel* build_analyzer_tab(wxWindow* parent);
    wxPanel* build_supports_tab(wxWindow* parent);

    // Main notebook
    wxNotebook* m_notebook;

    // Settings Tab
    wxTextCtrl* m_api_key_input;

    // Monitor Tab
    SwitchButton* m_toggle_active;
    wxTextCtrl*   m_ip_input;

    // Tuner Tab
    wxTextCtrl*   m_tuner_input;
    wxStaticText* m_tuner_output;

    // Analyzer Tab
    wxTextCtrl* m_analyzer_output;

    // Supports Tab
    wxTextCtrl* m_supports_output;
};

}} // namespace Slic3r::GUI

#endif // slic3r_AICommandCenterDialog_hpp_
