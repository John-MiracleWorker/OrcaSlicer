#include "AICommandCenterDialog.hpp"
#include "GUI_App.hpp"
#include "I18N.hpp"
#include "DeviceCore/DevManager.h"
#include "AIPrintMonitor.hpp"
#include "GeminiClient.hpp"
#include "Plater.hpp"
#include "libslic3r/Model.hpp"
#include <wx/sizer.h>
#include <wx/msgdlg.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/statline.h>
#include <wx/notebook.h>
#include <sstream>

namespace Slic3r { namespace GUI {

// Custom colors for premium look
static const wxColour ACCENT_COLOR(0, 180, 216); // Cyan/Teal #00b4d8
static const wxColour BG_DARK(30, 30, 30);       // #1e1e1e
static const wxColour BG_CARD(45, 45, 45);       // Slightly lighter for cards
static const wxColour TEXT_PRIMARY(255, 255, 255);
static const wxColour TEXT_SECONDARY(160, 160, 160);

AICommandCenterDialog::AICommandCenterDialog(wxWindow* parent)
    : DPIDialog(parent, wxID_ANY, _L("AI Command Center"), wxDefaultPosition, wxSize(600, 500), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    SetBackgroundColour(BG_DARK);
    build();
    wxGetApp().UpdateDarkUI(this);
    CenterOnParent();
}

void AICommandCenterDialog::build()
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
    this->SetSizer(main_sizer);

    // Header
    wxStaticText* header = new wxStaticText(this, wxID_ANY, _L("AI Command Center"));
    header->SetFont(header->GetFont().Bold().Scaled(1.5f));
    header->SetForegroundColour(TEXT_PRIMARY);
    main_sizer->Add(header, 0, wxALIGN_CENTER | wxTOP | wxBOTTOM, this->FromDIP(15));

    // Notebook (Tabbed Interface)
    m_notebook = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_LEFT);
    m_notebook->SetBackgroundColour(BG_DARK);

    // Add tabs
    m_notebook->AddPage(build_settings_tab(m_notebook), _L("Settings"), true);
    m_notebook->AddPage(build_tuner_tab(m_notebook), _L("Tuner"));
    m_notebook->AddPage(build_monitor_tab(m_notebook), _L("Monitor"));
    m_notebook->AddPage(build_analyzer_tab(m_notebook), _L("Analyzer"));
    m_notebook->AddPage(build_supports_tab(m_notebook), _L("Supports"));

    main_sizer->Add(m_notebook, 1, wxEXPAND | wxALL, this->FromDIP(10));

    // Footer
    wxBoxSizer* footer_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton*   close_btn    = new wxButton(this, wxID_CANCEL, _L("Close"));
    footer_sizer->AddStretchSpacer();
    footer_sizer->Add(close_btn, 0, wxALL, this->FromDIP(5));
    main_sizer->Add(footer_sizer, 0, wxEXPAND | wxBOTTOM, this->FromDIP(10));

    main_sizer->SetSizeHints(this);
}

