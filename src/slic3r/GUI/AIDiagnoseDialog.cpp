#include "AIDiagnoseDialog.hpp"
#include "I18N.hpp"
#include "GUI_App.hpp"
#include "MainFrame.hpp"
#include "libslic3r/AppConfig.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "ConfigManipulation.hpp"
#include "libslic3r/PrintConfig.hpp"

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/msgdlg.h>
#include <wx/defs.h>
#include <wx/gdicmn.h>
#include <wx/button.h>
#include <wx/textctrl.h>
#include <wx/gauge.h>
#include <wx/image.h>
#include <wx/mstream.h>
#include <wx/base64.h>
#include <boost/filesystem.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <algorithm>
#include <cctype>
#include <fstream>

#include "slic3r/GUI/GLCanvas3D.hpp"
#include "libslic3r/GCode/ThumbnailData.hpp"
#include "utility"

namespace Slic3r { namespace GUI {

AIDiagnoseDialog::AIDiagnoseDialog(wxWindow* parent)
    : DPIDialog(parent, wxID_ANY, _L("AI Failure Diagnosis"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    build();
    load_ai_settings();
    update_provider_ui();

    std::string key = load_api_key();
    if (!key.empty()) {
        m_gemini_client.set_api_key(key);
    } else if (selected_provider_key() == "gemini") {
        this->CallAfter([this]() { prompt_for_api_key(); });
    }
}

AIDiagnoseDialog::~AIDiagnoseDialog() {}

void AIDiagnoseDialog::build()
{
    auto main_sizer = new wxBoxSizer(wxVERTICAL);
    this->SetSizer(main_sizer);

    auto intro_text = new wxStaticText(
        this, wxID_ANY,
        _L("Describe the print failure (e.g., 'stringing', 'warping', 'layer shifts').\n"
           "Attach a photo if available; the AI will analyze settings and propose fixes."));
    main_sizer->Add(intro_text, 0, wxALL | wxEXPAND, 10);

    auto provider_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto provider_label = new wxStaticText(this, wxID_ANY, _L("AI Provider:"));
    m_provider_choice   = new wxChoice(this, wxID_ANY);
    m_provider_choice->Append(_L("Gemini (Cloud)"));
    m_provider_choice->Append(_L("Local Model (OpenAI-compatible)"));
    m_provider_choice->Bind(wxEVT_CHOICE, &AIDiagnoseDialog::on_provider_changed, this);
    provider_sizer->Add(provider_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    provider_sizer->Add(m_provider_choice, 1, wxALIGN_CENTER_VERTICAL | wxEXPAND);
    main_sizer->Add(provider_sizer, 0, wxLEFT | wxRIGHT | wxTOP | wxEXPAND, 10);

    auto local_sizer = new wxFlexGridSizer(2, 2, 6, 8);
    local_sizer->Add(new wxStaticText(this, wxID_ANY, _L("Local Endpoint:")), 0, wxALIGN_CENTER_VERTICAL);
    m_local_endpoint = new wxTextCtrl(this, wxID_ANY, "");
    local_sizer->Add(m_local_endpoint, 1, wxEXPAND);
    local_sizer->Add(new wxStaticText(this, wxID_ANY, _L("Local Model:")), 0, wxALIGN_CENTER_VERTICAL);
    m_local_model = new wxTextCtrl(this, wxID_ANY, "");
    local_sizer->Add(m_local_model, 1, wxEXPAND);
    local_sizer->AddGrowableCol(1, 1);
    main_sizer->Add(local_sizer, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);

    auto photo_label = new wxStaticText(this, wxID_ANY, _L("Failure photo (optional):"));
    main_sizer->Add(photo_label, 0, wxLEFT | wxRIGHT | wxTOP, 10);
    m_photo_picker = new wxFilePickerCtrl(
        this, wxID_ANY, "", _L("Choose a photo"),
        "Image files (*.png;*.jpg;*.jpeg;*.bmp)|*.png;*.jpg;*.jpeg;*.bmp");
    main_sizer->Add(m_photo_picker, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);

    m_input_desc = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(400, 120), wxTE_MULTILINE);
    main_sizer->Add(m_input_desc, 1, wxALL | wxEXPAND, 10);

    auto btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_btn_analyze  = new wxButton(this, wxID_ANY, _L("Diagnose"));
    m_btn_analyze->Bind(wxEVT_BUTTON, &AIDiagnoseDialog::on_analyze, this);

    m_btn_key = new wxButton(this, wxID_ANY, _L("API Key..."));
    m_btn_key->Bind(wxEVT_BUTTON, &AIDiagnoseDialog::on_change_key, this);

    m_progress = new wxGauge(this, wxID_ANY, 100);

    btn_sizer->Add(m_btn_analyze, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    btn_sizer->Add(m_btn_key, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    btn_sizer->Add(m_progress, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);

    main_sizer->Add(btn_sizer, 0, wxALL | wxEXPAND, 10);

    auto result_label = new wxStaticText(this, wxID_ANY, _L("AI Diagnosis & Suggestions:"));
    main_sizer->Add(result_label, 0, wxLEFT | wxTOP, 10);

    m_result_view = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(400, 220), wxTE_MULTILINE | wxTE_READONLY);
    main_sizer->Add(m_result_view, 1, wxALL | wxEXPAND, 10);

    auto diff_label = new wxStaticText(this, wxID_ANY, _L("Proposed Changes Preview:"));
    main_sizer->Add(diff_label, 0, wxLEFT | wxTOP, 10);

    m_diff_view = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(400, 180), wxTE_MULTILINE | wxTE_READONLY);
    main_sizer->Add(m_diff_view, 1, wxALL | wxEXPAND, 10);

    auto bottom_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_btn_apply       = new wxButton(this, wxID_ANY, _L("Apply Changes"));
    m_btn_apply->Bind(wxEVT_BUTTON, &AIDiagnoseDialog::on_apply, this);
    m_btn_apply->Disable();

    auto btn_close = new wxButton(this, wxID_CANCEL, _L("Close"));

    bottom_sizer->AddStretchSpacer();
    bottom_sizer->Add(m_btn_apply, 0, wxRIGHT, 5);
    bottom_sizer->Add(btn_close, 0, wxLEFT, 5);

    main_sizer->Add(bottom_sizer, 0, wxALL | wxEXPAND, 10);

    SetMinSize(wxSize(540, 760));
    Fit();
    CenterOnParent();
}

void AIDiagnoseDialog::on_analyze(wxCommandEvent& event)
{
    std::string user_input = m_input_desc->GetValue().ToStdString();
    if (user_input.empty()) {
        wxMessageBox(_L("Please describe the issue first."), _L("Input Required"), wxICON_WARNING | wxOK);
        return;
    }

    PresetBundle* preset_bundle = wxGetApp().preset_bundle;
    if (!preset_bundle) {
        wxMessageBox(_L("Preset settings are not available yet. Please open a project or restart OrcaSlicer."),
                     _L("AI Failure Diagnosis"), wxICON_ERROR | wxOK);
        return;
    }

    save_ai_settings();
    const std::string provider = selected_provider_key();

    if (provider == "gemini") {
        std::string key = load_api_key();
        if (key.empty()) {
            const char* env_key = std::getenv("GEMINI_API_KEY");
            if (env_key)
                key = env_key;
        }
        if (key.empty()) {
            wxMessageBox(_L("Gemini API key is missing. Please set it first."),
                         _L("AI Failure Diagnosis"), wxICON_WARNING | wxOK);
            prompt_for_api_key();
            return;
        }
        m_gemini_client.set_api_key(key);
    } else if (provider == "local") {
        std::string endpoint = m_local_endpoint ? m_local_endpoint->GetValue().ToStdString() : "";
        std::string model    = m_local_model ? m_local_model->GetValue().ToStdString() : "";
        if (endpoint.empty() || model.empty()) {
            wxMessageBox(_L("Local endpoint and model are required for local AI."),
                         _L("AI Failure Diagnosis"), wxICON_WARNING | wxOK);
            return;
        }
        m_local_client.set_endpoint(endpoint);
        m_local_client.set_model(model);
    }

    m_btn_analyze->Disable();
    m_btn_apply->Disable();
    m_progress->Pulse();
    m_result_view->SetValue(_L("Analyzing... please wait."));
    if (m_diff_view)
        m_diff_view->Clear();

    AIObjectives objectives = load_ai_objectives(wxGetApp().app_config);
    std::string settings_summary = build_settings_snapshot(*preset_bundle, objectives);

    std::string image_data_base64 = load_photo_base64();
    if (image_data_base64.empty())
        image_data_base64 = capture_scene_base64();

    std::string task_context =
        "Task: Diagnose the print failure described by the user. "
        "Identify likely root causes and propose targeted setting changes. "
        "Prefer changes that directly address the defect over global refactors.";

    auto on_success = [this](const std::string& response) {
        this->CallAfter([this, response]() {
            m_progress->SetValue(100);
            m_btn_analyze->Enable();

            try {
                std::string content_text = extract_response_text(response);
                if (content_text.empty())
                    content_text = response;

                content_text = strip_json_fences(content_text);

                std::string normalized_text = content_text;
                bool        adjusted_grid   = false;

                try {
                    std::stringstream           result_ss(content_text);
                    boost::property_tree::ptree result_pt;
                    boost::property_tree::read_json(result_ss, result_pt);

                    auto to_lower = [](std::string value) {
                        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                        return value;
                    };

                    if (auto changes_opt = result_pt.get_child_optional("changes")) {
                        for (auto& change_item : *changes_opt) {
                            std::string key   = change_item.second.get<std::string>("key", "");
                            std::string value = change_item.second.get<std::string>("value", "");
                            if (key == "sparse_infill_pattern") {
                                std::string lower = to_lower(value);
                                if (lower.find("grid") != std::string::npos) {
                                    change_item.second.put("value", "rectilinear");
                                    adjusted_grid = true;
                                }
                            }
                        }
                    }

                    if (adjusted_grid) {
                        std::ostringstream out;
                        boost::property_tree::write_json(out, result_pt, false);
                        normalized_text = out.str();
                    }
                } catch (const std::exception&) {
                    ;
                }

                m_analysis_result = normalized_text;
                m_result_view->SetValue(normalized_text);
                update_diff_preview(normalized_text);
                m_btn_apply->Enable();
            } catch (const std::exception& e) {
                m_result_view->SetValue("Error parsing AI response: " + std::string(e.what()) + "\nRaw: " + response);
                m_btn_analyze->Enable();
            }
        });
    };

    auto on_error = [this](const std::string& error) {
        this->CallAfter([this, error]() {
            m_progress->SetValue(0);
            m_btn_analyze->Enable();
            wxMessageBox(error, _L("Analysis Failed"), wxICON_ERROR | wxOK);
        });
    };

    if (provider == "local") {
        m_local_client.query_refinement(user_input, settings_summary, image_data_base64, task_context, on_success, on_error);
    } else {
        m_gemini_client.query_refinement(user_input, settings_summary, image_data_base64, task_context, on_success, on_error);
    }
}

void AIDiagnoseDialog::on_apply(wxCommandEvent& event)
{
    PresetBundle* preset_bundle = wxGetApp().preset_bundle;
    if (!preset_bundle) {
        wxMessageBox(_L("Preset settings are not available yet. Please open a project or restart OrcaSlicer."),
                     _L("AI Failure Diagnosis"), wxICON_ERROR | wxOK);
        return;
    }

    try {
        std::stringstream           ss(m_analysis_result);
        boost::property_tree::ptree pt;
        boost::property_tree::read_json(ss, pt);

        DynamicPrintConfig* print_config    = &preset_bundle->prints.get_edited_preset().config;
        DynamicPrintConfig* filament_config = &preset_bundle->filaments.get_edited_preset().config;
        DynamicPrintConfig* printer_config  = &preset_bundle->printers.get_edited_preset().config;
        DynamicPrintConfig* project_config  = &preset_bundle->project_config;

        DynamicPrintConfig new_print_config    = *print_config;
        DynamicPrintConfig new_filament_config = *filament_config;
        DynamicPrintConfig new_printer_config  = *printer_config;
        DynamicPrintConfig new_project_config  = *project_config;

        const auto& print_keys    = Preset::print_options();
        const auto& filament_keys = Preset::filament_options();
        const auto& printer_keys  = Preset::printer_options();

        bool changed_print    = false;
        bool changed_filament = false;
        bool changed_printer  = false;
        bool changed_project  = false;
        std::vector<std::string> failures;

        auto in_list = [](const std::string& key, const std::vector<std::string>& keys) {
            return std::find(keys.begin(), keys.end(), key) != keys.end();
        };

        for (auto& item : pt.get_child("changes")) {
            std::string key       = item.second.get<std::string>("key");
            std::string value_str = item.second.get<std::string>("value");
            std::string value_norm = normalize_ai_value(key, value_str);

            try {
                ConfigSubstitutionContext substitution_context(ForwardCompatibilitySubstitutionRule::Disable);
                if (in_list(key, filament_keys)) {
                    new_filament_config.set_deserialize(key, value_norm, substitution_context);
                    changed_filament = true;
                    BOOST_LOG_TRIVIAL(info) << "AI Diagnose applied (filament): " << key << " = " << value_norm;
                } else if (in_list(key, print_keys)) {
                    new_print_config.set_deserialize(key, value_norm, substitution_context);
                    changed_print = true;
                    BOOST_LOG_TRIVIAL(info) << "AI Diagnose applied (print): " << key << " = " << value_norm;
                } else if (in_list(key, printer_keys)) {
                    new_printer_config.set_deserialize(key, value_norm, substitution_context);
                    changed_printer = true;
                    BOOST_LOG_TRIVIAL(info) << "AI Diagnose applied (printer): " << key << " = " << value_norm;
                } else if (project_config->has(key)) {
                    new_project_config.set_deserialize(key, value_norm, substitution_context);
                    changed_project = true;
                    BOOST_LOG_TRIVIAL(info) << "AI Diagnose applied (project): " << key << " = " << value_norm;
                } else {
                    BOOST_LOG_TRIVIAL(error) << "AI Diagnose failed to apply: " << key << " = " << value_norm;
                    failures.push_back(key + ": unsupported setting");
                }
            } catch (const std::exception& e) {
                failures.push_back(key + ": " + e.what());
                BOOST_LOG_TRIVIAL(error) << "AI Diagnose failed to apply: " << key << " = " << value_norm << " (" << e.what() << ")";
            }
        }

        bool applied_any = false;
        if (changed_print && !print_config->diff(new_print_config).empty()) {
            ConfigManipulation cm;
            cm.apply(print_config, &new_print_config);
            applied_any = true;
        }
        if (changed_filament && !filament_config->diff(new_filament_config).empty()) {
            ConfigManipulation cm;
            cm.apply(filament_config, &new_filament_config);
            applied_any = true;
        }
        if (changed_printer && !printer_config->diff(new_printer_config).empty()) {
            ConfigManipulation cm;
            cm.apply(printer_config, &new_printer_config);
            applied_any = true;
        }
        if (changed_project && !project_config->diff(new_project_config).empty()) {
            ConfigManipulation cm;
            cm.apply(project_config, &new_project_config);
            applied_any = true;
        }

        if (applied_any) {
            if (Plater* plater = wxGetApp().plater()) {
                plater->update_project_dirty_from_presets();
            }
            wxGetApp().load_current_presets(false);
            if (!failures.empty()) {
                std::string msg = "Some settings could not be applied:\n";
                for (const auto& failure : failures) {
                    msg += " - " + failure + "\n";
                }
                wxMessageBox(msg, _L("Partial Success"), wxICON_WARNING | wxOK);
            } else {
                wxMessageBox(_L("Settings applied successfully!"), _L("Success"), wxICON_INFORMATION | wxOK);
            }
            EndModal(wxID_OK);
        } else {
            wxMessageBox(_L("No valid settings changes found to apply."), _L("Warning"), wxICON_WARNING | wxOK);
        }
    } catch (const std::exception& e) {
        wxMessageBox(std::string("Failed to apply settings: ") + e.what(), _L("Error"), wxICON_ERROR | wxOK);
    }
}

void AIDiagnoseDialog::on_change_key(wxCommandEvent& event) { prompt_for_api_key(); }

void AIDiagnoseDialog::on_provider_changed(wxCommandEvent& event)
{
    update_provider_ui();
    save_ai_settings();
    if (selected_provider_key() == "gemini" && load_api_key().empty()) {
        this->CallAfter([this]() { prompt_for_api_key(); });
    }
}

void AIDiagnoseDialog::on_dpi_changed(const wxRect& suggested_rect)
{
    // Handle DPI changes
}

std::string AIDiagnoseDialog::selected_provider_key() const
{
    int selection = m_provider_choice ? m_provider_choice->GetSelection() : 0;
    return (selection == 1) ? "local" : "gemini";
}

void AIDiagnoseDialog::load_ai_settings()
{
    AppConfig* config = wxGetApp().app_config;
    std::string provider = config ? config->get("ai_provider") : "";
    if (!m_provider_choice)
        return;

    if (provider == "local") {
        m_provider_choice->SetSelection(1);
    } else {
        m_provider_choice->SetSelection(0);
    }

    std::string endpoint = config ? config->get("ai_local_endpoint") : "";
    if (endpoint.empty())
        endpoint = "http://localhost:11434/v1/chat/completions";
    if (m_local_endpoint)
        m_local_endpoint->SetValue(endpoint);

    std::string model = config ? config->get("ai_local_model") : "";
    if (model.empty())
        model = "llama3";
    if (m_local_model)
        m_local_model->SetValue(model);
}

void AIDiagnoseDialog::save_ai_settings()
{
    AppConfig* config = wxGetApp().app_config;
    if (!config)
        return;

    config->set("ai_provider", selected_provider_key());

    if (m_local_endpoint)
        config->set("ai_local_endpoint", m_local_endpoint->GetValue().ToStdString());
    if (m_local_model)
        config->set("ai_local_model", m_local_model->GetValue().ToStdString());

    config->save();
}

void AIDiagnoseDialog::update_provider_ui()
{
    const bool use_local = selected_provider_key() == "local";
    if (m_local_endpoint)
        m_local_endpoint->Enable(use_local);
    if (m_local_model)
        m_local_model->Enable(use_local);
    if (m_btn_key)
        m_btn_key->Enable(!use_local);
}

std::string AIDiagnoseDialog::normalize_ai_value(const std::string& key, const std::string& value) const
{
    std::string trimmed = value;
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front())))
        trimmed.erase(trimmed.begin());
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back())))
        trimmed.pop_back();

    auto to_lower = [](std::string input) {
        std::transform(input.begin(), input.end(), input.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return input;
    };

    std::string lower = to_lower(trimmed);

    if (key == "sparse_infill_pattern") {
        if (lower.find("grid") != std::string::npos) {
            return std::string("rectilinear");
        }
    }

    if (key == "support_style") {
        if (lower.find("tree") != std::string::npos) {
            if (lower.find("slim") != std::string::npos)
                return std::string("tree_slim");
            if (lower.find("strong") != std::string::npos)
                return std::string("tree_strong");
            if (lower.find("hybrid") != std::string::npos)
                return std::string("tree_hybrid");
            if (lower.find("organic") != std::string::npos)
                return std::string("organic");
            return std::string("default");
        }
        if (lower.find("organic") != std::string::npos)
            return std::string("organic");
        if (lower.find("grid") != std::string::npos)
            return std::string("grid");
        if (lower.find("snug") != std::string::npos)
            return std::string("snug");
        if (lower.find("default") != std::string::npos || lower.find("auto") != std::string::npos)
            return std::string("default");
    } else if (key == "support_type") {
        if (lower.find("tree") != std::string::npos) {
            return (lower.find("manual") != std::string::npos) ? std::string("tree(manual)") : std::string("tree(auto)");
        }
        if (lower.find("normal") != std::string::npos) {
            return (lower.find("manual") != std::string::npos) ? std::string("normal(manual)") : std::string("normal(auto)");
        }
        if (lower.find("auto") != std::string::npos)
            return std::string("normal(auto)");
    }

    return trimmed;
}

