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

#include "fpphttp.h"
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

class FPPBrightnessPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelDataPlugin, public FPPPlugins::APIProviderPlugin
{
public:
    // The "true" asks FPP to watch config/plugin.fpp-brightness and call
    // settingChanged() below, so editing the exclude ranges no longer needs an
    // fppd restart.
    FPPBrightnessPlugin() : FPPPlugins::Plugin("fpp-brightness", true), FPPPlugins::ChannelDataPlugin(), FPPPlugins::APIProviderPlugin()
    {
        int startBrightness = 100;
        configLocation = FPP_DIR_CONFIG("/plugin.fpp-brightness.json");
        if (FileExists(configLocation))
        {
            Json::Value root;
            if (LoadJsonFromFile(configLocation, root))
            {
                if (root.isMember("brightness"))
                {
                    startBrightness = root["brightness"].asInt();
                }
            }
        }
        setBrightness(startBrightness, false);
        registerCommand();
    }
    virtual ~FPPBrightnessPlugin() {}

    class SetBrightnessCommand : public Command
    {
    public:
        SetBrightnessCommand(FPPBrightnessPlugin *p) : Command("Brightness"), plugin(p)
        {
            args.push_back(CommandArg("brightness", "int", "Brightness").setRange(0, 200).setDefaultValue("100"));
        }

        virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override
        {
            int brightness = 100;
            if (args.size() >= 1)
            {
                brightness = std::atoi(args[0].c_str());
            }
            if (brightness > 200)
            {
                return std::make_unique<Command::ErrorResult>("Brightness cannot be set above 200");
            }
            if (brightness < 0)
            {
                return std::make_unique<Command::ErrorResult>("Brightness cannot be set below 0");
            }
            plugin->resetFade();
            plugin->setBrightness(brightness, false);
            return std::make_unique<Command::Result>("Brightness Set");
        }
        FPPBrightnessPlugin *plugin;
    };
    class AdjustBrightnessCommand : public Command
    {
    public:
        AdjustBrightnessCommand(FPPBrightnessPlugin *p) : Command("Brightness Adjust"), plugin(p)
        {
            args.push_back(CommandArg("brightness", "int", "Brightness").setRange(-100, 100).setDefaultValue("0"));
        }

        virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override
        {
            int adjust = 0;
            if (args.size() >= 1)
            {
                adjust = std::stoi(args[0]);
            }
            if (plugin->brightness + adjust > 200)
            {
                return std::make_unique<Command::ErrorResult>("Brightness cannot be adjusted above 200");
            }
            if (plugin->brightness + adjust < 0)
            {
                return std::make_unique<Command::ErrorResult>("Brightness cannot be adjusted below 0");
            }
            plugin->resetFade();
            plugin->setBrightness(plugin->brightness + adjust, false);
            return std::make_unique<Command::Result>("Brightness Adjusted");
        }
        FPPBrightnessPlugin *plugin;
    };
    class FadeBrightnessCommand : public Command
    {
    public:
        FadeBrightnessCommand(FPPBrightnessPlugin *p) : Command("Brightness Fade"), plugin(p)
        {
            args.push_back(CommandArg("brightness", "int", "End brightness").setRange(0, 200).setDefaultValue("100"));
            args.push_back(CommandArg("duration", "int", "Duration (seconds)").setRange(0, 14400).setDefaultValue("60"));
        }

        virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override
        {
            int newBrightness = 100;
            int duration = 60;
            if (args.size() >= 1)
            {
                newBrightness = std::stoi(args[0]);
            }
            if (args.size() >= 2)
            {
                duration = std::stoi(args[1]);
            }
            if (newBrightness > 200)
            {
                return std::make_unique<Command::ErrorResult>("Brightness cannot be faded above 200");
            }
            if (newBrightness < 0)
            {
                return std::make_unique<Command::ErrorResult>("Brightness cannot be faded below 0");
            }
            plugin->fade(newBrightness, duration);
            return std::make_unique<Command::Result>("Brightness Fade");
        }

        FPPBrightnessPlugin *plugin;
    };

