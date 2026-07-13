#include "ScenarioBrowser.h"
#include "RandomMapSetup.h"

#include <filesystem>
#include <algorithm>
#include <thread>
#include <chrono>

#include <genie/script/ScnFile.h>
#include "render/SdlRenderTarget.h"
#include "core/Logger.h"
#ifdef ANDROID
#include <android/log.h>
#define ALOG(...) __android_log_print(ANDROID_LOG_INFO, "FreeAoE", __VA_ARGS__)
#else
#define ALOG(...)
#endif

namespace fs = std::filesystem;

ScenarioBrowser::Result ScenarioBrowser::show(const std::string &campaignsPath)
{
    ScenarioBrowser browser;
    browser.scan(campaignsPath);
    Result result;
    if (browser.m_entries.empty()) {
        WARN << "No campaigns found in" << campaignsPath;
        return result;
    }
    result.scenario = browser.run();
    // Check if random map was selected
    if (browser.m_randomMapResult.start) {
        result.isRandomMap = true;
        result.randomMapType = browser.m_randomMapResult.mapType;
        result.randomMapSize = browser.m_randomMapResult.mapSize;
        result.randomPlayerCount = browser.m_randomMapResult.playerCount;
    }
    return result;
}

void ScenarioBrowser::scan(const std::string &campaignsPath)
{
    m_entries.clear();

    ALOG("Scanning campaigns at: %s", campaignsPath.c_str());
    if (campaignsPath.empty() || !fs::exists(campaignsPath)) {
        ALOG("Campaigns path NOT FOUND: %s", campaignsPath.c_str());
        return;
    }
    ALOG("Path exists, scanning...");

    // Scan recursively for .cpx, .cpn, .scx, .scn files
    for (const auto &dirEntry : fs::recursive_directory_iterator(campaignsPath,
            fs::directory_options::skip_permission_denied)) {
        if (!dirEntry.is_regular_file()) continue;
        std::string ext = dirEntry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".cpx" || ext == ".cpn") {
            try {
                genie::CpxFile cpx;
                cpx.load(dirEntry.path().string());
                Entry e;
                e.name = cpx.name;
                e.name.erase(std::find(e.name.begin(), e.name.end(), '\0'), e.name.end());
                if (e.name.empty()) e.name = dirEntry.path().stem().string();
                e.path = dirEntry.path().string();
                e.scenarioCount = cpx.getFilecount();
                m_entries.push_back(std::move(e));
            } catch (const std::exception &ex) {
                WARN << "Failed to load campaign" << dirEntry.path().string() << ex.what();
            }
        } else if (ext == ".scx" || ext == ".scn") {
            Entry e;
            e.name = dirEntry.path().stem().string();
            e.path = dirEntry.path().string();
            e.scenarioIndex = 0;
            m_entries.push_back(std::move(e));
        }
    }

    ALOG("Found %zu campaign/scenario entries", m_entries.size());
    std::sort(m_entries.begin(), m_entries.end(),
        [](const Entry &a, const Entry &b) { return a.name < b.name; });

    // Add "Random Map" as first entry (special, scenarioIndex = -99)
    Entry randomEntry;
    randomEntry.name = ">> Random Map <<";
    randomEntry.path = "__RANDOM__";
    randomEntry.scenarioIndex = -99;
    m_entries.insert(m_entries.begin(), randomEntry);

    m_campaignEntries = m_entries;
}

void ScenarioBrowser::openCampaign(const Entry &entry)
{
    m_entries.clear();
    m_inCampaign = true;
    m_currentCampaignName = entry.name;
    m_scrollOffset = 0;

    try {
        genie::CpxFile cpx;
        cpx.load(entry.path);
        auto filenames = cpx.getFilenames();
        for (size_t i = 0; i < filenames.size(); i++) {
            Entry e;
            std::string fname = filenames[i];
            auto dotPos = fname.rfind('.');
            if (dotPos != std::string::npos) fname = fname.substr(0, dotPos);
            e.name = std::to_string(i + 1) + ". " + fname;
            e.path = entry.path;
            e.scenarioIndex = static_cast<int>(i);
            m_entries.push_back(std::move(e));
        }
    } catch (const std::exception &ex) {
        WARN << "Failed to open campaign" << entry.path << ex.what();
        m_entries = m_campaignEntries;
        m_inCampaign = false;
    }
}

