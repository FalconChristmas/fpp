#include <thread>

#include "MediaCommands.h"
#include "mediaoutput/mediaoutput.h"


std::unique_ptr<Command::Result> SetVolumeCommand::run(const std::vector<std::string> &args) {
    if (args.size() != 1) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }

    int v = std::atoi(args[0].c_str());
    setVolume(v);
    return std::make_unique<Command::Result>("Volume Set");
}
std::unique_ptr<Command::Result> AdjustVolumeCommand::run(const std::vector<std::string> &args) {
    if (args.size() != 1) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }

    int v = getVolume();
    v += std::atoi(args[0].c_str());
    setVolume(v);
    return std::make_unique<Command::Result>("Volume Set");
}
std::unique_ptr<Command::Result> IncreaseVolumeCommand::run(const std::vector<std::string> &args) {
    if (args.size() != 1) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    int v = getVolume();
    v += std::atoi(args[0].c_str());
    setVolume(v);
    return std::make_unique<Command::Result>("Volume Set");
}
std::unique_ptr<Command::Result> DecreaseVolumeCommand::run(const std::vector<std::string> &args) {
    if (args.size() != 1) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }

    int v = getVolume();
    v -= std::atoi(args[0].c_str());
    setVolume(v);
    return std::make_unique<Command::Result>("Volume Set");
}

#include <curl/curl.h>

static size_t URL_write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
    std::string *msg = (std::string*)userp;
    msg->append(static_cast<const char*>(buffer), size * nmemb);
    return size * nmemb;
}

class CURLResult : public Command::Result {
public:
    CURLResult(const std::vector<std::string> &args) : Command::Result() {
        std::string url = args[0];
        std::string method = "GET";
        if (args.size() > 1) {
            method = args[1];
        }
        std::string data;
        if (args.size() > 2) {
            data =args[2];
        }
        m_curl = curl_easy_init();
        
        
        CURLcode status;
        status = curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
        if (status != CURLE_OK) {
            cleanupCurl();
            return;
        }
        status = curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, URL_write_data);
        if (status != CURLE_OK) {
            cleanupCurl();
            return;
        }

        status = curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &m_result);
        if (status != CURLE_OK) {
            cleanupCurl();
            return;
        }

        if (method == "POST") {
            curl_easy_setopt(m_curl, CURLOPT_POSTFIELDSIZE, 4096);
            status = curl_easy_setopt(m_curl, CURLOPT_COPYPOSTFIELDS, data.c_str());
            if (status != CURLE_OK) {
                cleanupCurl();
                return;
            }
        }
        
        m_curlm = curl_multi_init();
        CURLMcode mstatus = curl_multi_add_handle(m_curlm, m_curl);
        if (mstatus != CURLM_OK) {
            cleanupCurl();
            return;
        }
        m_isDone = false;
    }
    virtual ~CURLResult() {
        int count = 0;
        while (!isDone() && count < 1000) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            count++;
        }
        cleanupCurl();
    }
    virtual bool isError() { return m_curl == nullptr || m_curlm == nullptr; }
    virtual bool isDone() {
        if (!isError()) {
            int handleCount;
            CURLMcode mstatus = curl_multi_perform(m_curlm, &handleCount);
            if (mstatus != CURLM_OK) {
                cleanupCurl();
                return true;
            }
            
            if (handleCount == 0) {
                int messagesLeft = 0;
                CURLMsg *msg = curl_multi_info_read(m_curlm, &messagesLeft);
                if (msg->msg == CURLMSG_DONE) {
                    m_isDone = true;
                }
            }
            return m_isDone;
        }
        return true;
    }
    
    void cleanupCurl() {
        if (m_curlm) {
            if (m_curl) {
                curl_multi_remove_handle(m_curlm, m_curl);
            }
            curl_multi_cleanup(m_curlm);
            m_curlm = nullptr;
        }
        if (m_curl) {
            curl_easy_cleanup(m_curl);
            m_curl = nullptr;
        }
    }
private:
    CURL *m_curl;
    CURLM *m_curlm;
    bool m_isDone;
};

std::unique_ptr<Command::Result> URLCommand::run(const std::vector<std::string> &args) {
    return std::make_unique<CURLResult>(args);
}