    void registerCommand()
    {
        addOwnedCommand(new SetBrightnessCommand(this));
        addOwnedCommand(new AdjustBrightnessCommand(this));
        addOwnedCommand(new FadeBrightnessCommand(this));
    }

    // These Command subclasses are declared here, so their vtables live in this
    // plugin's .so and they hold a back-pointer to this plugin. Both are gone
    // once the plugin is unloaded, so it has to take them back itself.
    void addOwnedCommand(Command *c)
    {
        myCommands.push_back(c);
        CommandManager::INSTANCE.addCommand(c);
    }
    void removeOwnedCommands()
    {
        for (Command *c : myCommands)
        {
            // removeCommand() only unregisters - CommandManager deletes whatever
            // is still in its registry at shutdown, so taking one back means
            // owning it again.
            CommandManager::INSTANCE.removeCommand(c);
            delete c;
        }
        myCommands.clear();
    }

    // Nothing here is asynchronous, so no readiness predicate is needed.
    virtual std::function<bool()> shutdown() override
    {
        removeOwnedCommands();
        return nullptr;
    }

    void handleBrightnessRequest(const HttpRequestPtr &req,
                                 std::function<void(const HttpResponsePtr &)> &&callback)
    {
        std::string path = req->path();
        std::vector<std::string> pieces;
        std::istringstream ss(path);
        std::string piece;
        while (std::getline(ss, piece, '/'))
        {
            if (!piece.empty())
                pieces.push_back(piece);
        }

        if (pieces.size() > 1)
        {
            std::vector<std::string> vals;
            for (size_t x = 1; x < pieces.size(); x++)
            {
                vals.push_back(pieces[x]);
            }
            setBrightness(vals);
        }

        std::string v = std::to_string(brightness);
        callback(makeStringResponse(v, 200));
    }
    virtual void multiSyncData(const uint8_t *data, int len) override
    {
        std::vector<std::string> v;
        std::string s = (char *)data;
        int start = 0;
        for (int x = 0; x < len; x++)
        {
            if (s[x] == '/')
            {
                v.push_back(s.substr(start, x - start));
                start = x + 1;
            }
        }
        if (len != start)
        {
            v.push_back(s.substr(start, len - start));
        }

        for (int x = 0; x < v.size(); x++)
        {
            // some path that includes the Brightness part
            if (v[x] == "Brightness")
            {
                for (int y = 0; y <= x; y++)
                {
                    v.erase(v.begin());
                }
                x = v.size();
            }
        }
        setBrightness(v);
    }
    void setBrightness(std::vector<std::string> &vals)
    {
        while (vals.size() < 2)
        {
            vals.push_back("");
        }
        std::string p1 = vals[0];
        if (p1 == "FadeUp")
        {
            std::string p2 = vals[1];
            if (p2 != "")
            {
                int len = std::stoi(p2);
                endFadeTime = lastms + len;
                startFadeBrightness = brightness;
                endFadeBrightness = 100;
                startFadeTime = lastms;

                std::string data = p1 + "/" + p2;
                int dlen = data.size() + 1;
                if (multiSync->isMultiSyncEnabled())
                    multiSync->SendPluginData(name, (uint8_t *)data.c_str(), dlen);
            }
        }
        else if (p1 == "FadeDown")
        {
            std::string p2 = vals[1];
            if (p2 != "")
            {
                int len = std::stoi(p2);

                endFadeTime = lastms + len;
                startFadeBrightness = brightness;
                endFadeBrightness = 0;
                startFadeTime = lastms;

                std::string data = p1 + "/" + p2;
                int dlen = data.size() + 1;
                if (multiSync->isMultiSyncEnabled())
                    multiSync->SendPluginData(name, (uint8_t *)data.c_str(), dlen);
            }
        }
        else if (p1 != "")
        {
            int i = std::stoi(p1);
            setBrightness(i);
            startFadeTime = -1;
        }
    }
    void unregisterApis() override
    {
        // Both the handler and the Events callback are this plugin's code, so
        // they have to be gone before the library can be.
        // unregisterPluginApi() does not return until no request is inside the
        // handler and the handler itself has been destroyed.
        FPPPlugins::unregisterPluginApi("/Brightness");
        Events::RemoveCallback("/Brightness");
    }
    void registerApis() override
    {
        // Registered through FPP rather than drogon::app() directly: drogon has
        // no route removal, so a handler registered straight with it could
        // never be withdrawn and would pin this plugin in memory. family=true
        // covers /Brightness/<value> and /Brightness/FadeUp/<ms> as well.
        FPPPlugins::registerPluginApi(
            "/Brightness",
            [this](const HttpRequestPtr &req, HttpCallback &&callback)
            {
                handleBrightnessRequest(req, std::move(callback));
            },
            {drogon::Get}, true);

        std::function<void(const std::string &topic, const std::string &payload)> f = [this](const std::string &topic, const std::string &payload)
        {
            multiSyncData((uint8_t *)payload.c_str(), payload.size() + 1);
        };
        Events::AddCallback("/Brightness", f);
    }

