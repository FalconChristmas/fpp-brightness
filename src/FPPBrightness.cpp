
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
#include "channeloutput/channeloutput.h"
#include "mqtt.h"
#include "MultiSync.h"

class FPPBrightnessPlugin : public FPPPlugin, public httpserver::http_resource {
public:
    
    FPPBrightnessPlugin() : FPPPlugin("fpp-brightness") {
        setBrightness(100, false);
    }
    virtual ~FPPBrightnessPlugin() {}
    
    virtual const httpserver::http_response render_GET(const httpserver::http_request &req) override {
        
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
        return httpserver::http_response_builder(v, 200);
    }
    virtual void multiSyncData(uint8_t *data, int len) {
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

    void registerApis(httpserver::webserver *m_ws) {
        m_ws->register_resource("/Brightness", this, true);
        
        std::function<void(const std::string &topic, const std::string &payload)> f = [this](const std::string &topic, const std::string &payload){
            multiSyncData((uint8_t*)payload.c_str(), payload.size() + 1);
        };
        if (mqtt) {
            mqtt->AddCallback("/Brightness", f);
        }
    }
    
    virtual void modifyChannelData(int ms, uint8_t *seqData) {
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
                seqData[x] = map[seqData[x]];
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