// ==================== SETTINGS TAB ====================
wxPanel* AICommandCenterDialog::build_settings_tab(wxWindow* parent)
{
    wxPanel* panel = new wxPanel(parent);
    panel->SetBackgroundColour(BG_DARK);
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    panel->SetSizer(sizer);

    // Title
    wxStaticText* title = new wxStaticText(panel, wxID_ANY, _L("AI Settings"));
    title->SetFont(title->GetFont().Bold().Scaled(1.2f));
    title->SetForegroundColour(TEXT_PRIMARY);
    sizer->Add(title, 0, wxALL, panel->FromDIP(10));

    // API Key Section
    wxStaticText* api_label = new wxStaticText(panel, wxID_ANY, _L("Gemini API Key:"));
    api_label->SetForegroundColour(TEXT_SECONDARY);
    sizer->Add(api_label, 0, wxLEFT | wxTOP, panel->FromDIP(10));

    wxBoxSizer* api_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_api_key_input       = new wxTextCtrl(panel, wxID_ANY, wxGetApp().app_config->get("ai_gemini_api_key"), wxDefaultPosition,
                                           wxSize(panel->FromDIP(300), -1), wxTE_PASSWORD);
    api_sizer->Add(m_api_key_input, 1, wxALIGN_CENTER_VERTICAL | wxALL, panel->FromDIP(5));

    wxButton* save_key_btn = new wxButton(panel, wxID_ANY, _L("Save"));
    save_key_btn->SetBackgroundColour(ACCENT_COLOR);
    save_key_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        std::string key = m_api_key_input->GetValue().ToStdString();
        wxGetApp().app_config->set("ai_gemini_api_key", key);
        wxMessageBox(_L("API Key saved!"), _L("Success"), wxOK | wxICON_INFORMATION);
    });
    api_sizer->Add(save_key_btn, 0, wxALIGN_CENTER_VERTICAL | wxALL, panel->FromDIP(5));
    sizer->Add(api_sizer, 0, wxEXPAND);

    // Info
    wxStaticText* info = new wxStaticText(panel, wxID_ANY,
                                          _L("Get your API key from Google AI Studio:\nhttps://aistudio.google.com/app/apikey"));
    info->SetForegroundColour(TEXT_SECONDARY);
    info->Wrap(panel->FromDIP(350));
    sizer->Add(info, 0, wxALL, panel->FromDIP(10));

    sizer->AddStretchSpacer();
    return panel;
}

// ==================== TUNER TAB ====================
wxPanel* AICommandCenterDialog::build_tuner_tab(wxWindow* parent)
{
    wxPanel* panel = new wxPanel(parent);
    panel->SetBackgroundColour(BG_DARK);
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    panel->SetSizer(sizer);

    // Title
    wxStaticText* title = new wxStaticText(panel, wxID_ANY, _L("AI Slicer Tuner"));
    title->SetFont(title->GetFont().Bold().Scaled(1.2f));
    title->SetForegroundColour(TEXT_PRIMARY);
    sizer->Add(title, 0, wxALL, panel->FromDIP(10));

    // Description
    wxStaticText* desc = new wxStaticText(panel, wxID_ANY, _L("Describe your print goals or issues, and AI will suggest optimal settings."));
    desc->SetForegroundColour(TEXT_SECONDARY);
    desc->Wrap(panel->FromDIP(400));
    sizer->Add(desc, 0, wxLEFT | wxRIGHT, panel->FromDIP(10));

    // Input
    m_tuner_input = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxSize(-1, panel->FromDIP(80)), wxTE_MULTILINE);
    m_tuner_input->SetHint(_L("e.g., I need a strong functional part, or I'm getting stringing..."));
    sizer->Add(m_tuner_input, 0, wxEXPAND | wxALL, panel->FromDIP(10));

    // Analyze Button
    wxButton* analyze_btn = new wxButton(panel, wxID_ANY, _L("Get AI Suggestions"));
    analyze_btn->SetBackgroundColour(ACCENT_COLOR);
    analyze_btn->Bind(wxEVT_BUTTON, [this, panel](wxCommandEvent&) {
        // Placeholder - would integrate with GeminiClient
        m_tuner_output->SetLabel(_L("Analyzing... (Feature coming soon)"));
    });
    sizer->Add(analyze_btn, 0, wxALIGN_CENTER | wxALL, panel->FromDIP(5));

    // Output
    m_tuner_output = new wxStaticText(panel, wxID_ANY, "");
    m_tuner_output->SetForegroundColour(TEXT_PRIMARY);
    m_tuner_output->Wrap(panel->FromDIP(400));
    sizer->Add(m_tuner_output, 1, wxEXPAND | wxALL, panel->FromDIP(10));

    return panel;
}

