#ifndef slic3r_LocalModelClient_hpp_
#define slic3r_LocalModelClient_hpp_

#include <string>
#include <functional>
#include <wx/event.h>

namespace Slic3r { namespace GUI {

class LocalModelClient : public wxEvtHandler
{
public:
    LocalModelClient();
    ~LocalModelClient();

    using SuccessCallback = std::function<void(const std::string&)>;
    using ErrorCallback   = std::function<void(const std::string&)>;

    void query_refinement(const std::string& user_desc,
                          const std::string& current_settings_summary,
                          const std::string& image_data,
                          const std::string& task_context,
                          SuccessCallback    on_success,
                          ErrorCallback      on_error);

    void set_endpoint(const std::string& endpoint) { m_endpoint = endpoint; }
    void set_model(const std::string& model) { m_model = model; }
    void set_api_key(const std::string& key) { m_api_key = key; }

private:
    std::string m_endpoint;
    std::string m_model;
    std::string m_api_key;

    void perform_request(const std::string& payload, SuccessCallback on_success, ErrorCallback on_error);
};

}} // namespace Slic3r::GUI

#endif // slic3r_LocalModelClient_hpp_
