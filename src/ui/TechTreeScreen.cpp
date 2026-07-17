#include "TechTreeScreen.h"

#include "mechanics/GameState.h"
#include "mechanics/Player.h"
#include "core/Logger.h"

#include <genie/dat/Research.h>
#include <genie/dat/Unit.h>

#include <algorithm>
#include <set>

// Well-known building IDs in AoE2
static const std::vector<std::pair<int16_t, const char*>> s_buildingOrder = {
    {109, "Town Center"},
    {68,  "Barracks"},
    {101, "Archery Range"},
    {20,  "Stable"},
    {49,  "Siege Workshop"},
    {82,  "Castle"},
    {104, "Monastery"},
    {87,  "Blacksmith"},
    {209, "University"},
    {12,  "Market"},
    {45,  "Dock"},
    {103, "Mill"},
    {562, "Lumber Camp"},
    {584, "Mining Camp"},
};

// Age tech IDs: 22=Feudal, 23=Castle, 24=Imperial
static int estimateAge(const genie::Tech &tech)
{
    for (size_t i = 0; i < tech.RequiredTechs.size(); i++) {
        int req = tech.RequiredTechs[i];
        if (req == 24) return 3; // Imperial
        if (req == 23) return 2; // Castle
        if (req == 22) return 1; // Feudal
    }
    // If no age requirement found, assume Dark Age
    return 0;
}

static std::string cleanName(const std::string &raw)
{
    std::string s = raw;
    s.erase(std::find(s.begin(), s.end(), '\0'), s.end());
    return s;
}

TechTreeScreen::TechTreeScreen(const std::shared_ptr<IRenderTarget> &renderTarget)
    : m_renderTarget(renderTarget)
{
    m_titleText = renderTarget->createText(Drawable::Text::UI);
    m_titleText->pointSize = 22;
    m_titleText->color = Drawable::Color(255, 220, 150, 255);

    m_itemText = renderTarget->createText(Drawable::Text::Plain);
    m_itemText->pointSize = 12;

    m_tooltipText = renderTarget->createText(Drawable::Text::Plain);
    m_tooltipText->pointSize = 11;
    m_tooltipText->color = Drawable::Color(255, 255, 220, 255);
}

void TechTreeScreen::show(const std::shared_ptr<GameState> &state)
{
    m_state = state;
    m_visible = true;
    m_scrollY = 0;
    m_tooltip.active = false;
    buildGrid();
}

void TechTreeScreen::hide()
{
    m_visible = false;
    m_tooltip.active = false;
}

void TechTreeScreen::buildGrid()
{
    m_rows.clear();
    m_cellRects.clear();
    if (!m_state) return;

    const Player::Ptr &human = m_state->humanPlayer();
    if (!human) return;

    const auto &civTechs = human->civilization.availableTechs();

    // Map: buildingId -> row index
    std::map<int16_t, size_t> buildingIndex;

    // Create rows for known buildings in order
    for (const auto &[bId, bName] : s_buildingOrder) {
        BuildingRow row;
        row.buildingId = bId;
        // Try to get name from unit data, fallback to hardcoded
        const genie::Unit &bUnit = human->civilization.unitData(bId);
        std::string uName = cleanName(bUnit.Name);
        row.buildingName = uName.empty() ? bName : uName;
        buildingIndex[bId] = m_rows.size();
        m_rows.push_back(std::move(row));
    }

    // "Other" row for techs without a known building
    BuildingRow otherRow;
    otherRow.buildingId = -1;
    otherRow.buildingName = "Other";
    size_t otherIdx = m_rows.size();
    m_rows.push_back(std::move(otherRow));

    // Populate techs
    for (const auto &[techId, tech] : civTechs) {
        std::string name = cleanName(tech.Name);
        if (name.empty()) continue;
        if (tech.ResearchTime < 0) continue;

        CellEntry entry;
        entry.name = name;
        entry.techId = techId;
        entry.age = estimateAge(tech);
        entry.available = human->researchAvailable(techId);
        entry.researched = human->hasResearched(techId);
        entry.isUnit = false;
        entry.researchTime = tech.ResearchTime;

        // Extract costs
        for (const auto &cost : tech.ResourceCosts) {
            if (!cost.Paid) continue;
            switch (cost.Type) {
            case 0: entry.costFood = cost.Amount; break;
            case 1: entry.costWood = cost.Amount; break;
            case 2: entry.costStone = cost.Amount; break;
            case 3: entry.costGold = cost.Amount; break;
            default: break;
            }
        }

        int16_t loc = tech.ResearchLocation;
        auto it = buildingIndex.find(loc);
        size_t rowIdx = (it != buildingIndex.end()) ? it->second : otherIdx;
        int age = std::clamp(entry.age, 0, 3);
        m_rows[rowIdx].entries[age].push_back(std::move(entry));
    }

    // Sort entries within each cell by name
    for (auto &row : m_rows) {
        for (int a = 0; a < 4; a++) {
            std::sort(row.entries[a].begin(), row.entries[a].end(),
                [](const CellEntry &a, const CellEntry &b) { return a.name < b.name; });
        }
    }

    // Remove empty rows
    m_rows.erase(std::remove_if(m_rows.begin(), m_rows.end(),
        [](const BuildingRow &r) {
            return r.entries[0].empty() && r.entries[1].empty() &&
                   r.entries[2].empty() && r.entries[3].empty();
        }), m_rows.end());
}

