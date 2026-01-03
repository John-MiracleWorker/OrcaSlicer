#ifndef slic3r_AISupportDialog_hpp_
#define slic3r_AISupportDialog_hpp_

#include "AICore.hpp"
#include "GUI_Utils.hpp"
#include "GeminiClient.hpp"
#include "LocalModelClient.hpp"
#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/gauge.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/window.h>
#include <wx/event.h>
#include <wx/choice.h>

namespace Slic3r { namespace GUI {

class AISupportDialog : public DPIDialog
{
public:
    AISupportDialog(wxWindow* parent);
    ~AISupportDialog();

private:
    void on_analyze(wxCommandEvent& event);
    void on_apply(wxCommandEvent& event);
    void on_change_key(wxCommandEvent& event);
    void on_provider_changed(wxCommandEvent& event);
    void on_goal_changed(wxCommandEvent& event);

    void on_dpi_changed(const wxRect& suggested_rect) override;

    void        build();
    void        prompt_for_api_key();
    void        save_api_key(const std::string& key);
    std::string load_api_key();
    void        load_ai_settings();
    void        save_ai_settings();
    void        update_provider_ui();
    std::string selected_provider_key() const;
    std::string normalize_ai_value(const std::string& key, const std::string& value) const;
    void        update_diff_preview(const std::string& analysis_json);
    std::string capture_scene_base64() const;
    std::string support_goal_key() const;

    GeminiClient     m_gemini_client;
    LocalModelClient m_local_client;

    wxTextCtrl*  m_input_desc;
    wxTextCtrl*  m_result_view;
    wxTextCtrl*  m_diff_view;
    wxGauge*     m_progress;
    wxButton*    m_btn_analyze;
    wxButton*    m_btn_apply;
    wxButton*    m_btn_key;
    wxChoice*    m_provider_choice;
    wxTextCtrl*  m_local_endpoint;
    wxTextCtrl*  m_local_model;
    wxChoice*    m_goal_choice;

    std::string m_analysis_result;
};

}} // namespace Slic3r::GUI

#endif // slic3r_AISupportDialog_hpp_
