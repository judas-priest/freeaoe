#pragma once

#include "render/IRenderTarget.h"
#include "render/EventTypes.h"
#include "core/Types.h"

#include <memory>

class SettingsScreen
{
public:
    SettingsScreen(const std::shared_ptr<IRenderTarget> &renderTarget);

    void show();
    void hide();
    bool isVisible() const { return m_visible; }

    void render();
    bool handleEvent(const input::Event &event);

private:
    std::shared_ptr<IRenderTarget> m_renderTarget;
    bool m_visible = false;

    Drawable::Text::Ptr m_titleText;
    Drawable::Text::Ptr m_labelText;

    // Settings values (read/write from Config)
    float m_soundVolume = 1.0f;
    float m_musicVolume = 1.0f;
    float m_gameSpeed = 1.0f;

    void loadSettings();
    void saveSettings();

    // Slider hit detection
    struct Slider {
        float x, y, width, height;
        float value; // 0.0 - 1.0
    };
    Slider m_soundSlider, m_musicSlider, m_speedSlider;
    void drawSlider(const Slider &slider, const std::string &label, const std::string &valueStr);
    bool handleSliderDrag(Slider &slider, float touchX, float touchY);
};