void TechTreeScreen::render()
{
    if (!m_visible) return;

    m_cellRects.clear();

    Size screenSize = m_renderTarget->getSize();

    // Full screen overlay
    m_renderTarget->draw(ScreenRect(0, 0, screenSize.width, screenSize.height),
        Drawable::Color(0, 0, 0, 200));

    // Panel
    const float panelX = 20;
    const float panelY = 20;
    const float panelW = screenSize.width - 40;
    const float panelH = screenSize.height - 40;

    m_renderTarget->draw(ScreenRect(panelX, panelY, panelW, panelH),
        Drawable::Color(30, 22, 10, 245));
    m_renderTarget->draw(ScreenRect(panelX, panelY, panelW, panelH),
        Drawable::Transparent, Drawable::Color(100, 80, 40, 255));

    // Title
    if (m_state && m_state->humanPlayer()) {
        m_titleText->string = "Technology Tree - " + m_state->humanPlayer()->civilization.name();
    } else {
        m_titleText->string = "Technology Tree";
    }
    m_titleText->position = ScreenPos(panelX + 15, panelY + 10);
    m_renderTarget->draw(m_titleText);

    // Close button
    const float closeX = panelX + panelW - 35;
    const float closeY = panelY + 8;
    m_renderTarget->draw(ScreenRect(closeX, closeY, 28, 28),
        Drawable::Color(80, 30, 30, 200));
    auto closeText = m_renderTarget->createText(Drawable::Text::Plain);
    closeText->string = "X";
    closeText->pointSize = 16;
    closeText->color = Drawable::White;
    closeText->position = ScreenPos(closeX + 7, closeY + 4);
    m_renderTarget->draw(closeText);

    // Layout constants
    const float headerH = 68;          // Space for title + age headers
    const float labelW = 110;          // Building name column width
    const float colW = (panelW - labelW - 20) / 4.f;
    const float entryH = 16;           // Height per tech entry line
    const float rowPadding = 6;        // Padding between building rows
    const float contentTop = panelY + headerH;
    const float contentBottom = panelY + panelH - 10;

    // Age column headers
    static const char *ageNames[] = {"Dark Age", "Feudal Age", "Castle Age", "Imperial Age"};
    for (int age = 0; age < 4; age++) {
        float cx = panelX + labelW + 10 + age * colW;
        m_renderTarget->draw(ScreenRect(cx, panelY + 42, colW - 4, 22),
            Drawable::Color(50, 38, 18, 220));
        auto headerText = m_renderTarget->createText(Drawable::Text::Plain);
        headerText->string = ageNames[age];
        headerText->pointSize = 13;
        headerText->color = Drawable::Color(220, 200, 150, 255);
        headerText->position = ScreenPos(cx + 5, panelY + 45);
        m_renderTarget->draw(headerText);
    }

    // Building label header
    m_renderTarget->draw(ScreenRect(panelX + 10, panelY + 42, labelW - 4, 22),
        Drawable::Color(50, 38, 18, 220));
    auto bldgHeader = m_renderTarget->createText(Drawable::Text::Plain);
    bldgHeader->string = "Building";
    bldgHeader->pointSize = 13;
    bldgHeader->color = Drawable::Color(220, 200, 150, 255);
    bldgHeader->position = ScreenPos(panelX + 15, panelY + 45);
    m_renderTarget->draw(bldgHeader);

    // Draw rows with scrolling
    float curY = contentTop + m_scrollY;

    // Compute total content height for scroll clamping
    m_contentHeight = 0;
    for (const auto &row : m_rows) {
        int maxEntries = 0;
        for (int a = 0; a < 4; a++) {
            maxEntries = std::max(maxEntries, (int)row.entries[a].size());
        }
        m_contentHeight += std::max(1, maxEntries) * entryH + rowPadding + 2;
    }

    for (const auto &row : m_rows) {
        // Compute row height = max entries across all 4 age columns
        int maxEntries = 0;
        for (int a = 0; a < 4; a++) {
            maxEntries = std::max(maxEntries, (int)row.entries[a].size());
        }
        float rowH = std::max(1, maxEntries) * entryH + rowPadding;

        // Skip rows fully above or below visible area
        if (curY + rowH < contentTop) {
            curY += rowH + 2;
            continue;
        }
        if (curY > contentBottom) {
            curY += rowH + 2;
            continue;
        }

        // Row separator line
        if (curY >= contentTop) {
            m_renderTarget->draw(ScreenRect(panelX + 10, curY, panelW - 20, 1),
                Drawable::Color(60, 50, 30, 150));
        }

        // Building name (vertically centered in row)
        float labelY = curY + rowH / 2 - 7;
        if (labelY >= contentTop && labelY < contentBottom) {
            m_itemText->string = row.buildingName;
            m_itemText->color = Drawable::Color(200, 180, 120, 255);
            m_itemText->position = ScreenPos(panelX + 12, labelY);
            m_renderTarget->draw(m_itemText);
        }

        // Draw entries per age column
        for (int age = 0; age < 4; age++) {
            float cx = panelX + labelW + 10 + age * colW;
            for (size_t i = 0; i < row.entries[age].size(); i++) {
                const CellEntry &entry = row.entries[age][i];
                float ey = curY + 2 + i * entryH;

                if (ey < contentTop || ey + entryH > contentBottom) continue;

                // Status indicator dot
                Drawable::Color dotColor;
                Drawable::Color textColor;
                if (entry.researched) {
                    dotColor = Drawable::Color(60, 180, 60, 255);
                    textColor = Drawable::Color(100, 220, 100, 255);
                } else if (entry.available) {
                    dotColor = Drawable::Color(200, 200, 200, 255);
                    textColor = Drawable::Color(220, 220, 220, 255);
                } else {
                    dotColor = Drawable::Color(80, 65, 50, 180);
                    textColor = Drawable::Color(120, 100, 80, 180);
                }

                // Small status square
                m_renderTarget->draw(ScreenRect(cx + 2, ey + 3, 8, 8), dotColor);

                // Tech name
                m_itemText->string = entry.name;
                m_itemText->color = textColor;
                m_itemText->position = ScreenPos(cx + 14, ey);
                m_renderTarget->draw(m_itemText);

                // Store cell rect for hover tooltip
                m_cellRects.push_back({ScreenRect(cx, ey, colW - 4, entryH), &entry});
            }
        }

        curY += rowH + 2;
    }

    // Scroll indicator (if content overflows)
    float visibleH = contentBottom - contentTop;
    if (m_contentHeight > visibleH) {
        float scrollBarH = std::max(20.f, visibleH * visibleH / m_contentHeight);
        float scrollRange = m_contentHeight - visibleH;
        float scrollBarY = contentTop + (-m_scrollY / scrollRange) * (visibleH - scrollBarH);
        m_renderTarget->draw(ScreenRect(panelX + panelW - 14, scrollBarY, 6, scrollBarH),
            Drawable::Color(100, 80, 50, 150));
    }

    // Tooltip
    if (m_tooltip.active) {
        // Draw tooltip background
        float tw = 220;
        float th = 50;
        float tx = m_tooltip.x + 12;
        float ty = m_tooltip.y - 10;
        // Keep on screen
        if (tx + tw > screenSize.width - 5) tx = m_tooltip.x - tw - 5;
        if (ty + th > screenSize.height - 5) ty = screenSize.height - th - 5;
        if (ty < 5) ty = 5;

        m_renderTarget->draw(ScreenRect(tx, ty, tw, th),
            Drawable::Color(20, 15, 5, 240));
        m_renderTarget->draw(ScreenRect(tx, ty, tw, th),
            Drawable::Transparent, Drawable::Color(120, 100, 60, 255));

        m_tooltipText->string = m_tooltip.text;
        m_tooltipText->position = ScreenPos(tx + 6, ty + 4);
        m_renderTarget->draw(m_tooltipText);
    }
}