void AIDiagnoseDialog::update_diff_preview(const std::string& analysis_json)
{
    if (!m_diff_view)
        return;

    PresetBundle* preset_bundle = wxGetApp().preset_bundle;
    if (!preset_bundle) {
        m_diff_view->SetValue("Preset settings are not available yet.");
        return;
    }

    try {
        std::stringstream           ss(analysis_json);
        boost::property_tree::ptree pt;
        boost::property_tree::read_json(ss, pt);

        std::stringstream out;
        const auto& print_keys    = Preset::print_options();
        const auto& filament_keys = Preset::filament_options();
        const auto& printer_keys  = Preset::printer_options();

        auto in_list = [](const std::string& key, const std::vector<std::string>& keys) {
            return std::find(keys.begin(), keys.end(), key) != keys.end();
        };

        DynamicPrintConfig& print_config    = preset_bundle->prints.get_edited_preset().config;
        DynamicPrintConfig& filament_config = preset_bundle->filaments.get_edited_preset().config;
        DynamicPrintConfig& printer_config  = preset_bundle->printers.get_edited_preset().config;
        DynamicPrintConfig& project_config  = preset_bundle->project_config;

        if (auto changes_opt = pt.get_child_optional("changes")) {
            for (auto& item : *changes_opt) {
                std::string key       = item.second.get<std::string>("key");
                std::string value_str = item.second.get<std::string>("value");
                std::string reason    = item.second.get<std::string>("reason", "");
                std::string value_norm = normalize_ai_value(key, value_str);

                const DynamicPrintConfig* scope_cfg = nullptr;
                std::string scope_name = "unknown";

                if (in_list(key, filament_keys)) {
                    scope_cfg = &filament_config;
                    scope_name = "filament";
                } else if (in_list(key, print_keys)) {
                    scope_cfg = &print_config;
                    scope_name = "print";
                } else if (in_list(key, printer_keys)) {
                    scope_cfg = &printer_config;
                    scope_name = "printer";
                } else if (project_config.has(key)) {
                    scope_cfg = &project_config;
                    scope_name = "project";
                }

                std::string old_value = "<unset>";
                if (scope_cfg && scope_cfg->has(key))
                    old_value = scope_cfg->opt_serialize(key);

                out << key << " (" << scope_name << "): " << old_value << " -> " << value_norm;
                if (!reason.empty())
                    out << " // " << reason;
                out << "\n";
            }
        } else {
            out << "No changes found in AI response.\n";
        }

        m_diff_view->SetValue(out.str());
    } catch (const std::exception& e) {
        m_diff_view->SetValue(std::string("Failed to parse AI response: ") + e.what());
    }
}

