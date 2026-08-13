/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "fpp-pch.h"

#include "fpp-json.h"
#include "fpphttp.h" // drogon/HTTP helpers used here; no longer pulled transitively (see fpphttp_types.h)
#include "fpphttp_clientip.h" // getEffectiveClientIP() - FPP-internal, not in the plugin API

#include <sys/stat.h>
#include <dirent.h>

#include <algorithm> // std::sort/std::transform in the image file listing
#include <fcntl.h>
#include <filesystem> // image file listing for the Image effect's picker
#include <fstream> // virtualdisplaymap parse for model preview endpoint
#include <unistd.h> // write -- needed directly for NOPCH builds

#include <Magick++.h>

#include <magick/type.h>

#include "../channeloutput/channeloutputthread.h"
#include "../common.h"
#include "../effects.h"
#include "../log.h"
#include "../settings.h"

#include "PixelOverlay.h"
#include "PixelOverlayEffects.h"
#include "PixelOverlayModel.h"
#include "PixelOverlayModelFB.h"
#include "PixelOverlayModelSub.h"

PixelOverlayManager PixelOverlayManager::INSTANCE;

class OverlayRange {
public:
    OverlayRange(int s, int e, int v) :
        start(s),
        end(e),
        value(v) {}

    int start = 0;
    int end = 0;
    int value = 0;
};

class PixelOverlayModelHolder {
public:
    PixelOverlayModel* model;
    std::map<std::string, std::function<void(PixelOverlayModel*)>> listeners;

    void toJson(Json::Value& json) const {
        if (model) {
            model->toJson(json);
        }
    }
};

uint32_t PixelOverlayManager::mapColor(const std::string& c) {
    if (c.empty()) {
        return 0x000000;
    }
    if (c[0] == '#') {
        std::string color = "0x" + c.substr(1);
        return std::stoul(color, nullptr, 0);
    } else if (c == "red") {
        return 0xFF0000;
    } else if (c == "green") {
        return 0x00FF00;
    } else if (c == "blue") {
        return 0x0000FF;
    } else if (c == "yellow") {
        return 0xFFFF00;
    } else if (c == "black") {
        return 0x000000;
    } else if (c == "white") {
        return 0xFFFFFF;
    } else if (c == "cyan") {
        return 0x00FFFF;
    } else if (c == "magenta") {
        return 0xFF00FF;
    } else if (c == "gray") {
        return 0x808080;
    } else if (c == "grey") {
        return 0x808080;
    }
    return std::stoul(c, nullptr, 0);
}
void PixelOverlayManager::PixelOverlayModelHolder::toJson(Json::Value& v) const {
    if (model) {
        model->toJson(v);
    }
}
PixelOverlayManager::PixelOverlayManager() :
    numActive(0) {
}
PixelOverlayManager::~PixelOverlayManager() {
    if (updateThread != nullptr) {
        std::unique_lock<std::mutex> l(threadLock);
        threadKeepRunning = false;
        l.unlock();
        threadCV.notify_all();
        updateThread->join();
        delete updateThread;
        updateThread = nullptr;
    }
    for (auto a : models) {
        if (a.second.model) {
            delete a.second.model;
        }
    }
    models.clear();
    modelNames.clear();
}
void PixelOverlayManager::Initialize() {
    loadModelMap();
    loadFonts();
}

void PixelOverlayManager::addModel(Json::Value config) {
    PixelOverlayModel* pmodel = nullptr;

    if (!config.isMember("Type"))
        config["Type"] = "Channel";

    std::string type = config["Type"].asString();

    if (type == "Channel") {
        if (config.isMember("StartChannel") && config.isMember("ChannelCount") && config.isMember("StringCount")) {
            pmodel = new PixelOverlayModel(config);
        } else {
            std::string name;
            if (config.isMember("Name")) {
                name = config["Name"].asString();
            }
            LogWarn(VB_CHANNELOUT, "PixelOverlayManager::loadModelMap() - invalid configuration for model %s\n", name.c_str());
        }
    } else if (type == "FB") {
        pmodel = new PixelOverlayModelFB(config);
    } else if (type == "Sub") {
        pmodel = new PixelOverlayModelSub(config);
    }

    std::unique_lock<std::recursive_mutex> lock(modelsLock);
    bool wasEmpty = models.empty();
    if (pmodel) {
        models[pmodel->getName()].model = pmodel;
        modelNames.push_back(pmodel->getName());
        for (auto& l : models[pmodel->getName()].listeners) {
            l.second(pmodel);
        }
    }
    if (wasEmpty) {
        RegisterCommands();
    }
}

void PixelOverlayManager::loadModelMap() {
    LogDebug(VB_CHANNELOUT, "PixelOverlayManager::loadModelMap()\n");
    if (updateThread != nullptr) {
        std::unique_lock<std::mutex> l(threadLock);
        threadKeepRunning = false;
        updates.clear();
        l.unlock();
        threadCV.notify_all();
        updateThread->join();
        delete updateThread;
        updateThread = nullptr;
    }
    for (auto a : models) {
        for (auto& l : a.second.listeners) {
            l.second(a.second.model);
        }
        delete a.second.model;
    }
    models.clear();
    modelNames.clear();
    // Drop the lazy submodel index so a re-uploaded xlights-submodels.json is
    // picked up on the next reference.
    subModelConfigs.clear();
    subModelIndexLoaded = false;
    ConvertCMMFileToJSON();

    std::string filename(FPP_DIR_CONFIG("/model-overlays.json"));
    if (FileExists(filename)) {
        Json::Value root;
        bool result = LoadJsonFromFile(filename, root, JsonRoot::Object);
        if (!result) {
            LogErr(VB_CHANNELOUT, "Error parsing model-overlays.json.");
            return;
        }

        if (root.isMember("autoCreate")) {
            autoCreate = root["autoCreate"].asBool();
        }
        Json::Value models = root["models"];
        for (int c = 0; c < models.size(); c++) {
            addModel(models[c]);
        }
    }
}
void PixelOverlayManager::ConvertCMMFileToJSON() {
    FILE* fp;
    char buf[64];
    char* s;
    int startChannel;
    int channelCount;
    std::string filename(FPP_DIR_MEDIA("/channelmemorymaps"));

    if (!FileExists(filename)) {
        return;
    }
    Json::Value result;

    LogDebug(VB_CHANNELOUT, "Loading Channel Memory Map data.\n");
    fp = fopen(filename.c_str(), "r");
    if (fp == NULL) {
        LogErr(VB_CHANNELOUT, "Could not open Channel Memory Map config file %s\n", filename.c_str());
        return;
    }

    while (fgets(buf, 64, fp) != NULL) {
        if (buf[0] == '#') {
            // Allow # comments for testing
            continue;
        }
        Json::Value model;
        char* saveptr = nullptr; // for thread-safe strtok_r below

        // Name
        s = strtok_r(buf, ",", &saveptr);
        if (s) {
            std::string modelName = s;
            model["Name"] = modelName;
        } else {
            continue;
        }

        // Start Channel
        s = strtok_r(NULL, ",", &saveptr);
        if (!s)
            continue;
        startChannel = strtol(s, NULL, 10);
        if (startChannel <= 0) {
            continue;
        }
        model["StartChannel"] = startChannel;

        // Channel Count
        s = strtok_r(NULL, ",", &saveptr);
        if (!s)
            continue;
        channelCount = strtol(s, NULL, 10);
        if (channelCount <= 0) {
            continue;
        }
        model["ChannelCount"] = channelCount;

        // Orientation
        s = strtok_r(NULL, ",", &saveptr);
        if (!s)
            continue;
        model["Orientation"] = !strcmp(s, "vertical") ? "vertical" : "horizontal";

        // Start Corner
        s = strtok_r(NULL, ",", &saveptr);
        if (!s)
            continue;
        model["StartCorner"] = s;

        // String Count
        s = strtok_r(NULL, ",", &saveptr);
        if (!s)
            continue;
        model["StringCount"] = (int)strtol(s, NULL, 10);

        // Strands Per String
        s = strtok_r(NULL, ",", &saveptr);
        if (!s)
            continue;
        model["StrandsPerString"] = (int)strtol(s, NULL, 10);
        result["models"].append(model);
    }
    fclose(fp);
    remove(filename.c_str());

    filename = FPP_DIR_CONFIG("/model-overlays.json");
    SaveJsonToFile(result, filename);
}

bool PixelOverlayManager::hasActiveOverlays() {
    return numActive > 0;
}
void PixelOverlayManager::resetChildParent(const std::string& name) {
    std::unique_lock<std::recursive_mutex> lock(modelsLock);
    PixelOverlayModel* model = getModelLocked(name);
    if (model) {
        PixelOverlayModelSub* subModel = dynamic_cast<PixelOverlayModelSub*>(model);
        if (subModel) {
            subModel->resetParent();
        }
    }
}

void PixelOverlayManager::modelStateChanged(PixelOverlayModel* m, const PixelOverlayState& old, const PixelOverlayState& state) {
    if (old.getState() == 0) {
        // enabling, add
        std::unique_lock<std::recursive_mutex> lock(activeModelsLock);
        activeModels.push_back(m);
        numActive++;
    } else if (state.getState() == 0) {
        // disabling, remove
        std::unique_lock<std::recursive_mutex> lock(activeModelsLock);
        activeModels.remove(m);
        numActive--;
    }
    if (numActive > 0) {
        StartChannelOutputThread();
    }
}

