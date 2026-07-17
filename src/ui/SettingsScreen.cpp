#include "SettingsScreen.h"

#include "global/Config.h"
#include "core/Logger.h"

#include <cmath>
#include <algorithm>

SettingsScreen::SettingsScreen(const std::shared_ptr<IRenderTarget> &renderTarget)
    : m_renderTarget(renderTarget)
{
    m_titleText = renderTarget->createText(Drawable::Text::UI);
    m_titleText->pointSize = 24;
    m_titleText->color = Drawable::Color(255, 220, 150, 255);

    m_labelText = renderTarget->createText(Drawable::Text::Plain);
    m_labelText->pointSize = 16;
}

void SettingsScreen::show()
{
    m_visible = true;
    loadSettings();
    if (m_engineGameSpeed) {
        m_gameSpeed = *m_engineGameSpeed;
    }
}

void SettingsScreen::hide()
{
    saveSettings();
    if (m_engineGameSpeed) {
        *m_engineGameSpeed = m_gameSpeed;
    }
    m_visible = false;
}

void SettingsScreen::loadSettings()
{
    std::string sv = Config::Inst().getValue(Config::SoundVolume);
    std::string mv = Config::Inst().getValue(Config::MusicVolume);
    m_soundVolume = sv.empty() ? 0.5f : std::stof(sv) / 100.f;
    m_musicVolume = mv.empty() ? 0.5f : std::stof(mv) / 100.f;
    m_gameSpeed = 1.0f; // No persistent game speed setting

    m_soundVolume = std::clamp(m_soundVolume, 0.f, 1.f);
    m_musicVolume = std::clamp(m_musicVolume, 0.f, 1.f);
}

void SettingsScreen::saveSettings()
{
    Config::Inst().setValue(Config::SoundVolume, std::to_string(int(m_soundVolume * 100)));
    Config::Inst().setValue(Config::MusicVolume, std::to_string(int(m_musicVolume * 100)));
}

void SettingsScreen::drawSlider(const Slider &slider, const std::string &label, const std::string &valueStr)
{
    // Label
    m_labelText->string = label;
    m_labelText->color = Drawable::Color(220, 200, 160, 255);
    m_labelText->position = ScreenPos(slider.x, slider.y - 22);
    m_renderTarget->draw(m_labelText);

    // Value text
    m_labelText->string = valueStr;
    m_labelText->position = ScreenPos(slider.x + slider.width + 10, slider.y - 2);
    m_renderTarget->draw(m_labelText);

    // Track background
    m_renderTarget->draw(ScreenRect(slider.x, slider.y + 4, slider.width, 12),
        Drawable::Color(25, 18, 8, 220));

    // Fill
    float fillW = slider.width * slider.value;
    m_renderTarget->draw(ScreenRect(slider.x, slider.y + 4, fillW, 12),
        Drawable::Color(120, 90, 40, 255));

    // Handle
    float handleX = slider.x + fillW - 8;
    m_renderTarget->draw(ScreenRect(handleX, slider.y, 16, 20),
        Drawable::Color(200, 170, 100, 255));
}

bool SettingsScreen::handleSliderDrag(Slider &slider, float touchX, float touchY)
{
    if (touchY >= slider.y - 10 && touchY <= slider.y + slider.height + 10 &&
        touchX >= slider.x - 10 && touchX <= slider.x + slider.width + 10) {
        slider.value = std::clamp((touchX - slider.x) / slider.width, 0.f, 1.f);
        return true;
    }
    return false;
}

