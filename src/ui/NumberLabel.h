#pragma once

#include "render/IRenderTarget.h"

#include <memory>

struct ScreenPos;

struct NumberLabel
{
    NumberLabel(std::shared_ptr<IRenderTarget> renderTarget);

    bool setValue(const int value);
    bool setMaxValue(const int maxValue);
    void setPosition(const ScreenPos &pos);

    void render();

private:
    void updateText();

    int m_maxValue = 0;
    int m_value = 0;

    int m_right = 0;
    int m_top = 0;
    Drawable::Text::Ptr m_text;

    std::shared_ptr<IRenderTarget> m_renderTarget;
};