// Runs on the channel output thread, once per frame.
//
// LOCK ORDER: nothing reached from here may take modelsLock. This function
// holds activeModelsLock for the whole pass, while every HTTP handler takes
// modelsLock first and then activeModelsLock (via setState() ->
// modelStateChanged()). Acquiring them in this order deadlocks the two threads
// against each other, and the symptom is nasty: fppd keeps running and
// /api/fppd/status keeps answering 200, but every /api/overlays and /api/models
// request hangs forever and overlays stop updating, so a health check will not
// notice. PixelOverlayModelSub::doOverlay() used to resolve its parent through
// getModel() from here; see the note on PixelOverlayModelSub::foundParent().
//
// removeAutoOverlayModel() and getActiveOverlayEffects() both drop modelsLock
// (or snapshot) before touching activeModelsLock for the same reason.
void PixelOverlayManager::doOverlays(uint8_t* channels) {
    if (numActive == 0) {
        return;
    }
    std::unique_lock<std::recursive_mutex> lock(activeModelsLock);
    // First, flush any buffers
    for (auto m : activeModels) {
        if (m->overlayBufferIsDirty()) {
            m->flushOverlayBuffer();
        }
    }
    // Second, do any sub-models
    for (auto m : activeModels) {
        if (m->getType() == "Sub") {
            m->doOverlay(channels);
        }
    }
    // Then do any non-subs
    for (auto m : activeModels) {
        if (m->getType() != "Sub") {
            m->doOverlay(channels);
        }
    }

    for (auto& m : activeRanges) {
        for (int s = m.start; s <= m.end; s++) {
            channels[s] = m.value;
        }
    }
    lock.unlock();
    std::unique_lock<std::mutex> l(threadLock);
    while (!afterOverlayModels.empty()) {
        PixelOverlayModel* m = afterOverlayModels.front();
        afterOverlayModels.pop_front();
        l.unlock();
        m->updateRunningEffects();
        l.lock();
    }
}

PixelOverlayModel* PixelOverlayManager::getModel(const std::string& name) {
    std::unique_lock<std::recursive_mutex> lock(modelsLock);
    return getModelLocked(name);
}
PixelOverlayModel* PixelOverlayManager::getModelLocked(const std::string& name) {
    auto a = models.find(name);
    if (a != models.end()) {
        return a->second.model;
    }
    // Not a loaded model -- it may be a lazily-loaded xLights submodel.
    return materializeSubModelLocked(name);
}

// Parse config/xlights-submodels.json once into a lightweight name -> compact
// config index.  Never called at boot; only on the first submodel reference or
// discovery request, so systems that never use submodels pay nothing.
void PixelOverlayManager::loadSubModelIndex() {
    if (subModelIndexLoaded) {
        return;
    }
    subModelIndexLoaded = true; // set regardless so a missing file isn't re-stat'd repeatedly

    std::string filename(FPP_DIR_CONFIG("/xlights-submodels.json"));
    if (!FileExists(filename)) {
        return;
    }
    Json::Value root;
    if (!LoadJsonFromFile(filename, root, JsonRoot::Object)) {
        LogErr(VB_CHANNELOUT, "Error parsing xlights-submodels.json.\n");
        return;
    }
    Json::Value subs = root["submodels"];
    for (int c = 0; c < subs.size(); c++) {
        if (subs[c].isMember("Name")) {
            std::string nm = subs[c]["Name"].asString();
            replaceAll(nm, "/", "_"); // match PixelOverlayModel name sanitization
            subModelConfigs[nm] = SaveJsonToString(subs[c], "");
        }
    }

    // Model groups share the same lazy index and the same "Sub" materialization
    // path (they are channelgrid Subs spanning multiple members).
    std::string groupFile(FPP_DIR_CONFIG("/xlights-modelgroups.json"));
    if (FileExists(groupFile)) {
        Json::Value groot;
        if (LoadJsonFromFile(groupFile, groot, JsonRoot::Object)) {
            Json::Value groups = groot["modelgroups"];
            for (int c = 0; c < groups.size(); c++) {
                if (groups[c].isMember("Name")) {
                    std::string nm = groups[c]["Name"].asString();
                    replaceAll(nm, "/", "_");
                    subModelConfigs[nm] = SaveJsonToString(groups[c], "");
                }
            }
        } else {
            LogErr(VB_CHANNELOUT, "Error parsing xlights-modelgroups.json.\n");
        }
    }
    LogDebug(VB_CHANNELOUT, "Loaded xLights submodel/group index: %d entries\n", (int)subModelConfigs.size());
}

// Materialize a single submodel (build the live PixelOverlayModelSub) the first
// time it is referenced.  Returns nullptr if the name is not a known submodel.
PixelOverlayModel* PixelOverlayManager::materializeSubModelLocked(const std::string& name) {
    if (!subModelIndexLoaded) {
        loadSubModelIndex();
    }
    auto it = subModelConfigs.find(name);
    if (it == subModelConfigs.end()) {
        return nullptr;
    }
    Json::Value config;
    if (!LoadJsonFromString(it->second, config)) {
        LogErr(VB_CHANNELOUT, "Error parsing cached submodel config for %s\n", name.c_str());
        return nullptr;
    }
    addModel(config);
    auto a = models.find(name);
    return (a != models.end()) ? a->second.model : nullptr;
}
void PixelOverlayManager::addModelListener(const std::string& name, const std::string& id, std::function<void(PixelOverlayModel*)> listener) {
    std::unique_lock<std::recursive_mutex> lock(modelsLock);
    models[name].listeners[id] = listener;
}
void PixelOverlayManager::removeModelListener(const std::string& name, const std::string& id) {
    std::unique_lock<std::recursive_mutex> lock(modelsLock);
    auto a = models.find(name);
    if (a != models.end()) {
        a->second.listeners.erase(id);
    }
}

static bool isTTF(const std::string& mainStr) {
    return (mainStr.size() >= 4) && (mainStr.compare(mainStr.size() - 4, 4, ".ttf") == 0);
}
static bool isPFB(const std::string& mainStr) {
    return (mainStr.size() >= 4) && (mainStr.compare(mainStr.size() - 4, 4, ".pfb") == 0);
}
static void findFonts(const std::string& dir, std::map<std::string, std::string>& fonts) {
    DIR* dp;
    struct dirent* ep;

    dp = opendir(dir.c_str());
    if (dp != NULL) {
        while ((ep = readdir(dp))) {
            char* dot = strstr(ep->d_name, ".");
            // No dot means no extension — skip
            if (!dot) {
                continue;
            }
            int location = dot - ep->d_name;

            // We're one of ".", "..", or hidden, so let's skip
            if (location == 0) {
                continue;
            }

            struct stat statbuf;
            std::string dname = dir;
            dname += ep->d_name;
            lstat(dname.c_str(), &statbuf);
            if (S_ISLNK(statbuf.st_mode)) {
                // symlink, skip
                continue;
            } else if (S_ISDIR(statbuf.st_mode)) {
                findFonts(dname + "/", fonts);
            } else if (isTTF(ep->d_name) || isPFB(ep->d_name)) {
                std::string fname = ep->d_name;
                fname.resize(fname.size() - 4);
                fonts[fname] = dname;
            }
        }
        closedir(dp);
    }
}

void PixelOverlayManager::loadFonts() {
    if (!fontsLoaded) {
#ifndef PLATFORM_OSX
        findFonts("/usr/share/fonts/truetype/", fonts);
        findFonts("/usr/share/fonts/X11/Type1/", fonts);
        findFonts("/usr/local/share/fonts/", fonts);
#else
        findFonts("/System/Library/fonts/", fonts);
#endif
        fontsLoaded = true;
    }
}

const std::string& PixelOverlayManager::mapFont(const std::string& f) {
    static std::string EMPTY_STR;
    if (fonts[f] != "") {
        return fonts[f];
    }
    return EMPTY_STR;
}

Json::Value PixelOverlayManager::getModelsAsJson() {
    Json::Value ret;
    std::unique_lock<std::recursive_mutex> lock(modelsLock);
    for (auto& mn : modelNames) {
        Json::Value model;
        models[mn].toJson(model);
        ret.append(model);
    }
    return ret;
}
HttpResponsePtr PixelOverlayManager::render_HEAD(const HttpRequestPtr& req) {
    return render_GET(req);
}

// --------------------------------------------------------------------------
// OpenAPI docs for the /models and /overlays/* endpoints handled below.
// --------------------------------------------------------------------------

/**
 * List all configured pixel-overlay models.
 *
 * @route GET /api/models
 * @param boolean simple Return just the model names instead of full definitions.
 * @param boolean all Prepend the "--All Models--" entry to the result.
 * @response 200 Array of models (or model names when `simple=true`).
 */

/**
 * Get a single pixel-overlay model definition by name.
 *
 * @route GET /api/models/{model}
 * @response 200 The model definition.
 * @response 404 No model with that name exists.
 */