    virtual void modifyChannelData(int ms, uint8_t *seqData) override
    {
        // We need real time, not sequence time in case a sequence is not
        // running, so use GetTimeMS here instead of passed-in ms.
        lastms = GetTimeMS();

        if (startFadeTime != -1)
        {
            if (lastms >= endFadeTime)
            {
                startFadeTime = -1;
                setBrightness(endFadeBrightness, false);
            }
            else
            {
                float f = 1.0 * (lastms - startFadeTime) / (endFadeTime - startFadeTime);
                int newb = (int)(f * (endFadeBrightness - startFadeBrightness)) + startFadeBrightness;
                setBrightness(newb, false);
            }
        }
        calcRanges();
        for (auto &a : ranges)
        {
            int len = a.second;
            for (int x = 0, start = a.first; x < len; x++, start++)
            {
                seqData[start] = map[seqData[start]];
            }
        }
    }

    std::vector<std::pair<uint32_t, uint32_t>> subtractRanges(const std::vector<std::pair<uint32_t, uint32_t>> &src, const std::vector<std::pair<uint32_t, uint32_t>> &sub)
    {
        std::vector<std::pair<uint32_t, uint32_t>> result;

        for (const auto &srcRange : src)
        {
            // Start with the full source range and progressively subtract exclude ranges
            std::vector<std::pair<uint32_t, uint32_t>> currentRanges;
            currentRanges.push_back(srcRange);

            // Process each exclude range
            for (const auto &excludeRange : sub)
            {
                std::vector<std::pair<uint32_t, uint32_t>> newRanges;

                for (const auto &curRange : currentRanges)
                {
                    // Check if exclude range overlaps with current range
                    if (excludeRange.second < curRange.first || excludeRange.first > curRange.second)
                    {
                        // No overlap - keep the current range as-is
                        newRanges.push_back(curRange);
                    }
                    else if (excludeRange.first <= curRange.first && excludeRange.second >= curRange.second)
                    {
                        // Exclude range completely covers current range - skip it (don't add to newRanges)
                    }
                    else if (excludeRange.first > curRange.first && excludeRange.second < curRange.second)
                    {
                        // Exclude range is completely within current range - split into two ranges
                        // Add the portion before the exclude (if excludeRange.first > 0)
                        if (excludeRange.first > 0)
                        {
                            newRanges.push_back(std::make_pair(curRange.first, excludeRange.first - 1));
                        }
                        // Add the portion after the exclude (if excludeRange.second < UINT32_MAX)
                        if (excludeRange.second < UINT32_MAX)
                        {
                            newRanges.push_back(std::make_pair(excludeRange.second + 1, curRange.second));
                        }
                    }
                    else if (excludeRange.first <= curRange.first)
                    {
                        // Exclude range overlaps the start - keep only the end portion
                        if (excludeRange.second < UINT32_MAX)
                        {
                            newRanges.push_back(std::make_pair(excludeRange.second + 1, curRange.second));
                        }
                    }
                    else
                    {
                        // Exclude range overlaps the end - keep only the start portion
                        if (excludeRange.first > 0)
                        {
                            newRanges.push_back(std::make_pair(curRange.first, excludeRange.first - 1));
                        }
                    }
                }

                currentRanges = newRanges;
            }

            // Add all remaining ranges to the result
            for (const auto &range : currentRanges)
            {
                if (range.first <= range.second)
                {
                    result.push_back(range);
                }
            }
        }

        return result;
    }

