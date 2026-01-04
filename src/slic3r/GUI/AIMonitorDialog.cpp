#include "AIMonitorDialog.hpp"
#include "GUI_App.hpp"
#include "I18N.hpp"
#include "DeviceCore/DevManager.h"
#include "AIPrintMonitor.hpp" // Required for AIPrintMonitor definition
#include <wx/sizer.h>
#include <wx/msgdlg.h>
#include <wx/stattext.h>
#include <wx/button.h>

namespace Slic3r { namespace GUI {

AIMonitorDialog::AIMonitorDialog(wxWindow* parent)
    : DPIDialog(parent, wxID_ANY, _L("AI Monitor"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    build();
    wxGetApp().UpdateDarkUI(this);
}

void AIMonitorDialog::build()
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
    this->SetSizer(main_sizer);

    // Padding
    main_sizer->AddSpacer(10);

    // Status Section
    wxBoxSizer* status_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_status_text            = new wxStaticText(this, wxID_ANY, _L("AI Active Protection:"));
    status_sizer->Add(m_status_text, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

    m_toggle_active = new SwitchButton(this);
    m_toggle_active->SetMinSize(wxSize(this->FromDIP(38), this->FromDIP(20)));

    // Set initial state
    bool is_active = wxGetApp().app_config->get("ai_monitoring_active") == "true";
    m_toggle_active->SetValue(is_active);

    m_toggle_active->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent& e) {
        bool                   enabled = m_toggle_active->GetValue();
        Slic3r::DeviceManager* dev     = Slic3r::GUI::wxGetApp().getDeviceManager();
        if (dev && dev->get_ai_monitor()) {
            dev->get_ai_monitor()->set_active_adjustment(enabled);
            wxGetApp().app_config->set("ai_monitoring_active", enabled ? "true" : "false");
        }
        m_toggle_active->Refresh(); // Ensure UI updates
        e.Skip();
    });

    status_sizer->Add(m_toggle_active, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    main_sizer->Add(status_sizer, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 10);

    // Button Sizer
    wxBoxSizer* btn_sizer = new wxBoxSizer(wxHORIZONTAL);

    // Test Button
    m_btn_test = new wxButton(this, wxID_ANY, _L("Simulate AI Check"));
    m_btn_test->Bind(wxEVT_BUTTON, [this](wxCommandEvent& e) {
        Slic3r::DeviceManager* dev = Slic3r::GUI::wxGetApp().getDeviceManager();
        if (dev && dev->get_ai_monitor()) {
            dev->get_ai_monitor()->force_check_now();
            wxMessageBox(_L("AI Check Initiated.\nPlease check the log in a few seconds."), _L("Test"), wxOK | wxICON_INFORMATION);
        } else {
            wxMessageBox(_L("AI Monitor is not initialized."), _L("Error"), wxICON_ERROR | wxOK);
        }
    });
    btn_sizer->Add(m_btn_test, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

    // Log Button
    m_btn_log = new wxButton(this, wxID_ANY, _L("View Action Log"));
    m_btn_log->Bind(wxEVT_BUTTON, [this](wxCommandEvent& e) {
        Slic3r::DeviceManager* dev = Slic3r::GUI::wxGetApp().getDeviceManager();
        if (dev && dev->get_ai_monitor()) {
            std::vector<std::string> logs = dev->get_ai_monitor()->get_action_log();
            wxString                 log_msg;
            for (const auto& line : logs) {
                log_msg += wxString::FromUTF8(line) + "\n";
            }
            if (log_msg.IsEmpty())
                log_msg = "No AI actions recorded yet.";
            wxMessageBox(log_msg, _L("AI Action Log"), wxOK | wxICON_INFORMATION);
        } else {
            wxMessageBox(_L("AI Monitor is not initialized."), _L("Error"), wxICON_ERROR | wxOK);
        }
    });
    btn_sizer->Add(m_btn_log, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

    main_sizer->Add(btn_sizer, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 0);

    // Disclaimer
    wxStaticText* disclaimer = new wxStaticText(this, wxID_ANY,
                                                _L("When active, AI will pause prints if defects are detected (Spaghetti/Layer Shift)."));
    disclaimer->Wrap(this->FromDIP(300));
    disclaimer->SetForegroundColour(wxColour(128, 128, 128));
    main_sizer->Add(disclaimer, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 10);

    // --- Klipper/Moonraker IP Config ---
    wxStaticText* ip_label = new wxStaticText(this, wxID_ANY, _L("Moonraker IP (for Klipper):"));
    main_sizer->Add(ip_label, 0, wxLEFT | wxRIGHT | wxTOP, 10);

    wxBoxSizer* ip_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_ip_input           = new wxTextCtrl(this, wxID_ANY, wxGetApp().app_config->get("ai_monitor_ip"), wxDefaultPosition,
                                          wxSize(this->FromDIP(180), -1));
    ip_sizer->Add(m_ip_input, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5);

    wxButton* save_ip_btn = new wxButton(this, wxID_ANY, _L("Save IP"));
    save_ip_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent& e) {
        std::string ip = m_ip_input->GetValue().ToStdString();
        wxGetApp().app_config->set("ai_monitor_ip", ip);
        wxMessageBox(_L("IP Saved: ") + ip, _L("Success"), wxOK | wxICON_INFORMATION);
    });
    ip_sizer->Add(save_ip_btn, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    main_sizer->Add(ip_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    main_sizer->AddSpacer(10);

    main_sizer->SetSizeHints(this);
    this->CenterOnParent();
}

void AIMonitorDialog::on_dpi_changed(const wxRect& suggested_rect)
{
    // Handle DPI changes if necessary, usually just Layout()
    this->Layout();
}

}} // namespace Slic3r::GUI