/**
 * List the fonts available for overlay text effects.
 *
 * @route GET /api/overlays/fonts
 * @response 200 Array of font names.
 */

/**
 * Get the pixel-overlay manager settings.
 *
 * @route GET /api/overlays/settings
 * @response 200 Object with the `autoCreate` flag.
 */

/**
 * List all overlay models with their current runtime state (active state,
 * running effect, dimensions).
 *
 * @route GET /api/overlays/models
 * @response 200 Array of models with runtime state.
 */

/**
 * Get a single overlay model with its current runtime state.
 *
 * @route GET /api/overlays/model/{model}
 * @response 200 Model definition plus runtime state.
 */

/**
 * Get the current pixel buffer of an overlay model. Append /rle for
 * run-length-encoded data, or /raw for the binary form.
 *
 * @route GET /api/overlays/model/{model}/data
 * @response 200 Object with a `data` array (and `rle` flag).
 */

/**
 * Get the current pixel buffer of an overlay model as raw binary:
 * width * height * bytesPerPixel bytes, row-major from the top-left, which is
 * the exact layout `PUT /api/overlays/model/{model}/data` accepts. The model
 * geometry is repeated in the `X-FPP-Model-Width`, `X-FPP-Model-Height` and
 * `X-FPP-Model-BytesPerPixel` response headers.
 *
 * The JSON form spends 4-6 wire bytes per payload byte; this one spends one.
 *
 * @route GET /api/overlays/model/{model}/data/raw
 * @response 200 Binary pixel data (application/octet-stream).
 */

/**
 * Get the per-pixel virtual-display coordinates for a model, for a lightweight
 * layout preview. Sourced from config/virtualdisplaymap (the xLights-exported
 * layout) and returned on demand so the UI never has to inline this data (which
 * can be hundreds of thousands of points per model) for every model at once.
 *
 * @route GET /api/overlays/model/{model}/preview
 * @response 200 Object with a `pixels` array of [x, y, channel] triples.
 */

/**
 * Clear (blank) an overlay model's pixel buffer.
 *
 * @route GET /api/overlays/model/{model}/clear
 * @response 200 Model cleared.
 */

/**
 * List the available overlay effects. Add ?full=true for full descriptions.
 *
 * @route GET /api/overlays/effects
 * @param boolean full Return full effect descriptions instead of just names.
 * @response 200 Array of effect names (or descriptions when `full=true`).
 */

/**
 * Get the description of a single overlay effect.
 *
 * @route GET /api/overlays/effects/{effect}
 * @response 200 The effect description.
 */

/**
 * List the overlay effects that are currently running.
 *
 * @route GET /api/overlays/running
 * @response 200 Active overlay effects.
 */
// Normalize an overlay model name for matching (mirrors the UI's
// normalizeModelName()): lowercase, strip everything but [a-z0-9]. Overlay
// model names use underscores (e.g. "Arch_1_-_Bottom") while the
// virtualdisplaymap uses the original xLights names (e.g. "Arch 1 - Bottom"),
// so preview lookups must compare on the normalized form.
static std::string normalizeOverlayModelName(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        if (c >= 'A' && c <= 'Z') {
            c = c - 'A' + 'a';
        }
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            out.push_back(c);
        }
    }
    return out;
}

// Collect the names of usable image files under the media images directory,
// relative to it, sorted, for the Image overlay effect's file picker.
static void collectOverlayImageNames(Json::Value& result) {
    static const char* const IMAGE_EXTENSIONS[] = {
        ".bmp", ".gif", ".jpeg", ".jpg", ".png", ".ppm", ".tga", ".tif", ".tiff", ".webp"
    };

    std::string dir = FPP_DIR_IMAGE("");
    std::vector<std::string> names;

    try {
        if (!std::filesystem::exists(dir)) {
            result.resize(0);
            return;
        }
        for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            bool isImage = false;
            for (const char* known : IMAGE_EXTENSIONS) {
                if (ext == known) {
                    isImage = true;
                    break;
                }
            }
            if (!isImage) {
                continue;
            }
            names.push_back(std::filesystem::relative(entry.path(), dir).string());
        }
    } catch (const std::filesystem::filesystem_error& e) {
        LogErr(VB_CHANNELOUT, "Error listing overlay images: %s\n", e.what());
    }

    std::sort(names.begin(), names.end());
    for (auto& n : names) {
        result.append(n);
    }
    if (names.empty()) {
        result.resize(0); // an empty Json::Value would serialize as null, not []
    }
}

// Collect the per-pixel [x, y, channel] preview coordinates for one model from
// config/virtualdisplaymap, appending them to pixels. Matches the model section
// on exact or normalized name. Parsing on demand (only when a preview is
// requested) keeps this potentially very large data out of the page.
static void collectModelPreviewPixels(const std::string& modelName, Json::Value& pixels) {
    const std::string want = modelName;
    const std::string wantNorm = normalizeOverlayModelName(modelName);
    std::ifstream in(FPP_DIR_CONFIG("/virtualdisplaymap"));
    if (!in.is_open()) {
        return;
    }
    std::string line;
    bool inModel = false;
    while (std::getline(in, line)) {
        if (!line.empty() && line[0] == '#') {
            // Model header line looks like: # Model: 'Some Name'
            if (line.find("Model:") != std::string::npos) {
                std::size_t q1 = line.find('\'');
                std::size_t q2 = (q1 == std::string::npos) ? std::string::npos : line.find('\'', q1 + 1);
                if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
                    std::string name = line.substr(q1 + 1, q2 - q1 - 1);
                    inModel = (name == want) || (normalizeOverlayModelName(name) == wantNorm);
                }
            }
            continue;
        }
        if (!inModel || line.empty()) {
            continue;
        }
        int x = 0, y = 0, z = 0;
        if (sscanf(line.c_str(), "%d,%d,%d", &x, &y, &z) >= 3) {
            Json::Value px(Json::arrayValue);
            px.append(x);
            px.append(y);
            px.append(z);
            pixels.append(px);
        }
    }
}

// ---- Bulk pixel data ------------------------------------------------------
//
// Backing code for PUT /api/overlays/model/{model}/data. The per-pixel endpoint
// costs one HTTP round trip per pixel, and the only bulk alternative was the
// mmapped shm buffer -- which is local-only and awkward from anything that is
// not C. This moves a whole frame, or a rectangle of one, per request.

// Query args are parsed with "absent" and "malformed" kept apart so a typo
// reports as an error instead of silently taking the default.
enum class ArgState {
    Missing,
    Bad,
    Ok
};
static ArgState getIntArg(const HttpRequestPtr& req, const char* name, int& out) {
    std::string v = getRequestArg(req, name);
    if (v.empty()) {
        return ArgState::Missing;
    }
    try {
        std::size_t pos = 0;
        int i = std::stoi(v, &pos, 10);
        if (pos != v.size()) {
            return ArgState::Bad;
        }
        out = i;
        return ArgState::Ok;
    } catch (const std::exception&) {
        return ArgState::Bad;
    }
}

// Wire layouts accepted for a raw pixel body. 0 means "not a raw format".
static int bytesForPixelFormat(const std::string& fmt) {
    if (fmt.empty() || fmt == "rgb") {
        return 3;
    }
    if (fmt == "rgbw") {
        return 4;
    }
    if (fmt == "mono" || fmt == "grey" || fmt == "gray") {
        return 1;
    }
    return 0;
}

// ?blend= selects how this one write combines with what is already in the
// model, reusing the overlay state blend rules. Deliberately NOT named "state":
// PUT .../state sets the model's *enable* state, and one arg name meaning both
// things would be a trap.
static bool parseBlend(const std::string& v, PixelOverlayState& out) {
    if (v.empty() || v == "opaque" || v == "Enabled") {
        out = PixelOverlayState(PixelOverlayState::Enabled);
    } else if (v == "transparent" || v == "Transparent") {
        out = PixelOverlayState(PixelOverlayState::Transparent);
    } else if (v == "transparentrgb" || v == "TransparentRGB") {
        out = PixelOverlayState(PixelOverlayState::TransparentRGB);
    } else {
        return false;
    }
    return true;
}

// ?scale= for encoded-image bodies, onto ScaleOverlayImage()'s vocabulary.
static bool parseImageScaling(const std::string& v, std::string& out) {
    if (v.empty() || v == "fit") {
        out = "Scale to Fit";
    } else if (v == "fill" || v == "crop") {
        out = "Crop to Fill";
    } else if (v == "stretch") {
        out = "Stretch";
    } else if (v == "none") {
        out = "None";
    } else {
        return false;
    }
    return true;
}

// Does this body look like an encoded image?
//
// Only ever consulted AFTER the body has failed to match the raw pixel geometry
// the request describes. Raw pixel data can legitimately begin with any byte
// sequence at all -- 0xFF 0xD8 0xFF is a perfectly ordinary red-ish pixel, not
// necessarily a JPEG -- so sniffing before the length check would misread real
// pixel data as an image. Length first, magic second, is unambiguous.
static bool looksLikeEncodedImage(std::string_view b) {
    auto starts = [&](const char* magic, std::size_t n) {
        return b.size() >= n && memcmp(b.data(), magic, n) == 0;
    };
    return starts("\x89PNG\r\n\x1a\n", 8) ||                     // PNG
           starts("\xff\xd8\xff", 3) ||                          // JPEG
           starts("GIF8", 4) ||                                  // GIF
           starts("BM", 2) ||                                    // BMP
           starts("II*\0", 4) ||                                 // TIFF little-endian
           starts("MM\0*", 4) ||                                 // TIFF big-endian
           (b.size() >= 12 && memcmp(b.data(), "RIFF", 4) == 0 && // WebP
            memcmp(b.data() + 8, "WEBP", 4) == 0);
}

