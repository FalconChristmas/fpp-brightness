
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
#include <jsoncpp/json/json.h>
#include "common.h"
#include "settings.h"
#include "Plugin.h"
#include "Plugins.h"
#include "log.h"
#include "channeloutput/channeloutput.h"
#include "mqtt.h"
#include "MultiSync.h"

#if __has_include("fppversion_defines.h")
#include "fppversion_defines.h"
#else
#define FPP_MAJOR_VERSION 3
#define FPP_MINOR_VERSION 4
#endif

#if __has_include("commands/Commands.h")
#include "commands/Commands.h"
#endif

class FPPBrightnessPlugin : public FPPPlugin, public httpserver::http_resource {
public:
    
    FPPBrightnessPlugin() : FPPPlugin("fpp-brightness") {
        int startBrightness = 100;
        if (FileExists("/home/fpp/media/config/plugin.fpp-brightness.json")) {
            Json::Value root;
            if (LoadJsonFromFile("/home/fpp/media/config/plugin.fpp-brightness.json", root)) {
                if (root.isMember("brightness")) {
                    startBrightness = root["brightness"].asInt();
                }
            }
        }
        setBrightness(startBrightness, false);
        registerCommand();
    }
    virtual ~FPPBrightnessPlugin() {}
    
#if FPP_MAJOR_VERSION > 3 || FPP_MINOR_VERSION > 4
    class SetBrightnessCommand : public Command {
    public:
        SetBrightnessCommand(FPPBrightnessPlugin *p) : Command("Brightness"), plugin(p) {
            args.push_back(CommandArg("brightness", "int", "Brightness").setRange(0, 200)
                           .setDefaultValue("100")
                           .setGetAdjustableValueURL("api/plugin-apis/Brightness"));
            args.push_back(CommandArg("remotes", "bool", "Send to Remotes").setDefaultValue("true"));
        }
        
        virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override {
            int brightness = 100;
            bool remotes = true;
            if (args.size() >= 1) {
                brightness = std::stoi(args[0]);
            }
            if (args.size() >= 2) {
                remotes= args[1] == "true" || args[1] == "1";
            }
            plugin->setBrightness(brightness, remotes);
            return std::make_unique<Command::Result>("Brightness Set");
        }
        FPPBrightnessPlugin *plugin;
    };
    void registerCommand() {
        CommandManager::INSTANCE.addCommand(new SetBrightnessCommand(this));
    }
#else
    void registerCommand() {}
#endif
    
#if FPP_MAJOR_VERSION >= 4
    virtual const std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request &req) override {
#else
    virtual const httpserver::http_response render_GET(const httpserver::http_request &req) override {
#endif
        
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
#if FPP_MAJOR_VERSION >= 4
        return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(v, 200));
#else
        return httpserver::http_response_builder(v, 200);
#endif
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
                if (getFPPmode() == MASTER_MODE) multiSync->SendPluginData(name, (uint8_t*)data.c_str(), dlen);
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
                if (getFPPmode() == MASTER_MODE) multiSync->SendPluginData(name, (uint8_t*)data.c_str(), dlen);
            }
        } else if (p1 != "") {
            int i = std::stoi(p1);
            setBrightness(i);
            startFadeTime = -1;
        }
    }

    void registerApis(httpserver::webserver *m_ws) override {
        m_ws->register_resource("/Brightness", this, true);
        
        std::function<void(const std::string &topic, const std::string &payload)> f = [this](const std::string &topic, const std::string &payload){
            multiSyncData((uint8_t*)payload.c_str(), payload.size() + 1);
        };
        if (mqtt) {
            mqtt->AddCallback("/Brightness", f);
        }
    }
    
    virtual void modifyChannelData(int ms, uint8_t *seqData) override {
        if (startFadeTime != -1) {
            if (ms >= endFadeTime) {
                startFadeTime = -1;
                setBrightness(endFadeBrightness, false);
            } else {
                float f = (ms - startFadeTime)/(endFadeTime - startFadeTime);
                float newb = f * (endFadeBrightness - startFadeBrightness);
                newb = startFadeBrightness + newb;
                setBrightness(newb, false);
            }
        }
        for (auto &a : GetOutputRanges()) {
            int len = a.second;
            for (int x = 0, start = a.first; x < len; x++, start++) {
                seqData[start] = map[seqData[start]];
            }
        }
        lastms = ms;
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
            SaveJsonToFile(val, "/home/fpp/media/config/plugin.fpp-brightness.json");
        }
        if (sendSync && getFPPmode() == MASTER_MODE) {
            std::string s = std::to_string(i);
            int len = s.size() + 1;
            multiSync->SendPluginData(name, (uint8_t*)s.c_str(), len);
        }
    }
    
    int startFadeTime = -1;
    int endFadeTime = 0;
    int startFadeBrightness = 0;
    int endFadeBrightness = 0;
    
    int brightness = -1;
    int lastms = 0;
    uint8_t map[256];
};


extern "C" {
    FPPPlugin *createPlugin() {
        return new FPPBrightnessPlugin();
    }
}
