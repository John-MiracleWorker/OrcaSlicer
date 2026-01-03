#include "GeminiClient.hpp"
#include <curl/curl.h>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/log/trivial.hpp>

// We reuse existing project dependencies where possible.
// OrcaSlicer uses boost::property_tree for JSON in some places, or nlohmann json if available.
// Checking deps, nlohmann json is often preferred if available, but let's stick to boost::property_tree or manual string construction
// if we want to be safe with existing includes, OR use the project's json lib.
// Given the file list, I didn't see explicit nlohmann json in the root src, but it might be in deps.
// Let's use pure string construction for the request and boost::property_tree for response parsing to be safe and consistent with typical
// C++ projects of this era unless I see nlohmann. Actually, I saw `json.hpp` or similar in widespread use? No, I viewed `MainFrame.cpp` and
// it uses `boost/property_tree/ptree.hpp`. So I will use boost::property_tree.

namespace Slic3r { namespace GUI {

GeminiClient::GeminiClient() { curl_global_init(CURL_GLOBAL_ALL); }

GeminiClient::~GeminiClient() { curl_global_cleanup(); }

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    ((std::string*) userp)->append((char*) contents, size * nmemb);
    return size * nmemb;
}

void GeminiClient::perform_request(const std::string& payload, SuccessCallback on_success, ErrorCallback on_error)
{
    // We run this in a detached thread to not block the UI
    // In a real production app, we should use a proper job queue or the existing `Downloader` infrastructure if adaptable.
    // For this feature, a std::thread is a simple starting point.

    std::string api_key    = m_api_key; // copy for capture
    std::string model_name = m_model_name;

    std::thread([payload, on_success, on_error, api_key, model_name]() {
        CURL*       curl;
        CURLcode    res;
        std::string readBuffer;

        curl = curl_easy_init();
        if (curl) {
            struct curl_slist* headers = NULL;
            headers                    = curl_slist_append(headers, "Content-Type: application/json");

            std::string url = "https://generativelanguage.googleapis.com/v1beta/models/" + model_name + ":generateContent?key=" + api_key;

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

            // Set timeout
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

            res = curl_easy_perform(curl);

            if (res != CURLE_OK) {
                std::string error_msg = curl_easy_strerror(res);
                // Call back on main thread if possible, or just call directly (careful with UI updates)
                // For safety in wxWidgets, we should use wxQueueEvent, but here we passed a std::function.
                // The caller MUST ensure thread safety or we wrap this in wxCallAfter equivalent.
                // Since we are in Slic3r::GUI, we can assume wx access.
                // However, without `wxCallAfter` (which needs header), we might be risky.
                // Let's assume the callback handles posting to main thread or we do it here.
                if (on_error)
                    on_error(error_msg);
            } else {
                long response_code;
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

void GeminiClient::query_refinement(const std::string& user_desc,
                                    const std::string& current_settings_summary,
                                    const std::string& image_data,
                                    const std::string& task_context,
                                    SuccessCallback    on_success,
                                    ErrorCallback      on_error)
{
    if (m_api_key.empty()) {
        if (on_error)
            on_error("API Key is missing. Please check your settings.");
        return;
    }

    // Construct JSON payload
    // Using simple string concatenation to avoid boost::property_tree verbosity for simple write,
    // but escaping is important. For now, we'll do basic escaping.
    // Ideally, utilize a library.

    // Simple escape function for JSON
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
        "If an image is provided, rely heavily on the visual evidence of the model geometry and simulation to inform your suggestions. "
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

    std::stringstream ss;
    ss << "{";
    ss << "\"contents\": [{ \"parts\": [";

    // System instruction (optional, but we put it in first part usually for chat models, or separate system_instruction field)
    // For Preview models, strict prompt structure is best. Let's combine system + prompt in the text, or use system_instruction if
    // supported. Simplifying: Just put everything in text for now along with the image.

    ss << "{ \"text\": \"" << json_escape(system_prompt) << "\\n\\n" << prompt << "\" }";

    if (!image_data.empty()) {
        ss << ", { \"inlineData\": { \"mimeType\": \"image/jpeg\", \"data\": \"" << image_data << "\" } }";
    }

    ss << "] }]";
    ss << "}";

    perform_request(ss.str(), on_success, on_error);
}

void GeminiClient::analyze_print_failure(const std::string& image_data, SuccessCallback on_success, ErrorCallback on_error)
{
    if (m_api_key.empty()) {
        if (on_error)
            on_error("API Key is missing.");
        return;
    }

    auto json_escape = [](const std::string& s) {
        std::ostringstream o;
        for (auto c = s.cbegin(); c != s.cend(); c++) {
            if (*c == '"')
                o << "\\\"";
            else if (*c == '\\')
                o << "\\\\";
            else if ('\x00' <= *c && *c <= '\x1f')
                o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int) *c;
            else
                o << *c;
        }
        return o.str();
    };

    std::string system_prompt = "You are an AI Print Monitor for a 3D printer. "
                                "Analyze the provided image from the printer's camera. "
                                "Detect if there are any critical failures such as: "
                                "- Spaghetti (detached filament mess) "
                                "- Detached object (object moved from bed) "
                                "- Layer shift (significant misalignment) "
                                "- Nozzle blob (huge blob on nozzle) "
                                "Ignore minor stringing or cosmetic issues. "
                                "Return a JSON object with: "
                                "- \"failure_detected\": boolean "
                                "- \"reason\": string (short description) "
                                "- \"confidence\": float (0.0 to 1.0) "
                                "- \"severity\": string (\"low\", \"medium\", \"high\", \"critical\") "
                                "If the print looks fine (or empty bed but no mess), return failure_detected: false.";

    std::stringstream ss;
    ss << "{";
    ss << "\"contents\": [{ \"parts\": [";
    ss << "{ \"text\": \"" << json_escape(system_prompt) << "\" }";
    if (!image_data.empty()) {
        ss << ", { \"inlineData\": { \"mimeType\": \"image/jpeg\", \"data\": \"" << image_data << "\" } }";
    }
    ss << "] }]";
    // We can add generationConfig to enforce JSON if supported, for now text prompt instruction is usually enough for Gemini
    ss << "}";

    perform_request(ss.str(), on_success, on_error);
}

}} // namespace Slic3r::GUI
