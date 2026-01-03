#include "LocalModelClient.hpp"
#include <curl/curl.h>
#include <sstream>
#include <thread>
#include <iomanip>

namespace Slic3r { namespace GUI {

LocalModelClient::LocalModelClient() { curl_global_init(CURL_GLOBAL_ALL); }

LocalModelClient::~LocalModelClient() { curl_global_cleanup(); }

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    ((std::string*) userp)->append((char*) contents, size * nmemb);
    return size * nmemb;
}

void LocalModelClient::perform_request(const std::string& payload, SuccessCallback on_success, ErrorCallback on_error)
{
    std::string endpoint = m_endpoint;
    std::string api_key  = m_api_key;

    std::thread([payload, on_success, on_error, endpoint, api_key]() {
        CURL*       curl;
        CURLcode    res;
        std::string readBuffer;

        curl = curl_easy_init();
        if (curl) {
            struct curl_slist* headers = NULL;
            headers                    = curl_slist_append(headers, "Content-Type: application/json");

            if (!api_key.empty()) {
                std::string auth = "Authorization: Bearer " + api_key;
                headers          = curl_slist_append(headers, auth.c_str());
            }

            curl_easy_setopt(curl, CURLOPT_URL, endpoint.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

            res = curl_easy_perform(curl);

            if (res != CURLE_OK) {
                std::string error_msg = curl_easy_strerror(res);
                if (on_error)
                    on_error(error_msg);
            } else {
                long response_code = 0;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
                if (response_code >= 200 && response_code < 300) {
                    if (on_success)
                        on_success(readBuffer);
                } else {
                    if (on_error)
                        on_error("API Error: HTTP " + std::to_string(response_code) + "\n" + readBuffer);
                }
            }

            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        } else {
            if (on_error)
                on_error("Failed to initialize CURL");
        }
    }).detach();
}

void LocalModelClient::query_refinement(const std::string& user_desc,
                                        const std::string& current_settings_summary,
                                        const std::string& image_data,
                                        const std::string& task_context,
                                        SuccessCallback    on_success,
                                        ErrorCallback      on_error)
{
    if (m_endpoint.empty()) {
        if (on_error)
            on_error("Local model endpoint is missing. Please set it in the AI settings.");
        return;
    }
    if (m_model.empty()) {
        if (on_error)
            on_error("Local model name is missing. Please set it in the AI settings.");
        return;
    }

    auto json_escape = [](const std::string& s) {
        std::ostringstream o;
        for (auto c = s.cbegin(); c != s.cend(); c++) {
            if (*c == '"')
                o << "\\\"";
            else if (*c == '\\')
                o << "\\\\";
            else if ('\x00' <= *c && *c <= '\x1f') {
                o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int) *c;
            } else {
                o << *c;
            }
        }
        return o.str();
    };

    std::string system_prompt =
        "You are an expert 3D printing assistant for OrcaSlicer. "
        "Analyze the user's description (if provided), the printer context (model, nozzle size, gcode flavor, bed type, filament type), "
        "and the complete list of current print settings, including advanced options. "
        "Review every setting and suggest optimizations where appropriate for quality, reliability, and reasonable speed. "
        "You must include multiple advanced-setting changes in the 'changes' list (at least 3) when it is safe to do so. "
        "Never use grid infill: do not set sparse_infill_pattern to 'grid'. "
        "Provide your response as JSON only, with a 'changes' list (key, value, reason) and a 'summary' string. "
        "Only use keys present in the provided settings list. "
        "Do NOT hallucinate setting names or omit advanced options from consideration.";

    std::string prompt = "User Description: " + json_escape(user_desc) + "\\n\\n";
    if (!task_context.empty())
        prompt += "Task Context: " + json_escape(task_context) + "\\n\\n";
    prompt += "Current Print Settings (complete list): " + json_escape(current_settings_summary);

    if (!image_data.empty()) {
        prompt += "\\n\\nImage data was captured but is not attached for local models.";
    }

    std::stringstream ss;
    ss << "{";
    ss << "\"model\": \"" << json_escape(m_model) << "\",";
    ss << "\"messages\": [";
    ss << "{ \"role\": \"system\", \"content\": \"" << json_escape(system_prompt) << "\" },";
    ss << "{ \"role\": \"user\", \"content\": \"" << prompt << "\" }";
    ss << "],";
    ss << "\"temperature\": 0.2";
    ss << "}";

    perform_request(ss.str(), on_success, on_error);
}

}} // namespace Slic3r::GUI