genie::ScnFilePtr ScenarioBrowser::run()
{
#ifdef USE_SDL2
    // Get screen size
    SDL_DisplayMode dm;
    if (SDL_GetCurrentDisplayMode(0, &dm) != 0) {
        ALOG("SDL_GetCurrentDisplayMode failed: %s", SDL_GetError());
        dm.w = 1280; dm.h = 720;
    }
    int sw = dm.w, sh = dm.h;
    ALOG("Display mode: %dx%d", sw, sh);
    if (sh > sw) std::swap(sw, sh);

    m_window = std::make_unique<SdlWindow>(Size(sw, sh), "freeaoe");
    SDL_SetWindowFullscreen(m_window->sdlWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);

    // Set logical size for consistent coordinates
    int logicalW = 1280;
    int logicalH = logicalW * sh / sw;
    SDL_RenderSetLogicalSize(m_window->sdlRenderer, logicalW, logicalH);

    // Non-owning shared_ptr to window's render target
    m_renderTarget = std::shared_ptr<IRenderTarget>(
        m_window->renderTarget.get(), [](IRenderTarget*){});

    // Force the render target size to match logical size
    auto *sdlRT = static_cast<SdlRenderTarget*>(m_renderTarget.get());
    sdlRT->setSize(Size(logicalW, logicalH));

    m_titleText = m_renderTarget->createText(Drawable::Text::UI);
    m_titleText->pointSize = 28;
    m_titleText->color = Drawable::Color(255, 220, 150, 255);

    m_itemText = m_renderTarget->createText(Drawable::Text::Plain);
    m_itemText->pointSize = 20;

    while (m_window->isOpen() && !m_done) {
        input::Event event;
        while (m_window->pollEvent(event)) {
            if (event.type == input::Event::Closed) {
                m_window->close();
                return nullptr;
            }
            handleEvent(event);
        }
        render();
        m_window->display();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    m_window.reset();
#endif
    return m_result;
}

void ScenarioBrowser::render()
{
    m_renderTarget->clear(Drawable::Color(30, 20, 10, 255));

    Size screenSize = m_renderTarget->getSize();

    float y = 30;

    // Title
    m_titleText->string = m_inCampaign ? m_currentCampaignName : "Select Campaign";
    m_titleText->position = ScreenPos(30, y);
    m_renderTarget->draw(m_titleText);
    y += 50;

    // Back button
    if (m_inCampaign) {
        m_itemText->string = "< Back";
        m_itemText->color = Drawable::Color(150, 150, 150, 255);
        m_itemText->position = ScreenPos(30, y);
        m_renderTarget->draw(m_itemText);
        y += 35;
    }

    // Separator
    m_renderTarget->draw(ScreenRect(30, y, screenSize.width - 60, 1),
                         Drawable::Color(100, 80, 60, 255));
    y += 15;

    // List
    const float itemHeight = 40;
    const float listStartY = y;
    const int visibleCount = static_cast<int>((screenSize.height - y - 20) / itemHeight);

    for (int i = m_scrollOffset;
         i < static_cast<int>(m_entries.size()) && i < m_scrollOffset + visibleCount;
         i++) {
        const Entry &entry = m_entries[i];

        // Highlight background
        m_renderTarget->draw(ScreenRect(25, y - 2, screenSize.width - 50, itemHeight - 4),
                             Drawable::Color(50, 35, 20, 255));

        m_itemText->color = Drawable::White;

        std::string label = entry.name;
        if (entry.scenarioIndex == -1 && entry.scenarioCount > 0) {
            label += "  (" + std::to_string(entry.scenarioCount) + ")";
        }
        m_itemText->string = label;
        m_itemText->position = ScreenPos(40, y + 5);
        m_renderTarget->draw(m_itemText);

        y += itemHeight;
    }

    // Scroll indicator
    if (static_cast<int>(m_entries.size()) > visibleCount) {
        float scrollRatio = static_cast<float>(m_scrollOffset) /
            std::max(1, static_cast<int>(m_entries.size()) - visibleCount);
        float barH = 40;
        float barY = listStartY + scrollRatio * (screenSize.height - listStartY - barH - 20);
        m_renderTarget->draw(ScreenRect(screenSize.width - 15, barY, 8, barH),
                             Drawable::Color(100, 80, 60, 200));
    }
}

void ScenarioBrowser::handleEvent(const input::Event &event)
{
    // Handle touch release (tap)
    if (event.type == input::Event::TouchEnded || event.type == input::Event::MouseButtonReleased) {
        float x, y;
        if (event.type == input::Event::TouchEnded) {
            x = event.touch.x;
            y = event.touch.y;
        } else {
            x = event.mouseButton.x;
            y = event.mouseButton.y;
        }

        float listStartY = m_inCampaign ? 130 : 95;

        // Back button
        if (m_inCampaign && y >= 80 && y < 115) {
            m_entries = m_campaignEntries;
            m_inCampaign = false;
            m_scrollOffset = 0;
            return;
        }

        // List item tap
        if (y >= listStartY) {
            int index = m_scrollOffset + static_cast<int>((y - listStartY) / 40);
            if (index >= 0 && index < static_cast<int>(m_entries.size())) {
                const Entry &entry = m_entries[index];
                if (entry.scenarioIndex == -99) {
                    // Random Map — show setup screen
                    auto rmResult = RandomMapSetup::show(m_window.get(), m_renderTarget);
                    if (rmResult.start) {
                        m_randomMapResult = rmResult;
                        m_done = true;
                    }
                } else if (entry.scenarioIndex == -1) {
                    openCampaign(entry);
                } else {
                    try {
                        if (entry.path.find(".cpx") != std::string::npos ||
                            entry.path.find(".cpn") != std::string::npos ||
                            entry.path.find(".CPX") != std::string::npos ||
                            entry.path.find(".CPN") != std::string::npos) {
                            genie::CpxFile cpx;
                            cpx.load(entry.path);
                            m_result = cpx.getScnFile(entry.scenarioIndex);
                        } else {
                            m_result = std::make_shared<genie::ScnFile>();
                            m_result->load(entry.path);
                        }
                        m_done = true;
                    } catch (const std::exception &ex) {
                        WARN << "Failed to load scenario" << entry.path << ":" << ex.what();
                    }
                }
            }
        }
    }

    // Scroll with touch drag
    if (event.type == input::Event::TouchMoved) {
        static float prevY = 0;
        float dy = event.touch.y - prevY;
        prevY = event.touch.y;
        if (std::abs(dy) > 3) {
            int maxScroll = std::max(0, static_cast<int>(m_entries.size()) - 5);
            if (dy < 0 && m_scrollOffset < maxScroll) m_scrollOffset++;
            if (dy > 0 && m_scrollOffset > 0) m_scrollOffset--;
        }
    }
    if (event.type == input::Event::TouchBegan) {
        // Reset scroll tracking
    }
}
