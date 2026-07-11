#include "NumberLabel.h"

#include "render/IRenderTarget.h"
#include "core/Types.h"

#include <string>

NumberLabel::NumberLabel(std::shared_ptr<IRenderTarget> renderTarget) :
    m_renderTarget(std::move(renderTarget))
{
    m_text = m_renderTarget->createText(Drawable::Text::UI);
    m_text->outlineColor = Drawable::Black;
    m_text->color = Drawable::White;
    m_text->pointSize = 16;
    m_text->alignment = Drawable::Text::AlignRight;
}

bool NumberLabel::setValue(const int value)
{
    if (value == m_value) {
        return false;
    }
    m_value = value;
    updateText();
    return true;
}

bool NumberLabel::setMaxValue(const int maxValue)
{
    if (maxValue == m_maxValue) {
        return false;
    }
    m_maxValue = maxValue;
    updateText();
    return true;
}

void NumberLabel::setPosition(const ScreenPos &pos)
{
    m_top = pos.y;
    m_right = pos.x;
}

void NumberLabel::render()
{
    m_renderTarget->draw(m_text);
}

void NumberLabel::updateText()
{
    std::string string = std::to_string(m_value);
    if (m_maxValue) {
        string += '/';
        string += std::to_string(m_maxValue);
    }
    m_text->string = string;
    m_text->position = ScreenPos(m_right, m_top);
}