void SettingsScreen::render()
{
    if (!m_visible) return;

    Size screenSize = m_renderTarget->getSize();

    // Overlay
    m_renderTarget->draw(ScreenRect(0, 0, screenSize.width, screenSize.height),
        Drawable::Color(0, 0, 0, 180));

    // Panel
    float panelW = std::min(500.f, screenSize.width - 40.f);
    float panelH = std::min(350.f, screenSize.height - 80.f);
    float panelX = (screenSize.width - panelW) / 2;
    float panelY = (screenSize.height - panelH) / 2;

    m_renderTarget->draw(ScreenRect(panelX, panelY, panelW, panelH),
        Drawable::Color(35, 25, 12, 245));
    m_renderTarget->draw(ScreenRect(panelX, panelY, panelW, panelH),
        Drawable::Transparent, Drawable::Color(100, 80, 40, 255));

    // Title
    m_titleText->string = "Settings";
    m_titleText->position = ScreenPos(panelX + 20, panelY + 15);
    m_renderTarget->draw(m_titleText);

    // Close button
    float closeX = panelX + panelW - 40;
    float closeY = panelY + 10;
    m_renderTarget->draw(ScreenRect(closeX, closeY, 30, 30),
        Drawable::Color(80, 30, 30, 200));
    auto closeText = m_renderTarget->createText(Drawable::Text::Plain);
    closeText->string = "X";
    closeText->pointSize = 18;
    closeText->color = Drawable::White;
    closeText->position = ScreenPos(closeX + 8, closeY + 4);
    m_renderTarget->draw(closeText);

    // Sliders
    float sliderX = panelX + 30;
    float sliderW = panelW - 120;
    float sliderH = 20;

    m_soundSlider = {sliderX, panelY + 80, sliderW, sliderH, m_soundVolume};
    drawSlider(m_soundSlider, "Sound Volume", std::to_string(int(m_soundVolume * 100)) + "%");

    m_musicSlider = {sliderX, panelY + 150, sliderW, sliderH, m_musicVolume};
    drawSlider(m_musicSlider, "Music Volume", std::to_string(int(m_musicVolume * 100)) + "%");

    m_speedSlider = {sliderX, panelY + 220, sliderW, sliderH, m_gameSpeed / 2.f}; // 0-2x mapped to 0-1
    std::string speedStr = std::to_string(m_gameSpeed).substr(0, 3) + "x";
    drawSlider(m_speedSlider, "Game Speed", speedStr);

    // Done button
    float doneX = panelX + panelW / 2 - 50;
    float doneY = panelY + panelH - 50;
    m_renderTarget->draw(ScreenRect(doneX, doneY, 100, 36),
        Drawable::Color(60, 45, 20, 230));
    auto doneText = m_renderTarget->createText(Drawable::Text::Plain);
    doneText->string = "Done";
    doneText->pointSize = 16;
    doneText->color = Drawable::Color(220, 200, 160, 255);
    doneText->position = ScreenPos(doneX + 30, doneY + 8);
    m_renderTarget->draw(doneText);
}

bool SettingsScreen::handleEvent(const input::Event &event)
{
    if (!m_visible) return false;

    // Escape closes the screen
    if (event.type == input::Event::KeyPressed && event.key.code == input::Key::Escape) {
        hide();
        return true;
    }

    ScreenPos pos;
    bool isTap = false;
    bool isDrag = false;

    if (event.type == input::Event::TouchEnded || event.type == input::Event::MouseButtonReleased) {
        pos = (event.type == input::Event::TouchEnded)
            ? ScreenPos(event.touch.x, event.touch.y)
            : ScreenPos(event.mouseButton.x, event.mouseButton.y);
        isTap = true;
    } else if (event.type == input::Event::TouchMoved) {
        pos = ScreenPos(event.touch.x, event.touch.y);
        isDrag = true;
    } else if (event.type == input::Event::TouchBegan || event.type == input::Event::MouseButtonPressed) {
        pos = (event.type == input::Event::TouchBegan)
            ? ScreenPos(event.touch.x, event.touch.y)
            : ScreenPos(event.mouseButton.x, event.mouseButton.y);
        isDrag = true;
    }

    if (isDrag) {
        // Handle slider drags
        if (handleSliderDrag(m_soundSlider, pos.x, pos.y)) {
            m_soundVolume = m_soundSlider.value;
            return true;
        }
        if (handleSliderDrag(m_musicSlider, pos.x, pos.y)) {
            m_musicVolume = m_musicSlider.value;
            return true;
        }
        if (handleSliderDrag(m_speedSlider, pos.x, pos.y)) {
            m_gameSpeed = m_speedSlider.value * 2.f; // 0-1 → 0-2x
            if (m_gameSpeed < 0.25f) m_gameSpeed = 0.25f;
            return true;
        }
        return true; // consume all events while visible
    }

    if (!isTap) return true;

    Size screenSize = m_renderTarget->getSize();
    float panelW = std::min(500.f, screenSize.width - 40.f);
    float panelH = std::min(350.f, screenSize.height - 80.f);
    float panelX = (screenSize.width - panelW) / 2;
    float panelY = (screenSize.height - panelH) / 2;

    // Close button
    if (ScreenRect(panelX + panelW - 40, panelY + 10, 30, 30).contains(pos)) {
        hide();
        return true;
    }

    // Done button
    float doneX = panelX + panelW / 2 - 50;
    float doneY = panelY + panelH - 50;
    if (ScreenRect(doneX, doneY, 100, 36).contains(pos)) {
        hide();
        return true;
    }

    // Click outside = close
    if (!ScreenRect(panelX, panelY, panelW, panelH).contains(pos)) {
        hide();
        return true;
    }

    return true;
}