// `lock` is render_PUT()'s hold on modelsLock. It is released before the
// expensive part of the request (image decode, the blit itself) for two
// reasons, and the caller must not use it afterwards:
//
//  * The Text branch below already unlocks before dispatching, because
//    touching a model's running effect under modelsLock inverts the order the
//    effect thread uses (effectLock, then modelsLock via a submodel's parent
//    lookup). Evicting an effect here has to follow the same rule.
//  * Holding modelsLock across a GraphicsMagick decode -- milliseconds -- while
//    this endpoint is designed to be called at frame rates widens an existing
//    window in which the channel output thread sits blocked in doOverlays()
//    holding activeModelsLock and waiting on modelsLock. See
//    docs/PixelOverlayBulkData.md.
static HttpResponsePtr bulkPixelDataPut(const HttpRequestPtr& req, PixelOverlayModel* m,
                                        std::unique_lock<std::recursive_mutex>& lock) {
    auto errorResp = [](const std::string& msg) {
        Json::Value r;
        r["Status"] = "ERROR";
        r["Message"] = msg;
        return makeStringResponse(SaveJsonToString(r, ""), 400, "application/json");
    };

    const int modelW = m->getWidth();
    const int modelH = m->getHeight();

    int x = 0, y = 0, w = 0, h = 0;
    bool xExplicit = false, yExplicit = false, wExplicit = false, hExplicit = false;
    struct {
        const char* name;
        int* val;
        bool* explicitly;
    } intArgs[] = {
        { "x", &x, &xExplicit },
        { "y", &y, &yExplicit },
        { "w", &w, &wExplicit },
        { "h", &h, &hExplicit },
    };
    for (auto& a : intArgs) {
        ArgState st = getIntArg(req, a.name, *a.val);
        if (st == ArgState::Bad) {
            return errorResp(std::string("Argument \"") + a.name + "\" is not an integer");
        }
        *a.explicitly = (st == ArgState::Ok);
    }

    std::string fmtArg = getRequestArg(req, "fmt");
    std::string scaleArg = getRequestArg(req, "scale");
    std::string targetArg = getRequestArg(req, "target");
    std::string autoEnableArg = getRequestArg(req, "autoEnable");
    std::string stopEffectArg = getRequestArg(req, "stopEffect");

    PixelOverlayState blend;
    if (!parseBlend(getRequestArg(req, "blend"), blend)) {
        return errorResp("Unknown blend \"" + getRequestArg(req, "blend") +
                         "\"; expected opaque, transparent, or transparentrgb");
    }

    bool toOverlay = false;
    if (targetArg == "overlay") {
        toOverlay = true;
    } else if (!targetArg.empty() && targetArg != "channel") {
        return errorResp("Unknown target \"" + targetArg + "\"; expected channel or overlay");
    }

    // string_view, not getRequestContent(): the body is a whole frame and there
    // is no reason to copy it before we know what it is.
    std::string_view body = req->body();
    if (body.empty()) {
        return errorResp("Empty body");
    }

    std::string ctype = req->getHeader("content-type");
    std::transform(ctype.begin(), ctype.end(), ctype.begin(), ::tolower);

    enum class BodyKind {
        Raw,
        Json,
        Image
    };
    BodyKind kind = BodyKind::Raw;
    if (fmtArg == "image" || startsWith(ctype, "image/")) {
        kind = BodyKind::Image;
    } else if (startsWith(ctype, "application/json")) {
        kind = BodyKind::Json;
    } else {
        std::size_t i = 0;
        while (i < body.size() && isspace((unsigned char)body[i])) {
            i++;
        }
        if (i < body.size() && body[i] == '{') {
            kind = BodyKind::Json;
        }
    }

    // Where the pixel (or encoded image) bytes actually live. The JSON form
    // decodes into `decoded`; the other two point straight at the body.
    std::vector<uint8_t> decoded;
    const uint8_t* data = (const uint8_t*)body.data();
    std::size_t dataLen = body.size();

    if (kind == BodyKind::Json) {
        Json::Value root;
        if (!LoadJsonFromString(std::string(body), root)) {
            return errorResp("Could not parse the JSON body");
        }
        if (!root.isMember("Data")) {
            return errorResp("A JSON body needs a base64 \"Data\" member");
        }
        // JSON members win over query args -- one request, one source of truth.
        struct {
            const char* key;
            int* val;
            bool* explicitly;
        } jsonInts[] = {
            { "X", &x, &xExplicit },
            { "Y", &y, &yExplicit },
            { "W", &w, &wExplicit },
            { "H", &h, &hExplicit },
        };
        for (auto& a : jsonInts) {
            if (root.isMember(a.key)) {
                if (!root[a.key].isIntegral()) {
                    return errorResp(std::string("JSON member \"") + a.key + "\" is not an integer");
                }
                *a.val = root[a.key].asInt();
                *a.explicitly = true;
            }
        }
        if (root.isMember("Format")) {
            fmtArg = root["Format"].asString();
        }
        decoded = base64Decode(root["Data"].asString());
        if (decoded.empty()) {
            return errorResp("The \"Data\" member did not decode to any bytes");
        }
        data = decoded.data();
        dataLen = decoded.size();
        if (fmtArg == "image") {
            kind = BodyKind::Image;
        }
    }

    // An omitted w/h means "from the offset to the far edge", which makes a
    // plain full-frame post need no args at all.
    if (!wExplicit) {
        w = modelW - std::max(0, x);
    }
    if (!hExplicit) {
        h = modelH - std::max(0, y);
    }
    if (w <= 0 || h <= 0) {
        return errorResp("Region " + std::to_string(w) + "x" + std::to_string(h) +
                         " has no area (model is " + std::to_string(modelW) + "x" +
                         std::to_string(modelH) + ")");
    }

    int srcBpp = 3;
    if (kind != BodyKind::Image) {
        srcBpp = bytesForPixelFormat(fmtArg);
        if (!srcBpp) {
            return errorResp("Unknown fmt \"" + fmtArg + "\"; expected rgb, rgbw, mono, or image");
        }
        std::size_t expect = (std::size_t)w * h * srcBpp;
        if (dataLen != expect) {
            // Wrong length for the geometry given. If the bytes are an encoded
            // image, the caller almost certainly meant to post one, so take it
            // rather than making them add ?fmt=image.
            if (looksLikeEncodedImage(std::string_view((const char*)data, dataLen))) {
                kind = BodyKind::Image;
            } else {
                return errorResp("Body is " + std::to_string(dataLen) + " bytes; a " +
                                 std::to_string(w) + "x" + std::to_string(h) + " " +
                                 (fmtArg.empty() ? "rgb" : fmtArg) + " region needs " +
                                 std::to_string(expect) +
                                 " (w * h * " + std::to_string(srcBpp) + ")");
            }
        }
    }

    std::string scaling;
    if (kind == BodyKind::Image) {
        if (!parseImageScaling(scaleArg, scaling)) {
            return errorResp("Unknown scale \"" + scaleArg + "\"; expected fit, fill, stretch, or none");
        }
        // For an image, w/h are the box it gets scaled into, and unlike the raw
        // path there is no body length to cross-check them against. Clamp to the
        // model: a bigger box cannot draw anything more (the excess is clipped
        // away), but it would have GraphicsMagick allocate for it first --
        // ?w=9999&h=9999 is a ~300MB resize, which is fatal on a BeagleBone.
        w = std::min(w, modelW);
        h = std::min(h, modelH);
    }

    // autoEnable takes the same vocabulary as the Text and Image effects and,
    // like them, only ever promotes a Disabled model. Done while modelsLock is
    // still held, because that is the order every other state change uses.
    if (!autoEnableArg.empty()) {
        PixelOverlayState st(autoEnableArg);
        if (st.getState() != PixelOverlayState::Disabled &&
            m->getState().getState() == PixelOverlayState::Disabled) {
            m->setState(st);
        }
    }

    // Everything below is either slow or touches the effect lock, so drop
    // modelsLock first. See the note on this function.
    lock.unlock();

    // Take the model over. An effect still ticking on it (scrolling text, a
    // moving image) would overwrite whatever we draw on its very next update;
    // the Image effect's static path evicts the running effect for exactly this
    // reason. ?stopEffect=false opts out for a caller deliberately drawing over
    // one. Note this differs from the older .../pixel and .../fill endpoints,
    // which leave a running effect alone.
    if (stopEffectArg != "false" && stopEffectArg != "0") {
        std::unique_lock<std::recursive_mutex> l(m->getRunningEffectMutex());
        if (m->getRunningEffect()) {
            m->setRunningEffect(nullptr, 25); // one no-op tick, then dropped
        }
    }

    bool ok = false;
    std::string err;
    if (kind == BodyKind::Image) {
        // With no x/y given, centre the image in the box the way the Image
        // effect does; with either given, honour it as the top-left corner.
        bool center = !xExplicit && !yExplicit;
        ok = DrawEncodedImageOnModel(m, data, dataLen, scaling, w, h,
                                     x, y, center, blend, toOverlay, err);
    } else {
        ok = toOverlay ? m->blitOverlayBuffer(data, w, h, srcBpp, x, y)
                       : m->blitData(data, w, h, srcBpp, x, y, blend);
        if (!ok) {
            err = "the region landed entirely outside the model";
        }
    }
    if (!ok) {
        return errorResp(err);
    }

    LogDebug(VB_CHANNELOUT, "PUT overlay data from %s: model \"%s\", %zu bytes, %s, %dx%d @ %d,%d -> %s\n",
             getEffectiveClientIP(req).c_str(), m->getName().c_str(), dataLen,
             kind == BodyKind::Image ? "image" : (fmtArg.empty() ? "rgb" : fmtArg.c_str()),
             w, h, x, y, toOverlay ? "overlay buffer" : "channel data");

    Json::Value r;
    r["Status"] = "OK";
    r["Message"] = "";
    r["Model"] = m->getName();
    r["X"] = x;
    r["Y"] = y;
    r["W"] = w;
    r["H"] = h;
    r["Target"] = toOverlay ? "overlay" : "channel";
    r["BytesPerPixel"] = m->getBytesPerPixel();
    // True when part of the region fell outside the model and was clipped away
    // rather than rejected -- that is what lets a caller slide something off an
    // edge by reposting it at moving offsets.
    r["Clipped"] = (x < 0) || (y < 0) || ((x + w) > modelW) || ((y + h) > modelH);
    return makeStringResponse(SaveJsonToString(r, ""), 200, "application/json");
}