// ==================== MONITOR TAB ====================
wxPanel* AICommandCenterDialog::build_monitor_tab(wxWindow* parent)
{
    wxPanel* panel = new wxPanel(parent);
    panel->SetBackgroundColour(BG_DARK);
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    panel->SetSizer(sizer);

    // Title
    wxStaticText* title = new wxStaticText(panel, wxID_ANY, _L("AI Print Monitor"));
    title->SetFont(title->GetFont().Bold().Scaled(1.2f));
    title->SetForegroundColour(TEXT_PRIMARY);
    sizer->Add(title, 0, wxALL, panel->FromDIP(10));

    // Toggle Section
    wxBoxSizer*   toggle_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* toggle_label = new wxStaticText(panel, wxID_ANY, _L("AI Guard Active:"));
    toggle_label->SetForegroundColour(TEXT_PRIMARY);
    toggle_sizer->Add(toggle_label, 0, wxALIGN_CENTER_VERTICAL | wxALL, panel->FromDIP(5));

    m_toggle_active = new SwitchButton(panel);
    m_toggle_active->SetMinSize(wxSize(panel->FromDIP(38), panel->FromDIP(20)));
    bool is_active = wxGetApp().app_config->get("ai_monitoring_active") == "true";
    m_toggle_active->SetValue(is_active);
    m_toggle_active->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent&) {
        bool                   enabled = m_toggle_active->GetValue();
        Slic3r::DeviceManager* dev     = Slic3r::GUI::wxGetApp().getDeviceManager();
        if (dev && dev->get_ai_monitor()) {
            dev->get_ai_monitor()->set_active_adjustment(enabled);
            // Start or stop the monitoring timer
            if (enabled) {
                dev->get_ai_monitor()->start_monitoring(120000); // 2 minutes
            } else {
                dev->get_ai_monitor()->stop_monitoring();
            }
            wxGetApp().app_config->set("ai_monitoring_active", enabled ? "true" : "false");
        }
    });
    toggle_sizer->Add(m_toggle_active, 0, wxALIGN_CENTER_VERTICAL | wxALL, panel->FromDIP(5));
    sizer->Add(toggle_sizer, 0, wxEXPAND);

    sizer->Add(new wxStaticLine(panel), 0, wxEXPAND | wxALL, panel->FromDIP(10));

    // Moonraker IP Section
    wxStaticText* ip_label = new wxStaticText(panel, wxID_ANY, _L("Moonraker IP (for Klipper):"));
    ip_label->SetForegroundColour(TEXT_SECONDARY);
    sizer->Add(ip_label, 0, wxLEFT | wxTOP, panel->FromDIP(10));

    wxBoxSizer* ip_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_ip_input           = new wxTextCtrl(panel, wxID_ANY, wxGetApp().app_config->get("ai_monitor_ip"), wxDefaultPosition,
                                          wxSize(panel->FromDIP(200), -1));
    m_ip_input->SetHint(_L("e.g., 192.168.1.100"));
    ip_sizer->Add(m_ip_input, 1, wxALIGN_CENTER_VERTICAL | wxALL, panel->FromDIP(5));

    wxButton* save_ip_btn = new wxButton(panel, wxID_ANY, _L("Save IP"));
    save_ip_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        wxGetApp().app_config->set("ai_monitor_ip", m_ip_input->GetValue().ToStdString());
        wxMessageBox(_L("IP Saved!"), _L("Success"), wxOK | wxICON_INFORMATION);
    });
    ip_sizer->Add(save_ip_btn, 0, wxALIGN_CENTER_VERTICAL | wxALL, panel->FromDIP(5));
    sizer->Add(ip_sizer, 0, wxEXPAND);

    sizer->Add(new wxStaticLine(panel), 0, wxEXPAND | wxALL, panel->FromDIP(10));

    // Action Buttons
    wxBoxSizer* btn_sizer = new wxBoxSizer(wxHORIZONTAL);

    wxButton* test_btn = new wxButton(panel, wxID_ANY, _L("Simulate Check"));
    test_btn->SetBackgroundColour(ACCENT_COLOR);
    test_btn->Bind(wxEVT_BUTTON, [](wxCommandEvent&) {
        Slic3r::DeviceManager* dev = Slic3r::GUI::wxGetApp().getDeviceManager();
        if (dev && dev->get_ai_monitor()) {
            dev->get_ai_monitor()->force_check_now();
            wxMessageBox(_L("AI Check Initiated!\nCheck logs in a few seconds."), _L("Test"), wxOK | wxICON_INFORMATION);
        }
    });
    btn_sizer->Add(test_btn, 0, wxALL, panel->FromDIP(5));

    wxButton* log_btn = new wxButton(panel, wxID_ANY, _L("View Logs"));
    log_btn->Bind(wxEVT_BUTTON, [](wxCommandEvent&) {
        Slic3r::DeviceManager* dev = Slic3r::GUI::wxGetApp().getDeviceManager();
        if (dev && dev->get_ai_monitor()) {
            std::vector<std::string> logs = dev->get_ai_monitor()->get_action_log();
            wxString                 log_msg;
            for (const auto& line : logs)
                log_msg += wxString::FromUTF8(line) + "\n";
            if (log_msg.IsEmpty())
                log_msg = "No AI actions recorded yet.";
            wxMessageBox(log_msg, _L("AI Action Log"), wxOK | wxICON_INFORMATION);
        }
    });
    btn_sizer->Add(log_btn, 0, wxALL, panel->FromDIP(5));
    sizer->Add(btn_sizer, 0, wxALIGN_CENTER);

    sizer->AddStretchSpacer();
    return panel;
}

