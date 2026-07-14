#include "MouseCursor.h"

#include <genie/resource/SlpFile.h>
#include <genie/dat/UnitCommand.h>

#include "core/Logger.h"
#include "mechanics/UnitManager.h"
#include "render/IRenderTarget.h"
#include "resource/AssetManager.h"

MouseCursor::MouseCursor(std::shared_ptr<IRenderTarget> renderTarget) :
    m_renderTarget(std::move(renderTarget))
{
    m_cursorsFile = AssetManager::Inst()->getSlp(AssetManager::filenameID("mcursors.shp"));
    if (!m_cursorsFile) {
        WARN << "Failed to get cursors";
    }

    setCursor(Type::Normal);
}

bool MouseCursor::isValid() const
{
    return m_currentType != Type::Invalid;
}

bool MouseCursor::setPosition(const ScreenPos &position)
{
    if (position == m_position) {
        return false;
    }

    m_position = position;
    return true;
}

bool MouseCursor::update(const std::shared_ptr<UnitManager> &unitManager)
{
    unitManager->onCursorPositionChanged(m_position, m_renderTarget->camera());

    switch(unitManager->state()) {
    case UnitManager::State::Default: {
        const TaskSet &targetActions = unitManager->currentActionUnderCursor();
        if (targetActions.isEmpty()) {
            return setCursor(MouseCursor::Normal);
        }

        const Task &targetAction = targetActions.first(); // TODO: simplification, might want to prioritize or something

        REQUIRE(targetAction.data, return false);

        switch (targetAction.data->ActionType) {
        case genie::ActionType::Combat:
            return setCursor(MouseCursor::Attack);
        case genie::ActionType::GatherRebuild:
        case genie::ActionType::Hunt:
            return setCursor(MouseCursor::Axe);
        case genie::ActionType::Build:
        case genie::ActionType::Repair:
            return setCursor(MouseCursor::Build);
        case genie::ActionType::Heal:
            return setCursor(MouseCursor::Protect);
        case genie::ActionType::Convert:
            return setCursor(MouseCursor::Protect);
        case genie::ActionType::Garrison:
            return setCursor(MouseCursor::Garrison);
        default:
            return setCursor(MouseCursor::Action);
        }
        break;
    }
    case UnitManager::State::SelectingAttackTarget: {
        return setCursor(MouseCursor::Axe); // idk
    }
    case UnitManager::State::PlacingBuilding:
    case UnitManager::State::PlacingWall:
        return setCursor(MouseCursor::Invalid); // IDK
    case UnitManager::State::SelectingGarrisonTarget:
        return setCursor(MouseCursor::Garrison);
    }

    WARN << "Unhandled state" << unitManager->state();
    return false;
}

void MouseCursor::render()
{
    if (m_currentType == Invalid) {
        return;
    }
    if (m_currentType != Invalid) {
        m_renderTarget->draw(m_image, m_position);
    }
}

bool MouseCursor::setCursor(const MouseCursor::Type type)
{
    if (type == m_currentType) {
        return false;
    }
    if (!m_cursorsFile) {
        WARN << "No cursors file available!";
        return false;
    }

    if (type != Invalid) {
        REQUIRE(type < m_cursorsFile->getFrameCount(), return false);

        const genie::SlpFramePtr &newFrame = m_cursorsFile->getFrame(type);
        REQUIRE(newFrame != nullptr, return false);

        m_image = m_renderTarget->convertFrameToImage(newFrame);
    }

    m_currentType = type;
    return true;
}
