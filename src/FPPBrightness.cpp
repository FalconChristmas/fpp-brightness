#include <fpp-pch.h>

#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <cstring>

#include <unistd.h>
#include <termios.h>
#include <chrono>
#include <thread>
#include <cmath>

#include <httpserver.hpp>
#include "common.h"
#include "settings.h"
#include "Plugin.h"
#include "Plugins.h"
#include "log.h"
#include "Events.h"
#include "MultiSync.h"
#include "channeloutput/ChannelOutputSetup.h"
#include "fppversion_defines.h"
#include "commands/Commands.h"

class FPPBrightnessPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelDataPlugin, public FPPPlugins::APIProviderPlugin, public httpserver::http_resource {
public:
    
    FPPBrightnessPlugin() : FPPPlugins::Plugin("fpp-brightness"), FPPPlugins::ChannelDataPlugin(), FPPPlugins::APIProviderPlugin() {
        int startBrightness = 100;
        configLocation = FPP_DIR_CONFIG("/plugin.fpp-brightness.json");
        if (FileExists(configLocation)) {
            Json::Value root;
            if (LoadJsonFromFile(configLocation, root)) {
                if (root.isMember("brightness")) {
                    startBrightness = root["brightness"].asInt();
                }
            }
        }
        setBrightness(startBrightness, false);
        registerCommand();
    }
    virtual ~FPPBrightnessPlugin() {}
    
    class SetBrightnessCommand : public Command {
    public:
        SetBrightnessCommand(FPPBrightnessPlugin *p) : Command("Brightness"), plugin(p) {
            args.push_back(CommandArg("brightness", "int", "Brightness").setRange(0, 200)
                           .setDefaultValue("100"));
        }
        
        virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override {
            int brightness = 100;
            if (args.size() >= 1) {
                brightness = std::stoi(args[0]);
            }
            plugin->resetFade();
            plugin->setBrightness(brightness, false);
            return std::make_unique<Command::Result>("Brightness Set");
        }
        FPPBrightnessPlugin *plugin;
    };
    class AdjustBrightnessCommand : public Command {
    public:
        AdjustBrightnessCommand(FPPBrightnessPlugin *p) : Command("Brightness Adjust"), plugin(p) {
            args.push_back(CommandArg("brightness", "int", "Brightness").setRange(-100, 100)
                           .setDefaultValue("0"));
        }
        
        virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override {
            int adjust = 0;
            if (args.size() >= 1) {
                adjust = std::stoi(args[0]);
            }
            plugin->resetFade();
            plugin->setBrightness(plugin->brightness + adjust, false);
            return std::make_unique<Command::Result>("Brightness Adjusted");
        }
        FPPBrightnessPlugin *plugin;
    };
    class FadeBrightnessCommand : public Command {
    public:
        FadeBrightnessCommand(FPPBrightnessPlugin *p) : Command("Brightness Fade"), plugin(p) {
            args.push_back(CommandArg("brightness", "int", "End brightness").setRange(0, 200)
                           .setDefaultValue("100"));
            args.push_back(CommandArg("duration", "int", "Duration (seconds)").setRange(0, 14400)
                           .setDefaultValue("60"));
        }

        virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override {
            int newBrightness = 100;
            int duration = 60;
            if (args.size() >= 1) {
                newBrightness = std::stoi(args[0]);
            }
            if (args.size() >= 2) {
                duration = std::stoi(args[1]);
            }
            plugin->fade(newBrightness, duration);
            return std::make_unique<Command::Result>("Brightness Fade");
        }

        FPPBrightnessPlugin *plugin;
    };

    void registerCommand() {
        CommandManager::INSTANCE.addCommand(new SetBrightnessCommand(this));
        CommandManager::INSTANCE.addCommand(new AdjustBrightnessCommand(this));
        CommandManager::INSTANCE.addCommand(new FadeBrightnessCommand(this));
    }
    