    void calcRanges()
    {
        if (ranges.empty())
        {
            std::vector<std::pair<uint32_t, uint32_t>> excludes;

            std::string ex = settings["BrightnessExcludeRanges"];
            if (ex != "")
            {
                std::vector<std::string> exranges = split(ex, ',');
                for (auto &r : exranges)
                {
                    size_t idx = r.find('-');
                    if (idx != std::string::npos)
                    {
                        std::string fp = r.substr(0, idx);
                        std::string ep = r.substr(idx + 1);
                        int st = std::atoi(fp.c_str());
                        if (st > 0)
                        {
                            uint32_t start = st - 1;
                            int end = std::atoi(ep.c_str()) - 1;
                            if (end > 0 && end > start)
                            {
                                excludes.emplace_back(start, end);
                            }
                        }
                    }
                    else
                    {
                        int start = std::atoi(r.c_str());
                        if (start > 0)
                        {
                            excludes.emplace_back(start - 1, start - 1);
                        }
                    }
                }
                std::vector<std::pair<uint32_t, uint32_t>> srcRanges;
                for (auto &rng : GetOutputRanges())
                {
                    uint32_t end = rng.first + rng.second - 1;
                    srcRanges.emplace_back(rng.first, end);
                }
                for (auto &a : subtractRanges(srcRanges, excludes))
                {
                    ranges.emplace_back(a.first, a.second - a.first + 1);
                }
            }
            if (ranges.empty())
            {
                ranges = GetOutputRanges();
            }
        }
    }

    void setBrightness(int i, bool sendSync = true)
    {
        if (i > 200)
        {
            i = 200;
        }
        if (i < 0)
        {
            i = 0;
        }
        if (brightness != i)
        {
            brightness = i;
            for (int x = 0; x < 256; x++)
            {
                float b = x * (float)i;
                b /= 100.0f;
                int newb = std::round(b);
                if (newb == 0 && b > 0.2)
                {
                    newb = 1;
                }
                if (newb > 255)
                {
                    newb = 255;
                }
                map[x] = newb;
            }
            Json::Value val;
            val["brightness"] = i;
            SaveJsonToFile(val, configLocation);
        }
        if (sendSync && multiSync->isMultiSyncEnabled())
        {
            std::string s = std::to_string(i);
            int len = s.size() + 1;
            multiSync->SendPluginData(name, (uint8_t *)s.c_str(), len);
        }
    }

    // Called by FPP when config/plugin.fpp-brightness changes; the base class
    // has already updated settings[key]. calcRanges() rebuilds from
    // BrightnessExcludeRanges whenever the cache is empty, so emptying it is the
    // whole job - the next output frame picks the new ranges up.
    virtual void settingChanged(const std::string &key, const std::string &value) override
    {
        if (key == "BrightnessExcludeRanges")
        {
            LogInfo(VB_PLUGIN, "Brightness: exclude ranges changed, recalculating\n");
            ranges.clear();
        }
    }

    void resetFade()
    {
        startFadeTime = -1;
    }

    void fade(int newBrightness, int duration = 60)
    {
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
    std::vector<Command *> myCommands;

    std::vector<std::pair<std::uint32_t, std::uint32_t>> ranges;
};

// Safe to dlclose() on unload: no threads, no timers, no CurlManager requests,
// no epoll descriptors and no drogon client objects, so nothing outside this
// library can still be holding a pointer into it once unregisterApis() and
// shutdown() have returned. The routes go through registerPluginApi(), the
// Events callback comes back in unregisterApis(), and the three commands are
// withdrawn and deleted in shutdown().
FPP_PLUGIN_SUPPORTS_UNLOAD()

extern "C"
{
    FPPPlugins::Plugin *createPlugin()
    {
        return new FPPBrightnessPlugin();
    }
}