void AIDiagnoseDialog::prompt_for_api_key()
{
    wxTextEntryDialog dlg(this, _L("Enter your Google Gemini API Key:"), _L("API Key Required"));
    std::string       current_key = load_api_key();
    if (!current_key.empty()) {
        dlg.SetValue(current_key);
    }

    if (dlg.ShowModal() == wxID_OK) {
        std::string key = dlg.GetValue().ToStdString();
        save_api_key(key);
        m_gemini_client.set_api_key(key);
    }
}

std::string AIDiagnoseDialog::load_api_key()
{
    if (AppConfig* config = wxGetApp().app_config) {
        std::string stored_key = config->get("ai_gemini_api_key");
        if (!stored_key.empty())
            return stored_key;
    }

    std::string key_file = Slic3r::data_dir() + "/gemini_key";
    if (boost::filesystem::exists(key_file)) {
        std::ifstream ifs(key_file);
        std::string   key;
        std::getline(ifs, key);
        return key;
    }
    return "";
}

void AIDiagnoseDialog::save_api_key(const std::string& key)
{
    if (AppConfig* config = wxGetApp().app_config) {
        config->set("ai_gemini_api_key", key);
        config->save();
    }

    std::string   key_file = Slic3r::data_dir() + "/gemini_key";
    std::ofstream ofs(key_file);
    ofs << key;
}