HttpResponsePtr PixelOverlayManager::render_GET(const HttpRequestPtr& req) {
    auto parts = getPathPieces(req->path());
    std::string p1 = parts[0];
    int plen = parts.size();
    LogDebug(VB_CHANNELOUT, "PixelOverlayManager::render_GET():   %s\n", p1.c_str());
    if (p1 == "models") {
        Json::Value result;
        bool empty = true;
        if (plen == 1) {
            std::unique_lock<std::recursive_mutex> lock(modelsLock);
            bool simple = false;
            if (getRequestArg(req, "simple") == "true") {
                simple = true;
            }
            if (getRequestArg(req, "all") == "true") {
                result.append("--All Models--");
            }
            bool includeSubs = getRequestArg(req, "submodels") == "true";
            if (includeSubs) {
                // Listing submodels for discovery: load the index (without
                // materializing every submodel) so their names are selectable.
                loadSubModelIndex();
            }
            for (auto& mn : modelNames) {
                if (simple) {
                    result.append(mn);
                    empty = false;
                } else {
                    Json::Value model;
                    models[mn].toJson(model);
                    result.append(model);
                    empty = false;
                }
            }
            if (includeSubs && simple) {
                // Append submodel names that are not already materialized. Only
                // names are listed here to avoid materializing every submodel.
                for (auto& sc : subModelConfigs) {
                    if (models.find(sc.first) == models.end()) {
                        result.append(sc.first);
                        empty = false;
                    }
                }
            }
        } else {
            std::string model = parts[1];
            std::string type;
            std::unique_lock<std::recursive_mutex> lock(modelsLock);
            PixelOverlayModel* m = getModelLocked(model); // resolves lazy submodels too
            if (m) {
                m->toJson(result);
            } else {
                return makeStringResponse("Model not found: " + parts[1], 404);
            }
        }
        if (empty && plen == 1) {
            return makeStringResponse("[]", 200, "application/json");
        } else {
            std::string resultStr = SaveJsonToString(result, "");
            return makeStringResponse(resultStr, 200, "application/json");
        }
    } else if (p1 == "overlays") {
        std::string p2 = plen > 1 ? parts[1] : "";
        std::string p3 = plen > 2 ? parts[2] : "";
        std::string p4 = plen > 3 ? parts[3] : "";
        std::string p5 = plen > 4 ? parts[4] : "";
        Json::Value result;
        if (p2 == "fonts") {
            for (auto& a : fonts) {
                result.append(a.first);
            }
        } else if (p2 == "images") {
            // Flat list of image files under the media images directory, as
            // paths relative to it, for the Image effect's file picker.  The
            // api/files/Images endpoint returns per-file metadata objects; the
            // command-argument dropdowns want a plain array of strings.
            collectOverlayImageNames(result);
        } else if (p2 == "settings") {
            result["autoCreate"] = autoCreate;
        } else if (p2 == "models") {
            std::unique_lock<std::recursive_mutex> lock(modelsLock);
            bool hasModels = false;
            for (auto& mn : modelNames) {
                Json::Value model;
                PixelOverlayModel* m = models[mn].model;
                m->toJson(model);
                model["isActive"] = (int)m->getState().getState();
                std::unique_lock<std::recursive_mutex> lock(m->getRunningEffectMutex());
                if (m->getRunningEffect()) {
                    model["effectName"] = m->getRunningEffect()->name();
                    model["isLocked"] = true;
                    model["effectRunning"] = true;
                } else {
                    model["effectRunning"] = false;
                }
                hasModels = true;
                model["width"] = m->getWidth();
                model["height"] = m->getHeight();
                result.append(model);
            }
            if (!hasModels) {
                result.resize(0);
            }
        } else if (p2 == "model") {
            std::unique_lock<std::recursive_mutex> lock(modelsLock);
            auto m = getModelLocked(p3); // resolves lazy submodels too
            if (!m && p4 == "data" && p5 == "raw") {
                // The legacy JSON paths fall through and answer 200 with a bare
                // "null" for an unknown model. That is left alone for
                // compatibility, but a binary endpoint must not hand back a
                // 4-byte JSON literal that a caller would read as pixel data.
                return makeStringResponse("Model not found: " + p3, 404);
            }
            if (m) {
                std::unique_lock<std::recursive_mutex> lock(m->getRunningEffectMutex());
                if (p4 == "data") {
                    if (p5 == "raw") {
                        // Binary read-back: width*height*bytesPerPixel bytes,
                        // the exact layout PUT .../data takes. The JSON form
                        // below spends 4-6 wire bytes per payload byte.
                        std::vector<uint8_t> buf;
                        m->getData(buf);
                        auto resp = makeStringResponse(std::string((const char*)buf.data(), buf.size()),
                                                       200, "application/octet-stream");
                        resp->addHeader("X-FPP-Model-Width", std::to_string(m->getWidth()));
                        resp->addHeader("X-FPP-Model-Height", std::to_string(m->getHeight()));
                        resp->addHeader("X-FPP-Model-BytesPerPixel", std::to_string(m->getBytesPerPixel()));
                        return resp;
                    }
                    Json::Value data;
                    m->getDataJson(data, p5 == "rle");
                    result["data"] = data;
                    result["rle"] = p5 == "rle";
                    result["isLocked"] = m->getRunningEffect() != nullptr; // compatibility
                    result["effectRunning"] = m->getRunningEffect() != nullptr;
                } else if (p4 == "clear") {
                    m->clear();
                    return makeStringResponse("{ \"Status\": \"OK\", \"Message\": \"\"}", 200);
                } else if (p4 == "preview") {
                    Json::Value pixels(Json::arrayValue);
                    collectModelPreviewPixels(m->getName(), pixels);
                    result["pixels"] = pixels;
                } else {
                    m->toJson(result);
                    result["isActive"] = (int)m->getState().getState();
                    if (m->getRunningEffect()) {
                        result["effectName"] = m->getRunningEffect()->name();
                        result["isLocked"] = true;
                        result["effectRunning"] = true;
                    } else {
                        result["isLocked"] = false; // compatibility
                        result["effectRunning"] = false;
                    }
                    result["width"] = m->getWidth();
                    result["height"] = m->getHeight();
                }
            }
        } else if (p2 == "effects") {
            if (p3 == "") {
                bool fullResult = getRequestArg(req, "full") == "true";
                for (auto& a : PixelOverlayEffect::GetPixelOverlayEffects()) {
                    if (fullResult) {
                        result.append(PixelOverlayEffect::GetPixelOverlayEffect(a)->getDescription());
                    } else {
                        result.append(a);
                    }
                }
            } else {
                PixelOverlayEffect* e = PixelOverlayEffect::GetPixelOverlayEffect(p3);
                if (e) {
                    result = e->getDescription();
                }
            }
        } else if (p2 == "running") {
            result = getActiveOverlayEffects();
        }
        std::string resultStr = SaveJsonToString(result, "");
        return makeStringResponse(resultStr, 200, "application/json");
    }
    return makeStringResponse("Not found: " + p1, 404);
}
/**
 * Replace the overlay model definitions (writes config/model-overlays.json) and
 * flag fppd for restart.
 *
 * @route POST /api/models
 * @response 200 Model overlay configuration saved.
 */

/**
 * Upload a raw channel-memory-map file (writes media/channelmemorymaps) and
 * flag fppd for restart.
 *
 * @route POST /api/models/raw
 * @response 200 Raw channel memory map saved.
 */