    virtual HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request &req) override {        
        std::string p0 = req.get_path_pieces()[0];
        int plen = req.get_path_pieces().size();
        if (plen > 1) {
            std::vector<std::string> vals;
            for (int x = 1; x < req.get_path_pieces().size(); x++) {
                std::string p1 = req.get_path_pieces()[x];
                vals.push_back(p1);
            }
            setBrightness(vals);
        }
        
        std::string v = std::to_string(brightness);
        return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(v, 200));
    }
    virtual void multiSyncData(const uint8_t *data, int len) override {
        std::vector<std::string> v;
        std::string s = (char*)data;
        int start = 0;
        for (int x = 0; x < len; x++) {
            if (s[x] == '/') {
                v.push_back(s.substr(start, x - start));
                start = x + 1;
            }
        }
        if (len != start) {
            v.push_back(s.substr(start, len - start));
        }
        
        for (int x = 0; x < v.size(); x++) {
            //some path that includes the Brightness part
            if (v[x] == "Brightness") {
                for (int y = 0; y <= x; y++) {
                    v.erase(v.begin());
                }
                x = v.size();
            }
        }
        setBrightness(v);
    }
    void setBrightness(std::vector<std::string> &vals) {
        while (vals.size() < 2) {
            vals.push_back("");
        }
        std::string p1 = vals[0];
        if (p1 == "FadeUp") {
            std::string p2 = vals[1];
            if (p2 != "") {
                int len = std::stoi(p2);
                endFadeTime = lastms + len;
                startFadeBrightness = brightness;
                endFadeBrightness = 100;
                startFadeTime = lastms;
                
                std::string data = p1 + "/" + p2;
                int dlen = data.size() + 1;
                if (multiSync->isMultiSyncEnabled()) multiSync->SendPluginData(name, (uint8_t*)data.c_str(), dlen);
            }
        } else if (p1 == "FadeDown") {
            std::string p2 = vals[1];
            if (p2 != "") {
                int len = std::stoi(p2);
                
                endFadeTime = lastms + len;
                startFadeBrightness = brightness;
                endFadeBrightness = 0;
                startFadeTime = lastms;
                
                std::string data = p1 + "/" + p2;
                int dlen = data.size() + 1;
                if (multiSync->isMultiSyncEnabled()) multiSync->SendPluginData(name, (uint8_t*)data.c_str(), dlen);
            }
        } else if (p1 != "") {
            int i = std::stoi(p1);
            setBrightness(i);
            startFadeTime = -1;
        }
    }
    void unregisterApis(httpserver::webserver* m_ws) override {
        m_ws->unregister_resource("/Brightness");
    }
    void registerApis(httpserver::webserver *m_ws) override {
        m_ws->register_resource("/Brightness", this, true);
        
        std::function<void(const std::string &topic, const std::string &payload)> f = [this](const std::string &topic, const std::string &payload){
            multiSyncData((uint8_t*)payload.c_str(), payload.size() + 1);
        };
        Events::AddCallback("/Brightness", f);
        Events::RemoveCallback("/Brightness");
    }
    
    virtual void modifyChannelData(int ms, uint8_t *seqData) override {
        // We need real time, not sequence time in case a sequence is not
        // running, so use GetTimeMS here instead of passed-in ms.
        lastms = GetTimeMS();

        if (startFadeTime != -1) {
            if (lastms >= endFadeTime) {
                startFadeTime = -1;
                setBrightness(endFadeBrightness, false);
            } else {
                float f = 1.0 * (lastms - startFadeTime)/(endFadeTime - startFadeTime);
                int newb = (int)(f * (endFadeBrightness - startFadeBrightness)) + startFadeBrightness;
                setBrightness(newb, false);
            }
        }
        calcRanges();
        for (auto &a : ranges) {
            int len = a.second;
            for (int x = 0, start = a.first; x < len; x++, start++) {
                seqData[start] = map[seqData[start]];
            }
        }
    }

    std::vector<std::pair<uint32_t, uint32_t>> subtractRanges(const std::vector<std::pair<uint32_t, uint32_t>>& src, const std::vector<std::pair<uint32_t, uint32_t>>& sub) {
        std::vector<std::pair<uint32_t, uint32_t>> result;

        for (const auto& srcRange : src) {
            // Start with the full source range and progressively subtract exclude ranges
            std::vector<std::pair<uint32_t, uint32_t>> currentRanges;
            currentRanges.push_back(srcRange);

            // Process each exclude range
            for (const auto& excludeRange : sub) {
                std::vector<std::pair<uint32_t, uint32_t>> newRanges;

                for (const auto& curRange : currentRanges) {
                    // Check if exclude range overlaps with current range
                    if (excludeRange.second < curRange.first || excludeRange.first > curRange.second) {
                        // No overlap - keep the current range as-is
                        newRanges.push_back(curRange);
                    } else if (excludeRange.first <= curRange.first && excludeRange.second >= curRange.second) {
                        // Exclude range completely covers current range - skip it (don't add to newRanges)
                    } else if (excludeRange.first > curRange.first && excludeRange.second < curRange.second) {
                        // Exclude range is completely within current range - split into two ranges
                        // Add the portion before the exclude (if excludeRange.first > 0)
                        if (excludeRange.first > 0) {
                            newRanges.push_back(std::make_pair(curRange.first, excludeRange.first - 1));
                        }
                        // Add the portion after the exclude (if excludeRange.second < UINT32_MAX)
                        if (excludeRange.second < UINT32_MAX) {
                            newRanges.push_back(std::make_pair(excludeRange.second + 1, curRange.second));
                        }
                    } else if (excludeRange.first <= curRange.first) {
                        // Exclude range overlaps the start - keep only the end portion
                        if (excludeRange.second < UINT32_MAX) {
                            newRanges.push_back(std::make_pair(excludeRange.second + 1, curRange.second));
                        }
                    } else {
                        // Exclude range overlaps the end - keep only the start portion
                        if (excludeRange.first > 0) {
                            newRanges.push_back(std::make_pair(curRange.first, excludeRange.first - 1));
                        }
                    }
                }

                currentRanges = newRanges;
            }

            // Add all remaining ranges to the result
            for (const auto& range : currentRanges) {
                if (range.first <= range.second) {
                    result.push_back(range);
                }
            }
        }

        return result;
    }


    void calcRanges() {
        if (ranges.empty()) {
            std::vector<std::pair<uint32_t, uint32_t>> excludes;

            std::string ex = settings["BrightnessExcludeRanges"];
            if (ex != "") {
                std::vector<std::string> exranges = split(ex, ',');
                for (auto &r : exranges) {
                    size_t idx = r.find('-');
                    if (idx != std::string::npos) {
                        std::string fp = r.substr(0, idx);
                        std::string ep = r.substr(idx + 1);
                        int st = std::atoi(fp.c_str());
                        if (st > 0) {
                            uint32_t start = st - 1;
                            int end = std::atoi(ep.c_str()) - 1;
                            if (end > 0 && end > start) {
                                excludes.emplace_back(start, end);
                            }
                        }
                    } else {
                        int start = std::atoi(r.c_str());
                        if (start > 0) {
                            excludes.emplace_back(start - 1, start - 1);
                        }
                    }
                }
                std::vector<std::pair<uint32_t, uint32_t>> srcRanges;
                for (auto & rng : GetOutputRanges()) {
                    uint32_t end = rng.first + rng.second - 1;
                    srcRanges.emplace_back(rng.first, end);
                }
                for (auto &a : subtractRanges(srcRanges, excludes)) {
                    ranges.emplace_back(a.first, a.second - a.first + 1);
                }
            }
            if (ranges.empty()) {
                ranges = GetOutputRanges();
            }
        }
    }
    
    
    void setBrightness(int i, bool sendSync = true) {
        if (brightness != i) {
            brightness = i;
            for (int x = 0; x < 256; x++) {
                float b = x * (float)i;
                b /= 100.0f;
                int newb = std::round(b);
                if (newb == 0 && b > 0.2) {
                    newb = 1;
                }
                if (newb > 255) {
                    newb = 255;
                }
                map[x] = newb;
            }
            Json::Value val;
            val["brightness"] = i;
            SaveJsonToFile(val, configLocation);
        }
        if (sendSync && multiSync->isMultiSyncEnabled()) {
            std::string s = std::to_string(i);
            int len = s.size() + 1;
            multiSync->SendPluginData(name, (uint8_t*)s.c_str(), len);
        }
    }

    void resetFade() {
        startFadeTime = -1;
    }

    void fade(int newBrightness, int duration = 60) {
        startFadeBrightness = brightness;
        endFadeBrightness = newBrightness;
        startFadeTime = lastms;
        endFadeTime = lastms + (duration * 1000);

        LogDebug(VB_PLUGIN, "Setup fade from %d-%d for time %d-%d (%d ms)\n",
            startFadeBrightness, endFadeBrightness,
            startFadeTime, endFadeTime, endFadeTime - startFadeTime);
    }
    

    std::string configLocation;
    long long startFadeTime = -1;
    long long endFadeTime = 0;
    int startFadeBrightness = 0;
    int endFadeBrightness = 0;
    
    int brightness = -1;
    long long lastms = 0;
    uint8_t map[256];

    std::vector<std::pair<std::uint32_t, std::uint32_t>> ranges;
};


extern "C" {
    FPPPlugins::Plugin *createPlugin() {
        return new FPPBrightnessPlugin();
    }
}
