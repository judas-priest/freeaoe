#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/Types.h"
#include "render/IRenderTarget.h"
#include "render/EventTypes.h"

namespace genie {
class ScnFile;
using ScnFilePtr = std::shared_ptr<ScnFile>;
}

#ifdef USE_SDL2
struct SdlWindow;
#endif

// Fullscreen scenario browser for Android.
// Shows campaign list → tap → scenario list → tap → returns selected scenario.
class ScenarioBrowser
{
public:
    struct Entry {
        std::string name;
        std::string path;         // .cpx/.cpn file (campaigns) or .scx/.scn (standalone)
        int scenarioIndex = -1;   // -1 = campaign entry, >=0 = scenario inside campaign
        int scenarioCount = 0;
    };

    // Returns selected scenario, or nullptr if cancelled/nothing found.
    static genie::ScnFilePtr show(const std::string &campaignsPath);

private:
    ScenarioBrowser() = default;

    void scan(const std::string &campaignsPath);
    genie::ScnFilePtr run();
    void render();
    void handleEvent(const input::Event &event);
    void openCampaign(const Entry &entry);

#ifdef USE_SDL2
    std::unique_ptr<SdlWindow> m_window;
#endif
    std::shared_ptr<IRenderTarget> m_renderTarget;

    std::vector<Entry> m_entries;
    std::vector<Entry> m_campaignEntries;
    int m_scrollOffset = 0;
    bool m_done = false;
    bool m_inCampaign = false;
    std::string m_currentCampaignName;
    genie::ScnFilePtr m_result;

    Drawable::Text::Ptr m_titleText;
    Drawable::Text::Ptr m_itemText;
};