HttpResponsePtr PixelOverlayManager::render_POST(const HttpRequestPtr& req) {
    auto parts = getPathPieces(req->path());
    std::string p1 = parts[0];
    if (p1 == "models") {
        std::string p2 = parts.size() > 1 ? parts[1] : "";
        if (p2 == "raw") {
            // upload of raw file
            char filename[2048];
            strcpy(filename, FPP_DIR_MEDIA("/channelmemorymaps").c_str());

            int fp = open(filename, O_CREAT | O_TRUNC | O_RDWR, 0666);
            if (fp == -1) {
                LogErr(VB_CHANNELOUT, "Could not open Channel Memory Map config file %s\n", filename);
                return makeStringResponse("Could not open Channel Memory Map config file", 500);
            }
            std::string content(getRequestContent(req));
            write(fp, content.c_str(), content.length());
            close(fp);
            LogDebug(VB_COMMAND, "POST /api/models/raw from %s: saved %zu bytes to %s\n",
                     getEffectiveClientIP(req).c_str(), content.length(), filename);
            std::string nvresults;
            urlPut("http://127.0.0.1/api/settings/restartFlag", "1", nvresults);
            return makeStringResponse("{ \"Status\": \"OK\", \"Message\": \"\"}", 200);
        } else if (parts.size() == 1) {
            char filename[2048];
            strcpy(filename, FPP_DIR_CONFIG("/model-overlays.json").c_str());

            int fp = open(filename, O_CREAT | O_TRUNC | O_RDWR, 0666);
            if (fp == -1) {
                LogErr(VB_CHANNELOUT, "Could not open Channel Memory Map config file %s\n", filename);
                return makeStringResponse("Could not open Channel Memory Map config file", 500);
            }
            std::string content(getRequestContent(req));
            write(fp, content.c_str(), content.length());
            close(fp);
            LogDebug(VB_COMMAND, "POST /api/models from %s: saved %zu bytes to %s\n",
                     getEffectiveClientIP(req).c_str(), content.length(), filename);
            std::string nvresults;
            urlPut("http://127.0.0.1/api/settings/restartFlag", "1", nvresults);
            return makeStringResponse("{ \"Status\": \"OK\", \"Message\": \"\"}", 200);
        }
    }
    return makeStringResponse("POST Not found " + req->path(), 404);
}
/**
 * Set an overlay model's active state. Body: `{"State": <int>}`.
 *
 * @route PUT /api/overlays/model/{model}/state
 * @response 200 State updated.
 */

/**
 * Fill an overlay model with a solid color. Body: `{"RGB":[r,g,b]}` or `{"Value":v}`.
 *
 * @route PUT /api/overlays/model/{model}/fill
 * @response 200 Model filled.
 */

/**
 * Set a single pixel in an overlay model. Body: `{"X":x,"Y":y,"RGB":[r,g,b]}`.
 *
 * @route PUT /api/overlays/model/{model}/pixel
 * @response 200 Pixel set.
 */

/**
 * Write bulk pixel data to an overlay model in a single request, as an
 * alternative to one round trip per pixel via `.../pixel`.
 *
 * The body may be any of three things, and FPP works out which:
 *
 * - **Raw pixel bytes** (`application/octet-stream`) laid out row-major from
 *   the top-left of the region, `w * h * fmt` bytes exactly. A wrong length is
 *   an error, never a partial write.
 * - **An encoded image** (PNG, JPEG, GIF, BMP, TIFF, WebP), by `Content-Type:
 *   image/*` or `fmt=image`. Decoded and scaled server-side, so a 32x32 PNG
 *   costs a few hundred bytes instead of 3KB and no file has to exist on the
 *   player first.
 * - **JSON** with the payload base64 in a `Data` member:
 *   `{"X":0,"Y":0,"W":32,"H":32,"Format":"rgb","Data":"..."}`. Members given
 *   here override the equivalent query arguments.
 *
 * A region that hangs off an edge is clipped to what is visible rather than
 * rejected, so a caller can slide an image across a model (and off it) by
 * reposting at moving offsets.
 *
 * @route PUT /api/overlays/model/{model}/data
 * @param integer x Left edge of the region. Default 0; may be negative.
 * @param integer y Top edge of the region. Default 0; may be negative.
 * @param integer w Region width. Defaults to the model width less `x`. For an image, the box it is scaled into.
 * @param integer h Region height. Defaults to the model height less `y`. For an image, the box it is scaled into.
 * @param string fmt Raw pixel layout: `rgb` (default), `rgbw`, `mono`, or `image` to force image decoding.
 * @param string blend How this write combines with what is there: `opaque` (default), `transparent`, `transparentrgb`.
 * @param string scale Image scaling: `fit` (default), `fill`, `stretch`, `none`.
 * @param string target `channel` (default) writes the output channel data; `overlay` composites into the mmapped overlay buffer shared with external clients.
 * @param string autoEnable Enable the model if it is currently Disabled: `true`, `Enabled`, `Transparent`, or `TransparentRGB`.
 * @param boolean stopEffect Evict a running effect on the model first. Default true.
 * @response 200 Pixel data written; body reports the region and whether it was clipped.
 * @response 400 Malformed arguments, wrong body length, undecodable image, or a region entirely off the model.
 * @response 404 No model with that name exists.
 */

/**
 * Save an overlay model's current buffer to an image file. Body: `{"File":"name"}`.
 *
 * @route PUT /api/overlays/model/{model}/save
 * @response 200 Overlay saved as image.
 */

/**
 * Render text onto an overlay model. Body includes Message, Color, Font,
 * FontSize, Position, PixelsPerSecond, AntiAlias and optional AutoEnable.
 *
 * @route PUT /api/overlays/model/{model}/text
 * @response 200 Text effect started.
 */

/**
 * Force the overlay buffer to be memory-mapped so external programs can access it.
 *
 * @route PUT /api/overlays/model/{model}/mmap
 * @response 200 Overlay buffer mmapped.
 */

/**
 * Set, update, or delete active overlay channel ranges. Body: `{"Value":v}`,
 * `{"delete":true}`, or `{"deleteAll":true}`.
 *
 * @route PUT /api/overlays/range/{ranges}
 * @response 200 Ranges updated.
 */
