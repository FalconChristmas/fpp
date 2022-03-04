#include "fpp-pch.h"

#include <curl/curl.h>

#include "MediaCommands.h"

std::unique_ptr<Command::Result> SetVolumeCommand::run(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }

    int v = std::atoi(args[0].c_str());
    setVolume(v);
    return std::make_unique<Command::Result>("Volume Set");
}
std::unique_ptr<Command::Result> AdjustVolumeCommand::run(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }

    int v = getVolume();
    v += std::atoi(args[0].c_str());
    setVolume(v);
    return std::make_unique<Command::Result>("Volume Set");
}
std::unique_ptr<Command::Result> IncreaseVolumeCommand::run(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    int v = getVolume();
    v += std::atoi(args[0].c_str());
    setVolume(v);
    return std::make_unique<Command::Result>("Volume Set");
}
std::unique_ptr<Command::Result> DecreaseVolumeCommand::run(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }

    int v = getVolume();
    v -= std::atoi(args[0].c_str());
    setVolume(v);
    return std::make_unique<Command::Result>("Volume Set");
}

static size_t URL_write_data(void* buffer, size_t size, size_t nmemb, void* userp) {
    std::string* msg = (std::string*)userp;
    msg->append(static_cast<const char*>(buffer), size * nmemb);
    return size * nmemb;
}

class CURLResult : public Command::Result {
public:
    CURLResult(const std::vector<std::string>& args) :
        Command::Result() {
        std::string url = args[0];
        LogDebug(VB_COMMAND, "URL: \"%s\"\n", url.c_str());

        std::string method = "GET";
        if (args.size() > 1) {
            method = args[1];
            if (method == "") {
                method = "GET";
            }
        }
        std::string data;
        if (args.size() > 2) {
            data = args[2];
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
            curl_easy_setopt(m_curl, CURLOPT_POSTFIELDSIZE, data.size());
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
    virtual bool isError() override { return m_curl == nullptr || m_curlm == nullptr; }
    virtual bool isDone() override {
        if (!isError()) {
            if (m_isDone) {
                return true;
            }
            int handleCount;
            CURLMcode mstatus = curl_multi_perform(m_curlm, &handleCount);
            if (mstatus != CURLM_OK) {
                cleanupCurl();
                return true;
            }

            if (handleCount == 0) {
                int messagesLeft = 0;
                CURLMsg* msg = curl_multi_info_read(m_curlm, &messagesLeft);
                if (msg && msg->msg == CURLMSG_DONE) {
                    char* ct = nullptr;
                    if (CURLE_OK == curl_easy_getinfo(m_curl, CURLINFO_CONTENT_TYPE, &ct)) {
                        if (ct != nullptr) {
                            m_contentType = ct;
                        }
                    }
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
    CURL* m_curl;
    CURLM* m_curlm;
    bool m_isDone;
};

std::unique_ptr<Command::Result> URLCommand::run(const std::vector<std::string>& args) {
    return std::make_unique<CURLResult>(args);
}


#ifdef HAS_VLC
class VLCPlayData : public VLCOutput {
public:
    VLCPlayData(const std::string& file, int l, int vol);
    virtual ~VLCPlayData();
    virtual void Stopped() override;
    std::string filename;
    int loop = 0;
    int volumeAdjust = 0;
    MediaOutputStatus status;
};
std::mutex runningMediaLock;
std::map<std::string, VLCPlayData*> runningCommandMedia;


VLCPlayData::VLCPlayData(const std::string& file, int l, int vol) :
    VLCOutput(file, &status, "--hdmi--"),
    filename(file),
    loop(l),
    volumeAdjust(vol) {
    SetVolumeAdjustment(vol);
    runningMediaLock.lock();
    runningCommandMedia[file] = this;
    runningMediaLock.unlock();
}

VLCPlayData::~VLCPlayData() {
    runningMediaLock.lock();
    if (runningCommandMedia[filename] == this) {
        runningCommandMedia.erase(filename);
        LogDebug(VB_COMMAND, "Removed cached VLCPlayData for file: \"%s\"\n", filename.c_str());
    }
    runningMediaLock.unlock();
}

void VLCPlayData::Stopped() {
    //cannot Stop/delete right now as the vlc player is locked, fork a thread
    std::thread th([this] {
        if (loop) {
            loop--;
            Restart();
        } else {
            Stop();
            delete this;
        }
    });
    th.detach();
}

std::unique_ptr<Command::Result> PlayMediaCommand::run(const std::vector<std::string>& args) {
    int loop = std::atoi(args[1].c_str());
    int volAdjust = 0;
    if (args.size() > 2) {
        volAdjust = std::atoi(args[2].c_str());
    }
    if (loop < 1) {
        loop = 1;
    }
    VLCOutput* out = new VLCPlayData(args[0], loop - 1, volAdjust);
    out->Start();

    return std::make_unique<Command::Result>("Playing");
}

std::unique_ptr<Command::Result> StopMediaCommand::run(const std::vector<std::string>& args) {
    if (args.size() == 0) {
        return std::make_unique<Command::Result>("Invalid Number of Arguments");
    }
    std::string rc = "Media Not Running";
    runningMediaLock.lock();
    VLCPlayData* media_ptr = nullptr;
    try {
        media_ptr = runningCommandMedia.at(args[0]);
    } catch (const std::out_of_range& oor) {
        LogInfo(VB_COMMAND, "Media: \"%s\" is not running so can't stop via command\n", args[0].c_str());
        media_ptr = nullptr;
    }
    if (media_ptr) {
        media_ptr->loop = 0;
        media_ptr->Stop();
        rc = "Stopped";
    }
    runningMediaLock.unlock();

    return std::make_unique<Command::Result>(rc);
}

std::unique_ptr<Command::Result> StopAllMediaCommand::run(const std::vector<std::string>& args) {
    runningMediaLock.lock();
    for (const auto& item : runningCommandMedia) {
        LogDebug(VB_COMMAND, "Stopping: \"%s\"\n", item.first.c_str());
        item.second->loop = 0;
        item.second->Stop();
    }
    runningMediaLock.unlock();
    return std::make_unique<Command::Result>("Stopped");
}
#endif