// ==================== ANALYZER TAB ====================
wxPanel* AICommandCenterDialog::build_analyzer_tab(wxWindow* parent)
{
    wxPanel* panel = new wxPanel(parent);
    panel->SetBackgroundColour(BG_DARK);
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    panel->SetSizer(sizer);

    wxStaticText* title = new wxStaticText(panel, wxID_ANY, _L("AI Model Analyzer"));
    title->SetFont(title->GetFont().Bold().Scaled(1.2f));
    title->SetForegroundColour(TEXT_PRIMARY);
    sizer->Add(title, 0, wxALL, panel->FromDIP(10));

    wxStaticText* desc =
        new wxStaticText(panel, wxID_ANY,
                         _L("Analyze your 3D model for potential print issues like overhangs, thin walls, or bad orientation."));
    desc->SetForegroundColour(TEXT_SECONDARY);
    desc->Wrap(panel->FromDIP(400));
    sizer->Add(desc, 0, wxLEFT | wxRIGHT, panel->FromDIP(10));

    wxButton* analyze_btn = new wxButton(panel, wxID_ANY, _L("Analyze Current Model"));
    analyze_btn->SetBackgroundColour(ACCENT_COLOR);
    analyze_btn->Bind(wxEVT_BUTTON, [this, panel](wxCommandEvent&) {
        Plater* plater = wxGetApp().plater();
        if (!plater || plater->model().objects.empty()) {
            m_analyzer_output->SetValue(_L("No model loaded. Please load a 3D model first."));
            return;
        }

        m_analyzer_output->SetValue(_L("Analyzing model geometry..."));

        // Extract geometry info from first object
        const ModelObject* obj         = plater->model().objects.front();
        Vec3d              size        = obj->raw_bounding_box().size();
        double             volume      = size.x() * size.y() * size.z(); // Approx from bbox
        int                facet_count = 0;
        for (const auto& vol : obj->volumes) {
            facet_count += vol->mesh().facets_count();
        }

        std::stringstream prompt;
        prompt << "I have a 3D model for FDM printing. Please analyze it for printability:\n";
        prompt << "- Model name: " << obj->name << "\n";
        prompt << "- Size (XYZ mm): " << size.x() << " x " << size.y() << " x " << size.z() << "\n";
        prompt << "- Volume: " << volume << " mm³\n";
        prompt << "- Facet count: " << facet_count << "\n\n";
        prompt << "Please identify potential issues (overhangs, thin walls, orientation problems) and suggest improvements.";

        std::string api_key = wxGetApp().app_config->get("ai_gemini_api_key");
        if (api_key.empty()) {
            m_analyzer_output->SetValue(_L("Please set your Gemini API key in the Settings tab first."));
            return;
        }

        GeminiClient client;
        client.set_api_key(api_key);
        client.analyze_text(
            prompt.str(),
            [this](const std::string& response) {
                CallAfter([this, response]() { m_analyzer_output->SetValue(wxString::FromUTF8(response)); });
            },
            [this](const std::string& error) {
                CallAfter([this, error]() { m_analyzer_output->SetValue("Error: " + wxString::FromUTF8(error)); });
            });
    });
    sizer->Add(analyze_btn, 0, wxALIGN_CENTER | wxALL, panel->FromDIP(10));

    // Output area
    m_analyzer_output = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxSize(-1, panel->FromDIP(200)),
                                       wxTE_MULTILINE | wxTE_READONLY);
    m_analyzer_output->SetBackgroundColour(BG_CARD);
    m_analyzer_output->SetForegroundColour(TEXT_PRIMARY);
    sizer->Add(m_analyzer_output, 1, wxEXPAND | wxALL, panel->FromDIP(10));

    return panel;
}