HttpResponsePtr PixelOverlayManager::render_PUT(const HttpRequestPtr& req) {
    auto parts = getPathPieces(req->path());
    std::string p1 = parts[0];
    std::string p2 = parts.size() > 1 ? parts[1] : "";
    std::string p3 = parts.size() > 2 ? parts[2] : "";
    std::string p4 = parts.size() > 3 ? parts[3] : "";
    if (p1 == "overlays") {
        if (p2 == "model") {
            std::unique_lock<std::recursive_mutex> lock(modelsLock);
            // getModelLocked(), not models.find(): this resolves lazy xLights
            // submodels the same way render_GET() does. The raw map lookup used
            // here before meant every PUT (state, fill, pixel, ...) 404'd on a
            // submodel that GET could see perfectly well.
            auto m = getModelLocked(p3);
            if (m) {
                if (p4 == "state") {
                    Json::Value root;
                    if (LoadJsonFromString(std::string(getRequestContent(req)), root)) {
                        if (root.isMember("State")) {
                            m->setState(PixelOverlayState(root["State"].asInt()));
                            return makeStringResponse("{ \"Status\": \"OK\", \"Message\": \"\"}", 200);
                        } else {
                            return makeStringResponse("Invalid request " + std::string(getRequestContent(req)), 500);
                        }
                    } else {
                        return makeStringResponse("Could not parse request " + std::string(getRequestContent(req)), 500);
                    }
                } else if (p4 == "fill") {
                    Json::Value root;
                    if (LoadJsonFromString(std::string(getRequestContent(req)), root)) {
                        if (root.isMember("RGB")) {
                            m->fill(root["RGB"][0].asInt(),
                                    root["RGB"][1].asInt(),
                                    root["RGB"][2].asInt());
                            m->flushOverlayBuffer();
                            return makeStringResponse("{ \"Status\": \"OK\", \"Message\": \"\"}", 200);
                        } else if (root.isMember("Value")) {
                            m->fill(root["Value"].asInt(),
                                    root["Value"].asInt(),
                                    root["Value"].asInt());
                            return makeStringResponse("{ \"Status\": \"OK\", \"Message\": \"\"}", 200);
                        }
                    }
                } else if (p4 == "pixel") {
                    Json::Value root;
                    if (LoadJsonFromString(std::string(getRequestContent(req)), root)) {
                        if (root.isMember("RGB")) {
                            m->setPixelValue(root["X"].asInt(),
                                             root["Y"].asInt(),
                                             root["RGB"][0].asInt(),
                                             root["RGB"][1].asInt(),
                                             root["RGB"][2].asInt());
                            return makeStringResponse("{ \"Status\": \"OK\", \"Message\": \"\"}", 200);
                        }
                    }
                } else if (p4 == "data") {
                    // Takes over `lock` and releases it before the slow work.
                    return bulkPixelDataPut(req, m, lock);
                } else if (p4 == "save") {
                    Json::Value root;
                    if (LoadJsonFromString(std::string(getRequestContent(req)), root)) {
                        if (root.isMember("File")) {
                            m->saveOverlayAsImage(root["File"].asString());
                            return makeStringResponse("{ \"Status\": \"OK\", \"Message\": \"\"}", 200);
                        }
                    }
                } else if (p4 == "text") {
                    Json::Value root;
                    if (LoadJsonFromString(std::string(getRequestContent(req)), root)) {
                        if (root.isMember("Message")) {
                            std::string color = root["Color"].asString();
                            unsigned int x = mapColor(color);

                            std::string msg = root["Message"].asString();

                            std::string font = root["Font"].asString();
                            std::string position = root["Position"].asString();
                            int fontSize = root["FontSize"].asInt();
                            bool aa = root["AntiAlias"].asBool();
                            int pps = root["PixelsPerSecond"].asInt();
                            bool autoEnable = false;
                            if (root.isMember("AutoEnable")) {
                                autoEnable = root["AutoEnable"].asBool();
                            }

                            std::string f = fonts[font];
                            if (f == "") {
                                f = font;
                            }

                            std::vector<std::string> args;
                            args.push_back(p3);
                            args.push_back(autoEnable ? "true" : "false");
                            args.push_back("Text");
                            args.push_back(color);
                            args.push_back(font);
                            args.push_back(std::to_string(fontSize));
                            args.push_back(aa ? "true" : "false");
                            args.push_back(position);
                            args.push_back(std::to_string(pps));
                            args.push_back("0");
                            args.push_back(msg);
                            lock.unlock();
                            LogDebug(VB_COMMAND, "PixelOverlay HTTP API from %s running \"Overlay Model Effect\" on model \"%s\"\n", getEffectiveClientIP(req).c_str(), p3.c_str());
                            CommandManager::INSTANCE.run("Overlay Model Effect", args);
                            return makeStringResponse("{ \"Status\": \"OK\", \"Message\": \"\"}", 200);
                        }
                    }
                } else if (p4 == "mmap") {
                    // Force mmap the overlay buffer so external programs can have access to it
                    m->getOverlayBuffer();
                    return makeStringResponse("{ \"Status\": \"OK\", \"Message\": \"\"}", 200);
                } else {
                    return makeStringResponse("Model Command Not found " + p4, 404);
                }
            } else {
                return makeStringResponse("Model Not found " + p3, 404);
            }
        } else if (p2 == "range") {
            int val = -1;
            bool deleteAll = false;
            if (p4 == "") {
                Json::Value root;
                if (LoadJsonFromString(std::string(getRequestContent(req)), root)) {
                    if (root.isMember("delete") && root["delete"].asBool()) {
                        val = -1;
                    } else if (root.isMember("deleteAll") && root["deleteAll"].asBool()) {
                        std::unique_lock<std::recursive_mutex> lock(activeModelsLock);
                        int sz = activeRanges.size();
                        activeRanges.clear();
                        numActive -= sz;
                        lock.unlock();
                        if (numActive == 0) {
                            numActive++;
                            std::this_thread::sleep_for(std::chrono::milliseconds(125));
                            numActive--;
                        }
                        return makeStringResponse("{ \"Status\": \"OK\", \"Message\": \"\"}", 200);
                    } else if (root.isMember("Value")) {
                        val = root["Value"].asInt();
                    }
                }
            } else if (p4 != "delete") {
                val = std::stoi(p4, nullptr, 10);
            }

            while (p3.size()) {
                std::size_t pos = 0;
                int start = std::stoi(p3, &pos, 10);
                int end = start;
                p3 = p3.substr(pos);
                if (p3.size() && p3[0] == '-') {
                    p3 = p3.substr(1);
                    end = std::stoi(p3, &pos, 10);
                    p3 = p3.substr(pos);
                }
                int startActive = numActive;
                if (start > 0 && end >= start) {
                    end--;
                    start--;
                    std::unique_lock<std::recursive_mutex> lock(activeModelsLock);
                    auto it = activeRanges.begin();
                    bool found = false;
                    while (it != activeRanges.end()) {
                        if (it->start == start && it->end == end) {
                            found = true;
                            if (val >= 0) {
                                it->value = val;
                                it++;
                            } else {
                                activeRanges.erase(it);
                                it = activeRanges.begin();
                                numActive--;
                            }
                        } else {
                            it++;
                        }
                    }
                    if (!found) {
                        activeRanges.push_back(OverlayRange(start, end, val));
                        numActive++;
                    }
                }
                // skip the ','
                if (p3.size()) {
                    p3 = p3.substr(1);
                }
                if (numActive == 0 && startActive) {
                    // removed some, may need to wait to make sure the new values are output
                    numActive++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(125));
                    numActive--;
                }
            }
            return makeStringResponse("{ \"Status\": \"OK\", \"Message\": \"\"}", 200);
        }
    }
    return makeStringResponse("PUT Not found " + req->path(), 404);
}

class OverlayCommand : public Command {
public:
    OverlayCommand(const std::string& s, PixelOverlayManager* m) :
        Command(s),
        manager(m) {}
    virtual ~OverlayCommand() {}

    std::recursive_mutex& getLock() { return manager->modelsLock; }
    PixelOverlayModel* getModelLocked(const std::string& name) {
        return manager->getModelLocked(name);
    }

    PixelOverlayManager* manager;
};

