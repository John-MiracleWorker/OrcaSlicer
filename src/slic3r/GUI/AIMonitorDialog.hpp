#ifndef slic3r_AIMonitorDialog_hpp_
#define slic3r_AIMonitorDialog_hpp_

#include "GUI_Utils.hpp"
#include <wx/dialog.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include "Widgets/SwitchButton.hpp"

namespace Slic3r { namespace GUI {

class AIMonitorDialog : public DPIDialog
{
public:
    AIMonitorDialog(wxWindow* parent);
    ~AIMonitorDialog() = default;

protected:
    void on_dpi_changed(const wxRect& suggested_rect) override;

private:
    void build();

    wxStaticText* m_status_text;
    SwitchButton* m_toggle_active;
    wxButton*     m_btn_log;
    wxButton*     m_btn_test;
    wxTextCtrl*   m_ip_input; // Manual Moonraker IP
};

}} // namespace Slic3r::GUI

#endif // slic3r_AIMonitorDialog_hpp_