// ==================== SUPPORTS TAB ====================
wxPanel* AICommandCenterDialog::build_supports_tab(wxWindow* parent)
{
    wxPanel* panel = new wxPanel(parent);
    panel->SetBackgroundColour(BG_DARK);
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    panel->SetSizer(sizer);

    wxStaticText* title = new wxStaticText(panel, wxID_ANY, _L("AI Support Assistant"));
    title->SetFont(title->GetFont().Bold().Scaled(1.2f));
    title->SetForegroundColour(TEXT_PRIMARY);
    sizer->Add(title, 0, wxALL, panel->FromDIP(10));

    wxStaticText* desc =
        new wxStaticText(panel, wxID_ANY,
                         _L("Let AI suggest optimal support placement for your model, minimizing material while ensuring printability."));
    desc->SetForegroundColour(TEXT_SECONDARY);
    desc->Wrap(panel->FromDIP(400));
    sizer->Add(desc, 0, wxLEFT | wxRIGHT, panel->FromDIP(10));

    wxButton* suggest_btn = new wxButton(panel, wxID_ANY, _L("Suggest Supports"));
    suggest_btn->SetBackgroundColour(ACCENT_COLOR);
    suggest_btn->Bind(wxEVT_BUTTON, [this, panel](wxCommandEvent&) {
        Plater* plater = wxGetApp().plater();
        if (!plater || plater->model().objects.empty()) {
            m_supports_output->SetValue(_L("No model loaded. Please load a 3D model first."));
            return;
        }

        m_supports_output->SetValue(_L("Analyzing model for support requirements..."));

        // Extract geometry info
        const ModelObject* obj  = plater->model().objects.front();
        Vec3d              size = obj->raw_bounding_box().size();

        std::stringstream prompt;
        prompt << "I have a 3D model for FDM printing that needs support analysis:\n";
        prompt << "- Model name: " << obj->name << "\n";
        prompt << "- Size (XYZ mm): " << size.x() << " x " << size.y() << " x " << size.z() << "\n\n";
        prompt << "Please suggest:\n";
        prompt << "1. Which areas likely need supports (overhangs >45°)\n";
        prompt << "2. Recommended support type (normal, tree, organic)\n";
        prompt << "3. Any orientation changes that could reduce supports\n";
        prompt << "4. Estimated support material percentage\n";

        std::string api_key = wxGetApp().app_config->get("ai_gemini_api_key");
        if (api_key.empty()) {
            m_supports_output->SetValue(_L("Please set your Gemini API key in the Settings tab first."));
            return;
        }

        GeminiClient client;
        client.set_api_key(api_key);
        client.analyze_text(
            prompt.str(),
            [this](const std::string& response) {
                CallAfter([this, response]() { m_supports_output->SetValue(wxString::FromUTF8(response)); });
            },
            [this](const std::string& error) {
                CallAfter([this, error]() { m_supports_output->SetValue("Error: " + wxString::FromUTF8(error)); });
            });
    });
    sizer->Add(suggest_btn, 0, wxALIGN_CENTER | wxALL, panel->FromDIP(10));

    // Output area
    m_supports_output = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxSize(-1, panel->FromDIP(200)),
                                       wxTE_MULTILINE | wxTE_READONLY);
    m_supports_output->SetBackgroundColour(BG_CARD);
    m_supports_output->SetForegroundColour(TEXT_PRIMARY);
    sizer->Add(m_supports_output, 1, wxEXPAND | wxALL, panel->FromDIP(10));

    return panel;
}

void AICommandCenterDialog::on_dpi_changed(const wxRect& suggested_rect) { this->Layout(); }

}} // namespace Slic3r::GUI