class EnableOverlayCommand : public OverlayCommand {
public:
    EnableOverlayCommand(PixelOverlayManager* m) :
        OverlayCommand("Overlay Model State", m) {
        args.push_back(CommandArg("Model", "multistring", "Model").setContentListUrl("api/models?simple=true", false));
        args.push_back(CommandArg("State", "string", "State").setContentList({ "Disabled", "Enabled", "Transparent", "TransparentRGB" }));
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override {
        if (args.size() != 2) {
            return std::make_unique<Command::ErrorResult>("Command needs 2 arguments, found " + std::to_string(args.size()));
        }
        std::unique_lock<std::recursive_mutex> lock(getLock());
        std::list<PixelOverlayModel*> models;
        for (auto& ms : split(args[0], ',')) {
            auto m = getModelLocked(ms);
            if (m) {
                models.push_back(m);
            } else {
                return std::make_unique<Command::ErrorResult>("No model found: " + ms);
            }
        }
        for (auto m : models) {
            m->setState(PixelOverlayState(args[1]));
        }
        return std::make_unique<Command::Result>("Model State Set");
    }
};
class ClearOverlayCommand : public OverlayCommand {
public:
    ClearOverlayCommand(PixelOverlayManager* m) :
        OverlayCommand("Overlay Model Clear", m) {
        args.push_back(CommandArg("Model", "multistring", "Model").setContentListUrl("api/models?simple=true", false));
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override {
        if (args.size() != 1) {
            return std::make_unique<Command::ErrorResult>("Command needs 1 argument, found " + std::to_string(args.size()));
        }
        std::unique_lock<std::recursive_mutex> lock(getLock());
        std::list<PixelOverlayModel*> models;
        for (auto& ms : split(args[0], ',')) {
            auto m = getModelLocked(ms);
            if (m) {
                models.push_back(m);
            } else {
                return std::make_unique<Command::ErrorResult>("No model found: " + ms);
            }
        }
        for (auto m : models) {
            m->clear();
        }
        return std::make_unique<Command::Result>("Models Cleared");
    }
};
class FillOverlayCommand : public OverlayCommand {
public:
    FillOverlayCommand(PixelOverlayManager* m) :
        OverlayCommand("Overlay Model Fill", m) {
        args.push_back(CommandArg("Model", "multistring", "Model").setContentListUrl("api/models?simple=true", false));
        args.push_back(CommandArg("State", "string", "State").setContentList({ "Don't Set", "Enabled", "Transparent", "TransparentRGB" }));
        args.push_back(CommandArg("Color", "color", "Color").setDefaultValue("#FF0000"));
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override {
        if (args.size() < 2 || args.size() > 3) {
            return std::make_unique<Command::ErrorResult>("Command needs 2 or 3 arguments, found " + std::to_string(args.size()));
        }
        std::unique_lock<std::recursive_mutex> lock(getLock());
        std::list<PixelOverlayModel*> models;
        for (auto& ms : split(args[0], ',')) {
            auto m = getModelLocked(ms);
            if (m) {
                models.push_back(m);
            } else {
                return std::make_unique<Command::ErrorResult>("No model found: " + ms);
            }
        }
        for (auto m : models) {
            std::string color;
            std::string state = "Don't Set";
            if (args.size() == 2) {
                color = args[1];
            } else {
                state = args[1];
                color = args[2];
            }
            if (state != "Don't Set") {
                m->setState(PixelOverlayState(state));
            }
            if (color.size() == 9 && color[0] == '#') {
                // #RRGGBBWW - explicit white channel for RGBW models
                uint32_t x = (uint32_t)std::stoul(color.substr(1), nullptr, 16);
                m->fillOverlayBuffer((x >> 24) & 0xFF,
                                     (x >> 16) & 0xFF,
                                     (x >> 8) & 0xFF,
                                     x & 0xFF);
            } else {
                unsigned int x = PixelOverlayManager::mapColor(color);
                m->fillOverlayBuffer((x >> 16) & 0xFF,
                                     (x >> 8) & 0xFF,
                                     x & 0xFF);
            }
            m->flushOverlayBuffer();
        }
        return std::make_unique<Command::Result>("Models Filled");
    }
};
class TextOverlayCommand : public OverlayCommand {
public:
    TextOverlayCommand(PixelOverlayManager* m) :
        OverlayCommand("Overlay Model Text", m) {
        args.push_back(CommandArg("Model", "string", "Model").setContentListUrl("api/models?simple=true", false));
        args.push_back(CommandArg("Color", "color", "Color").setDefaultValue("#FF0000"));
        args.push_back(CommandArg("Font", "string", "Font").setContentListUrl("api/overlays/fonts", false));
        args.push_back(CommandArg("FontSize", "int", "FontSize").setRange(4, 100).setDefaultValue("18"));
        args.push_back(CommandArg("FontAntiAlias", "bool", "Anti-Aliased").setDefaultValue("false"));
        args.push_back(CommandArg("Position", "string", "Position").setContentList({ "Center", "Right to Left", "Left to Right", "Bottom to Top", "Top to Bottom" }));
        args.push_back(CommandArg("Speed", "int", "Scroll Speed").setRange(0, 200).setDefaultValue("10"));
        args.push_back(CommandArg("AutoEnable", "bool", "Auto Enable/Disable Model").setDefaultValue("false"));

        // keep text as last argument if possible as the MQTT commands will, by default, use the payload of the mqtt
        // msg as the last argument.  Thus, this allows all of the above to be topic paths, but the text to be
        // sent in the payload
        args.push_back(CommandArg("Text", "string", "Text").setAdjustable());
    }
    virtual bool hidden() const override { return true; }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override {
        if (args.size() < 8) {
            return std::make_unique<Command::ErrorResult>("Command needs 9 arguments, found " + std::to_string(args.size()));
        }
        std::vector<std::string> newArgs;
        newArgs.push_back(args[0]); // model
        if (args.size() == 9) {
            newArgs.push_back(args[7]);
        } else {
            newArgs.push_back("true");
        }
        newArgs.push_back("Text");
        for (int x = 1; x < 7; x++) {
            newArgs.push_back(args[x]);
        }
        newArgs.push_back("0"); // duration
        if (args.size() == 9) {
            newArgs.push_back(args[8]);
        } else {
            newArgs.push_back(args[7]);
        }
        LogDebug(VB_COMMAND, "Legacy command \"%s\" translating to \"Overlay Model Effect\"\n", name.c_str());
        return CommandManager::INSTANCE.run("Overlay Model Effect", newArgs);
    }
};

class ApplyEffectOverlayCommand : public OverlayCommand {
public:
    ApplyEffectOverlayCommand(PixelOverlayManager* m) :
        OverlayCommand("Overlay Model Effect", m) {
        args.push_back(CommandArg("Models", "multistring", "Models").setContentListUrl("api/models?simple=true&all=true", false));
        args.push_back(CommandArg("AutoEnable", "string", "Auto Enable/Disable").setContentList({ "False", "Enabled", "Transparent", "Transparent RGB" }).setDefaultValue("Enabled"));
        args.push_back(CommandArg("Effect", "subcommand", "Effect").setContentListUrl("api/overlays/effects/", false));
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override {
        if (args.size() < 3) {
            return std::make_unique<Command::ErrorResult>("Command needs at least 3 arguments, found " + std::to_string(args.size()));
        }
        std::unique_lock<std::recursive_mutex> lock(getLock());
        std::list<PixelOverlayModel*> models;

        for (auto& ms : split(args[0], ',')) {
            if (ms == "--All Models--") {
                for (auto& mn : manager->getModelNames()) {
                    auto m = getModelLocked(mn);
                    models.push_back(m);
                }
                break;
            }
            auto m = getModelLocked(ms);
            if (m) {
                models.push_back(m);
            } else {
                return std::make_unique<Command::ErrorResult>("No model found: " + ms);
            }
        }
        for (auto m : models) {
            std::string effect = args[2];
            std::vector<std::string> newArgs(args.begin() + 3, args.end());
            if (!m->applyEffect(args[1], effect, newArgs)) {
                return std::make_unique<Command::ErrorResult>("Could not start effect: " + effect);
            }
        }
        return std::make_unique<Command::Result>("Model Effect Started");
    }
};

void PixelOverlayManager::RegisterCommands() {
    if (models.empty()) {
        return;
    }
    CommandManager::INSTANCE.addCategorizedCommand(new EnableOverlayCommand(this), "Pixel Overlay", 1);
    CommandManager::INSTANCE.addCategorizedCommand(new FillOverlayCommand(this), "Pixel Overlay", 1);
    CommandManager::INSTANCE.addCategorizedCommand(new TextOverlayCommand(this), "Pixel Overlay", 1);
    CommandManager::INSTANCE.addCategorizedCommand(new ClearOverlayCommand(this), "Pixel Overlay", 1);
    CommandManager::INSTANCE.addCategorizedCommand(new ApplyEffectOverlayCommand(this), "Pixel Overlay", 1);
}

void PixelOverlayManager::addAutoOverlayModel(const std::string& name,
                                              uint32_t startChannel, uint32_t channelCount, uint32_t channelPerNode,
                                              const std::string& orientation, const std::string& startLocation,
                                              uint32_t strings, uint32_t strands, const std::string& colorOrder) {
    Json::Value val;
    val["Name"] = name;
    val["Type"] = "Channel";
    val["StartChannel"] = startChannel + 1;
    val["ChannelCount"] = channelCount;
    val["StringCount"] = strings;
    val["StrandsPerString"] = strands;
    val["Orientation"] = orientation;
    val["StartCorner"] = startLocation;
    val["ChannelCountPerNode"] = channelPerNode;
    val["ColorOrder"] = colorOrder;
    val["autoCreated"] = true;

    addModel(val);
}
void PixelOverlayManager::removeAutoOverlayModel(const std::string& name) {
    std::unique_lock<std::recursive_mutex> lock(modelsLock);
    PixelOverlayModel* pmodel = models[name].model;
    if (pmodel && pmodel->isAutoCreated()) {
        modelNames.remove(name);
        models[name].model = nullptr;
        for (auto& l : models[name].listeners) {
            l.second(nullptr);
        }
        lock.unlock();

        removePeriodicUpdate(pmodel);
        std::unique_lock<std::recursive_mutex> alock(activeModelsLock);
        activeModels.remove(pmodel);
        alock.unlock();

        delete pmodel;
    }
}

Json::Value PixelOverlayManager::getActiveOverlayEffects() {
    Json::Value ret = Json::arrayValue;

    // quickly create a snapshot of the active models
    // so we don't hold the lock while iterating through them
    // and to avoid deadlocks if the model is being modified
    // while we are iterating through it
    std::unique_lock<std::recursive_mutex> lock(activeModelsLock);
    auto am = activeModels;
    lock.unlock();

    for (auto& m : am) {
        std::unique_lock<std::recursive_mutex> l(m->getRunningEffectMutex());
        RunningEffect* eff = m->getRunningEffect();
        if (eff) {
            Json::Value model;
            m->toJson(model);
            eff->toJson(model["effect"]);
            ret.append(model);
        }
    }
    return ret;
}

void PixelOverlayManager::doOverlayModelEffects() {
    SetThreadName("FPP-OverlayME");
    std::unique_lock<std::mutex> l(threadLock);
    while (threadKeepRunning) {
        uint32_t waitTime = 1000;
        if (!updates.empty()) {
            uint64_t curTime = GetTimeMS();
            while (!updates.empty() && updates.begin()->first <= curTime) {
                std::list<PixelOverlayModel*> models = updates.begin()->second;
                uint64_t startTime = updates.begin()->first;
                updates.erase(updates.begin());
                l.unlock();

                for (auto m : models) {
                    int32_t ms = m->updateRunningEffects();
                    if (ms != 0) {
                        l.lock();
                        if (ms > 0) {
                            uint64_t t = startTime + ms;
                            updates[t].push_back(m);
                        } else {
                            afterOverlayModels.push_back(m);
                        }
                        l.unlock();
                    }
                }
                l.lock();
                curTime = GetTimeMS();
            }
            if (!updates.empty()) {
                waitTime = updates.begin()->first - curTime;
            }
        }
        threadCV.wait_for(l, std::chrono::milliseconds(waitTime));
    }
}
void PixelOverlayManager::removePeriodicUpdate(PixelOverlayModel* m) {
    std::unique_lock<std::mutex> l(threadLock);
    for (auto& a : updates) {
        a.second.remove(m);
    }
    afterOverlayModels.remove(m);
}

void PixelOverlayManager::addPeriodicUpdate(int32_t initialDelayMS, PixelOverlayModel* m) {
    std::unique_lock<std::mutex> l(threadLock);
    if (updateThread == nullptr) {
        threadKeepRunning = true;
        updateThread = new std::thread(&PixelOverlayManager::doOverlayModelEffects, this);
    }
    if (initialDelayMS > 0) {
        uint64_t nextTime = GetTimeMS();
        nextTime += initialDelayMS;
        updates[nextTime].push_back(m);
    } else {
        afterOverlayModels.push_back(m);
    }
    l.unlock();
    threadCV.notify_all();
}
