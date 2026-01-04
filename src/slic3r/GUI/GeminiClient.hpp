#ifndef slic3r_GeminiClient_hpp_
#define slic3r_GeminiClient_hpp_

#include <string>
#include <functional>
#include <vector>
#include <memory>
#include <wx/event.h>

namespace Slic3r { namespace GUI {

class GeminiClient : public wxEvtHandler
{
public:
    GeminiClient();
    ~GeminiClient();

    // Callback types
    using SuccessCallback = std::function<void(const std::string&)>;
    using ErrorCallback   = std::function<void(const std::string&)>;

    // Main function to query the model
    // Main function to query the model
    void query_refinement(const std::string& user_desc,
                          const std::string& current_settings_summary,
                          const std::string& image_data,
                          const std::string& task_context,
                          SuccessCallback    on_success,
                          ErrorCallback      on_error);

    // Generic analysis function
    void analyze_print_failure(const std::string& image_data, SuccessCallback on_success, ErrorCallback on_error);

    // Text-only analysis (for model geometry, supports, etc.)
    void analyze_text(const std::string& prompt, SuccessCallback on_success, ErrorCallback on_error);

    // Set API Key (can be called from UI or loaded from config)
    void set_api_key(const std::string& key) { m_api_key = key; }

    // Set Model Name (e.g., gemini-2.0-flash-exp)
    void        set_model_name(const std::string& model) { m_model_name = model; }
    std::string get_model_name() const { return m_model_name; }

private:
    std::string m_api_key;
    std::string m_model_name = "gemini-2.0-flash-exp"; // Default to a flash model

    // Internal helper to perform the request
    void perform_request(const std::string& payload, SuccessCallback on_success, ErrorCallback on_error);
};

}} // namespace Slic3r::GUI

#endif // slic3r_GeminiClient_hpp_