std::string AIDiagnoseDialog::load_photo_base64() const
{
    if (!m_photo_picker)
        return "";

    wxString path = m_photo_picker->GetPath();
    if (path.empty())
        return "";

    wxImage image;
    if (!image.LoadFile(path))
        return "";

    wxMemoryOutputStream mos;
    image.SaveFile(mos, wxBITMAP_TYPE_JPEG);
    wxStreamBuffer* buf = mos.GetOutputStreamBuffer();
    std::string     jpg_data((char*) buf->GetBufferStart(), buf->GetBufferSize());
    return wxBase64Encode(jpg_data.data(), jpg_data.size()).ToStdString();
}

std::string AIDiagnoseDialog::capture_scene_base64() const
{
    Plater*     plater = wxGetApp().plater();
    GLCanvas3D* canvas = plater ? plater->canvas3D() : nullptr;
    if (!canvas)
        return "";

    unsigned int  w = 1024;
    unsigned int  h = 768;
    ThumbnailData thumbnail_data;

    ThumbnailsParams params{{Vec2d(w, h)}, false, false, true, false, -1, false};
    canvas->render_thumbnail(thumbnail_data, w, h, params, Camera::EType::Perspective, Camera::ViewAngleType::Iso);

    if (thumbnail_data.pixels.empty())
        return "";

    int channels = 3;
    if (thumbnail_data.pixels.size() == w * h * 4)
        channels = 4;

    unsigned char* rgb_data = (unsigned char*) malloc(w * h * 3);
    if (!rgb_data)
        return "";

    const unsigned char* src = thumbnail_data.pixels.data();
    unsigned char*       dst = rgb_data;
    for (unsigned int i = 0; i < w * h; ++i) {
        *dst++ = src[0];
        *dst++ = src[1];
        *dst++ = src[2];
        src += channels;
    }

    wxImage image(w, h, rgb_data, false);
    wxMemoryOutputStream mos;
    image.SaveFile(mos, wxBITMAP_TYPE_JPEG);
    wxStreamBuffer* buf = mos.GetOutputStreamBuffer();
    std::string     jpg_data((char*) buf->GetBufferStart(), buf->GetBufferSize());
    return wxBase64Encode(jpg_data.data(), jpg_data.size()).ToStdString();
}

}} // namespace Slic3r::GUI