bool TechTreeScreen::handleEvent(const input::Event &event)
{
    if (!m_visible) return false;

    ScreenPos pos;
    bool isTap = false;

    if (event.type == input::Event::TouchEnded || event.type == input::Event::MouseButtonReleased) {
        pos = (event.type == input::Event::TouchEnded)
            ? ScreenPos(event.touch.x, event.touch.y)
            : ScreenPos(event.mouseButton.x, event.mouseButton.y);
        isTap = true;
    }

    // Mouse move -> update tooltip
    if (event.type == input::Event::MouseMoved) {
        m_mouseX = event.mouseMove.x;
        m_mouseY = event.mouseMove.y;
        m_tooltip.active = false;

        for (const auto &cell : m_cellRects) {
            if (cell.rect.contains(ScreenPos(m_mouseX, m_mouseY)) && cell.entry) {
                m_tooltip.active = true;
                m_tooltip.x = m_mouseX;
                m_tooltip.y = m_mouseY;

                // Build tooltip text
                std::string tt = cell.entry->name;
                if (cell.entry->researched) tt += "  [DONE]";
                else if (cell.entry->available) tt += "  [Available]";
                else tt += "  [Locked]";

                // Cost line
                std::string costLine;
                if (cell.entry->costFood > 0) costLine += "F:" + std::to_string(cell.entry->costFood) + " ";
                if (cell.entry->costWood > 0) costLine += "W:" + std::to_string(cell.entry->costWood) + " ";
                if (cell.entry->costGold > 0) costLine += "G:" + std::to_string(cell.entry->costGold) + " ";
                if (cell.entry->costStone > 0) costLine += "S:" + std::to_string(cell.entry->costStone) + " ";
                if (cell.entry->researchTime > 0) costLine += "T:" + std::to_string(cell.entry->researchTime) + "s";
                if (!costLine.empty()) tt += "\n" + costLine;

                m_tooltip.text = tt;
                break;
            }
        }
        return true;
    }

    // Escape closes the screen
    if (event.type == input::Event::KeyPressed && event.key.code == input::Key::Escape) {
        hide();
        return true;
    }

    // Scroll with mouse wheel
    if (event.type == input::Event::MouseWheelScrolled) {
        Size screenSize = m_renderTarget->getSize();
        float visibleH = screenSize.height - 40 - 10 - 68; // panelH - bottom pad - headerH
        float maxScroll = std::max(0.f, m_contentHeight - visibleH);
        m_scrollY += event.mouseWheel.delta * 30.f;
        m_scrollY = std::clamp(m_scrollY, -maxScroll, 0.f);
        return true;
    }

    // Track touch start for scrolling
    if (event.type == input::Event::TouchBegan) {
        m_lastTouchY = event.touch.y;
        return true;
    }

    // Touch drag for scrolling
    if (event.type == input::Event::TouchMoved) {
        Size screenSize = m_renderTarget->getSize();
        float visibleH = screenSize.height - 40 - 10 - 68;
        float maxScroll = std::max(0.f, m_contentHeight - visibleH);
        float dy = event.touch.y - m_lastTouchY;
        m_lastTouchY = event.touch.y;
        m_scrollY += dy;
        m_scrollY = std::clamp(m_scrollY, -maxScroll, 0.f);
        return true;
    }

    if (!isTap) return true; // consume all events

    Size screenSize = m_renderTarget->getSize();
    float panelX = 20;
    float panelY = 20;
    float panelW = screenSize.width - 40;

    // Close button
    if (ScreenRect(panelX + panelW - 35, panelY + 8, 28, 28).contains(pos)) {
        hide();
        return true;
    }

    // Tap outside panel = close
    if (pos.x < panelX || pos.x > panelX + panelW ||
        pos.y < panelY || pos.y > panelY + screenSize.height - 40) {
        hide();
        return true;
    }

    return true;
}
