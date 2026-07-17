#include "ActionPanel.h"
#include "Engine.h"
#include "mechanics/Building.h"
#include "mechanics/Gate.h"
#include "actions/ActionGarrison.h"
#include "actions/ActionAutoScout.h"
#include "actions/ActionTransform.h"
#ifdef __ANDROID__
#include <android/log.h>
#endif

#ifndef USE_SDL2
#ifndef USE_SDL2
#include <SFML/Graphics/Color.hpp>
#endif
#ifndef USE_SDL2
#include <SFML/Graphics/RectangleShape.hpp>
#endif
#endif
#include <genie/dat/Research.h>
#include <genie/dat/Unit.h>

#include <genie/dat/Unit.h>
#include <genie/resource/SlpFile.h>
#include <stddef.h>
#include <stdint.h>
#include <algorithm>

#include "actions/IAction.h"
#include "core/Logger.h"
#include "mechanics/Civilization.h"
#include "mechanics/Player.h"
#include "mechanics/Unit.h"
#ifdef USE_SDL2
#include "render/SdlRenderTarget.h"
#else
#include "render/SfmlRenderTarget.h"
#endif
#include "resource/AssetManager.h"
#include "resource/LanguageManager.h"
#include "resource/Resource.h"
#include "resource/GameSpecific.h"
#include "resource/DataManager.h"

#ifdef USE_SDL2
ActionPanel::ActionPanel(std::shared_ptr<IRenderTarget> renderTarget) :
#else
ActionPanel::ActionPanel(std::shared_ptr<SfmlRenderTarget> renderTarget) :
#endif
    m_renderTarget(std::move(renderTarget))
{
}

ActionPanel::~ActionPanel()
{
}


bool ActionPanel::init()
{

    return true;
}

bool ActionPanel::handleEvent(input::Event event)
{
    if (event.type != input::Event::MouseButtonPressed && event.type != input::Event::MouseButtonReleased) {
        return false;
    }
    ScreenPos mousePos(event.mouseButton.x, event.mouseButton.y);
    if (!rect().contains(mousePos)) {
        releaseButtons();
        return false;
    }

    for (InterfaceButton &button : currentButtons) {
        if (button.interfacePage != m_currentPage) {
            continue;
        }

        if (!buttonRect(button.index).contains(mousePos)) {
            if (button.pressed) {
                m_dirty = true;
            }

            button.pressed = false;
            continue;
        }
        if (button.type == InterfaceButton::AttackStance) {
            if (event.type == input::Event::MouseButtonPressed) {
                handleButtonClick(button);
            }

            break;
        }

        if (event.type == input::Event::MouseButtonPressed) {
            if (button.pressed) {
                m_dirty = true;
            }
            button.pressed = true;
        } else {
            if (button.pressed) {
                m_dirty = true;
                handleButtonClick(button);
            }
            button.pressed = false;
        }
        break;
    }

    return true;
}

bool ActionPanel::update(Time /*time*/)
{
#ifdef ANDROID
    m_buttonSize = 56;
    m_bottomOffset = 10;
#else
    if (m_renderTarget->getSize().height >= 1024) {
        m_buttonSize = 45;
        m_bottomOffset = 30;
    } else {
        m_buttonSize = 40;
        m_bottomOffset = 20;
    }
#endif
    if (m_buttonsDirty) {
        m_selectedUnits = m_unitManager->selected().units;
        updateButtons();
        m_dirty = true;
    }

    if (m_unitManagerState != m_unitManager->state()) {
        m_unitManagerState = m_unitManager->state();
        updateButtons();
        m_dirty = true;
    }

    if (m_dirty) {
        m_dirty = false;
        return true;
    }

    return false;
}

void ActionPanel::draw()
{
    REQUIRE(!m_commandIcons.empty() && !m_unitIcons.empty() && !m_buildingIcons.empty() && !m_researchIcons.empty(), return);

    for (const InterfaceButton &button : currentButtons) {
        if (button.interfacePage != m_currentPage) {
            continue;
        }

        if (button.showBorder) {
            Drawable::Rect bevelRect;
            bevelRect.rect.setTopLeft(buttonPosition(button.index) - ScreenPos(2, 2));
            bevelRect.rect.setSize(Size(m_buttonSize, m_buttonSize));
            bevelRect.filled = true;

            Drawable::Rect shadowRect;
            shadowRect.rect.setTopLeft(buttonPosition(button.index));
            shadowRect.rect.setSize(Size(m_buttonSize - 2, m_buttonSize - 2));
            shadowRect.filled = true;

            // need this because the garrison icon is actually a cursor
            Drawable::Rect backgroundRect;
            backgroundRect.rect.setTopLeft(buttonPosition(button.index));
            backgroundRect.rect.setSize(Size(m_buttonSize - 4, m_buttonSize - 4));
            backgroundRect.filled = true;

            if (button.pressed) {
                bevelRect.fillColor = Drawable::Color(64, 64, 64);
                shadowRect.fillColor = Drawable::Color(192, 192, 192);
            } else {
                bevelRect.fillColor = Drawable::Color(192, 192, 192);
                shadowRect.fillColor = Drawable::Color(64, 64, 64);
            }
            bevelRect.borderColor =  bevelRect.fillColor;
            shadowRect.borderColor =  shadowRect.fillColor;

            m_renderTarget->draw(bevelRect);
            m_renderTarget->draw(shadowRect);
            m_renderTarget->draw(backgroundRect);
        }

        switch(button.type) {
        case InterfaceButton::CreateBuilding:
            m_renderTarget->draw(m_buildingIcons[button.iconId], buttonPosition(button.index));
            break;
        case InterfaceButton::CreateUnit:
            m_renderTarget->draw(m_unitIcons[button.iconId], buttonPosition(button.index));
            break;
        case InterfaceButton::Research:
            m_renderTarget->draw(m_researchIcons[button.iconId], buttonPosition(button.index));
            break;
        case InterfaceButton::AttackStance:
        case InterfaceButton::Other:
            m_renderTarget->draw(m_commandIcons[button.action], buttonPosition(button.index));
            break;
        }
//        m_renderTarget->draw(button.tex, buttonPosition(button.index));
    }
}

void ActionPanel::setUnitManager(const std::shared_ptr<UnitManager> &unitManager)
{
    if (unitManager == m_unitManager) {
        return;
    }
    if (m_unitManager) {
        m_unitManager->disconnect(this);
    }

    if (unitManager) {
        unitManager->connect(UnitManager::ActionsChanged, this, &ActionPanel::onActionsChanged);
    }
    m_unitManager = unitManager;
}

void ActionPanel::setHumanPlayer(const Player::Ptr &player)
{
    if (player->playerId == m_humanPlayerId) {
        return;
    }

    m_humanPlayerId = player->playerId;
    m_humanPlayer = player;

    REQUIRE(loadButtons(player->civilization.id()), return);
}

ScreenRect ActionPanel::rect() const
{
    ScreenRect r;
    r.height = 3 * m_buttonSize;
    //r.height = 3 * 51;
    r.width = 5 * m_buttonSize;
#ifdef ANDROID
    r.x = 3; // Flush-left on mobile
#else
    r.x = m_buttonSize;
#endif
//    r.y = 845;
    r.y = m_renderTarget->getSize().height - r.height - m_bottomOffset;
    //r.y = m_renderTarget->getSize().height - r.height - 25;
    return r;
}

void ActionPanel::releaseButtons()
{
    for (InterfaceButton &button : currentButtons) {
        if (button.pressed) {
            m_dirty = true;
        }

        button.pressed = false;
    }
}

std::string ActionPanel::helpTextId(const ActionPanel::Command icon)
{
    static const std::unordered_map<ActionPanel::Command, int> helpTextIds = {
        { Command::Cancel, 4911 },
        { Command::Ungarrison, 41014}, // TODO: 4950 for ram
        { Command::Stop, 4905 },
        { Command::Patrol, 4938 },
        { Command::Guard, 4936},
        { Command::Pack, 41055 },
        { Command::Unpack, 41056 },
        { Command::Convert, 4925  },
        { Command::SellWood, 41072 },
        { Command::SellFood, 41073 },
        { Command::SellStone, 41074 },
        { Command::CollectWood, 41076 }, // buy wood
        { Command::CollectFood, 41077 }, // buy food
        { Command::BuyFood, 41077 }, // buy food
        { Command::CollectStone, 41078 }, // buy stone
        { Command::BuyStone, 41078 }, // buy stone
        { Command::BuildCivilian, 41061 },
        { Command::BuildMilitary, 41062 },
        { Command::NextPage, 4912 }, // TODO 4918 for dock
        { Command::NextPage, 4912 },
        { Command::DisbandFormation, 41006 },
        { Command::LineFormation, 41021 },
        { Command::BoxFormation, 41020 },
        { Command::HordeFormation, 41019 },
        { Command::SetRallyPoint, 4944 },
        { Command::RemoveRallyPoint, 4949 },
        { Command::CloseGate, 41104 },
        { Command::OpenGate, 41105 },

        { Command::ResearchSpies, 41113 },

        { Command::Kill, 4941 },
        { Command::AttackGround, 4923 },
        { Command::FlankFormation, 41023 },
        { Command::SpreadOutFormation, 41022 },
        { Command::AbortTownBell, 41015 },
        { Command::RingTownBell, 41111 },
        { }
    };

    if (helpTextIds.find(icon) == helpTextIds.end()) {
        static const std::string nullString;
        return nullString;
    }

    return LanguageManager::Inst()->getString(helpTextIds.at(icon));
}

void ActionPanel::onPlayerResourceChanged(Player *player, const genie::ResourceType type, float newValue)
{
    (void)type;
    (void)newValue;

    // Might afford new units or not afford units
    if (player->playerId == m_humanPlayerId) {
        m_buttonsDirty = true;
    }
}

bool ActionPanel::loadButtons(const int playerCiv)
{
    genie::SlpFilePtr unitIconsSlp = AssetManager::Inst()->getInterfaceSlp(AssetManager::StandardSlpType::Units, playerCiv);
    REQUIRE(unitIconsSlp, return false);
    for (size_t i=0; i<unitIconsSlp->getFrameCount(); i++) {
        m_unitIcons[i] = m_renderTarget->convertFrameToImage(unitIconsSlp->getFrame(i));
    }

    genie::SlpFilePtr buildingIconsSlp = AssetManager::Inst()->getInterfaceSlp(AssetManager::StandardSlpType::Buildings, playerCiv);
    REQUIRE(buildingIconsSlp, return false);
    for (size_t i=0; i<buildingIconsSlp->getFrameCount(); i++) {
        m_buildingIcons[i] = m_renderTarget->convertFrameToImage(buildingIconsSlp->getFrame(i));
    }

    genie::SlpFilePtr researchIconsSlp = AssetManager::Inst()->getInterfaceSlp(AssetManager::StandardSlpType::Technology, playerCiv);
    REQUIRE(researchIconsSlp, return false);
    DBG << "Technology icons SLP: frames=" << researchIconsSlp->getFrameCount() << "civ=" << playerCiv;
    for (size_t i=0; i<researchIconsSlp->getFrameCount(); i++) {
        m_researchIcons[i] = m_renderTarget->convertFrameToImage(researchIconsSlp->getFrame(i));
    }

    genie::SlpFilePtr commandIconsSlp = AssetManager::Inst()->getInterfaceSlp(AssetManager::StandardSlpType::Commands, playerCiv);
    REQUIRE(commandIconsSlp, return false);
    for (size_t i=0; i<int(Command::IconCount); i++) {
        if (i >= commandIconsSlp->getFrameCount()) {
            WARN << "icon out of range " << i << "max is" << commandIconsSlp->getFrameCount();
            return false;
        }
        m_commandIcons[Command(i)] = m_renderTarget->convertFrameToImage(commandIconsSlp->getFrame(i));
    }

    ////////// HAX ///////////
    { // hax
        const genie::SlpFramePtr prevImage = commandIconsSlp->getFrame(int(Command::NextPage));
        m_commandIcons[Command::PreviousPage] = m_renderTarget->convertFrameToImage(prevImage->mirrorX());
    }

    // Auto-scout uses the patrol icon (compass)
    m_commandIcons[Command::AutoScout] = m_commandIcons[Command::Patrol];

    // Idle villager uses the CollectFood icon as a stand-in
    m_commandIcons[Command::FindIdleVillager] = m_commandIcons[Command::CollectFood];

    { // can't find this icon anywhere else
        genie::SlpFilePtr cursorsSlp = AssetManager::Inst()->getSlp("mcursors.shp", AssetManager::ResourceType::Interface);
        if (!cursorsSlp) {
            WARN << "Failed to load cursors";
            return false;
        }
        m_commandIcons[Command::Garrison] = m_renderTarget->convertFrameToImage(cursorsSlp->getFrame(13));
    }


    return true;
}

void ActionPanel::updateButtons()
{
    m_buttonsDirty = false;

    currentButtons.clear();

    if (m_selectedUnits.empty()) {
        // Show idle villager button even with no selection
        InterfaceButton idleBtn;
        idleBtn.type = InterfaceButton::Other;
        idleBtn.action = Command::FindIdleVillager;
        idleBtn.index = 0;
        idleBtn.interfacePage = 0;
        currentButtons.push_back(idleBtn);
        return;
    }
    if (m_unitManagerState != UnitManager::State::Default) {
        DBG << "unit manager state" << m_unitManagerState;
        return;
    }

    Unit::Ptr unit = *m_selectedUnits.begin();
    Player::Ptr owner = unit->player().lock();
    if (!owner) {
        WARN << "Unit without owner";
        return;
    }
    if (owner->playerId != m_humanPlayerId) {
        return;
    }

    if (unit->data()->Type >= genie::Unit::MovingType) {
        InterfaceButton killButton;
        killButton.action = Command::Kill;
        killButton.index = 3;
        currentButtons.push_back(killButton);
    }

    const TaskSet actions = unit->actions.availableActions();
    std::unordered_set<genie::ActionType> addedTypes;
    for (const Task &task : actions) {
        if (addedTypes.count(task.data->ActionType)) {
            continue;
        }
        addedTypes.insert(task.data->ActionType);

        switch(task.data->ActionType) {
        case genie::ActionType::Garrison: {
            InterfaceButton garrisonButton;
            garrisonButton.action = Command::Garrison;
            garrisonButton.index = 4;
            currentButtons.push_back(garrisonButton);
            break;
        }
        case genie::ActionType::Build: {
            InterfaceButton backButton;
            backButton.action = Command::PreviousPage;
            backButton.index = 14;
            backButton.interfacePage = genie::Unit::BuildingsInterface;
            currentButtons.push_back(backButton);
            backButton.interfacePage = genie::Unit::MilitaryBuildingsInterface;
            currentButtons.push_back(backButton);

            InterfaceButton civilianButton;
            civilianButton.action = Command::BuildCivilian;
            civilianButton.index = 0;
            currentButtons.push_back(civilianButton);

            InterfaceButton militaryButton;
            militaryButton.action = Command::BuildMilitary;
            militaryButton.index = 1;
            currentButtons.push_back(militaryButton);

            InterfaceButton repairButton;
            repairButton.action = Command::Repair;
            repairButton.index = 2;
            currentButtons.push_back(repairButton);
            break;
        }
        case genie::ActionType::RetreatToShootingRage:
        case genie::ActionType::Combat:
            break;
        default:
            WARN << "Unhandled action type" << task.data->ActionType << task.data->actionTypeName() << task.data->AutoSearchTargets;
            break;
        }
    }

    if (!actions.isEmpty()) {
        InterfaceButton stopButton;
        stopButton.action = Command::Stop;
        stopButton.index = 9;
        currentButtons.push_back(stopButton);
    }

    addCreateButtons(unit);
    if (unit->data()->InterfaceKind == genie::Unit::BuildingsInterface) {
        addResearchButtons(unit);
    }
    if (unit->data()->InterfaceKind == genie::Unit::SoldiersInterface) {
        addMilitaryButtons(unit);
    }

    // Building-specific buttons: Ungarrison and Town Bell
    if (unit->data()->Type >= genie::Unit::BuildingType) {
        Building::Ptr building = Building::fromUnit(unit);
        if (building) {
            // Ungarrison button if building has garrisoned units
            if (!building->garrisonedUnits.empty()) {
                InterfaceButton ungarrisonBtn;
                ungarrisonBtn.action = Command::Ungarrison;
                ungarrisonBtn.index = 5;
                ungarrisonBtn.interfacePage = 0;
                currentButtons.push_back(ungarrisonBtn);
            }

            // Town Bell / Abort Town Bell for Town Centers (ID 109)
            if (unit->data()->ID == Unit::TownCenter) {
                InterfaceButton bellBtn;
                bellBtn.action = m_bellActive ? Command::AbortTownBell : Command::RingTownBell;
                bellBtn.index = 6;
                bellBtn.interfacePage = 0;
                currentButtons.push_back(bellBtn);
            }
        }
    }
}

void ActionPanel::addCreateButtons(const std::shared_ptr<Unit> &unit)
{
    m_currentPage = 0;

    if (unit->creationProgress() < 1.) {
        return;
    }

    Player::Ptr humanPlayer = m_humanPlayer.lock();
    if (!humanPlayer) {
        WARN << "Player-less unit";
        return;
    }
    const std::vector<const genie::Unit *> creatableUnits = humanPlayer->civilization.creatableUnits(unit->data()->ID);

    if (creatableUnits.empty()) {
        return;
    }

    if (unit->data()->Type >= genie::Unit::BuildingType) {
        Building::Ptr building = Building::fromUnit(unit);
        InterfaceButton rallypointButton;
        if (building && building->hasRallyPoint) {
            rallypointButton.action = Command::RemoveRallyPoint;
        } else {
            rallypointButton.action = Command::SetRallyPoint;
        }
        rallypointButton.index = 4;
        currentButtons.push_back(rallypointButton);
    }

    bool hasNext = false;
    for (const genie::Unit *creatable : creatableUnits) {
        if (creatable->Creatable.ButtonID > m_buttonOffset + 15) {
            hasNext = true;
            continue;
        }

        InterfaceButton button;
        if (unit->data()->InterfaceKind == genie::Unit::CiviliansInterface) {
            button.type = InterfaceButton::CreateBuilding;
            button.interfacePage = creatable->InterfaceKind;
        } else {
            button.type = InterfaceButton::CreateUnit;
            button.interfacePage = 0;
        }
        button.index = std::max(creatable->Creatable.ButtonID - 1, 0);
        button.unit = creatable;
        button.iconId = creatable->IconID;

        currentButtons.push_back(button);
    }

    if (hasNext) {
        InterfaceButton rightButton;
        rightButton.action = Command::NextPage;
        rightButton.index = 14;
        currentButtons.push_back(rightButton);
    }
}

void ActionPanel::addResearchButtons(const std::shared_ptr<Unit> &unit)
{
    Player::Ptr player = unit->player().lock();
    if (!player) {
        WARN << "Player-less unit";
        return;
    }
    const std::vector<const genie::Tech *> techs = player->civilization.researchAvailableAt(unit->data()->ID);
    if (techs.empty()) {
        return;
    }

    bool hasNext = false;

    for (const genie::Tech *tech : techs) {
        if (tech->ButtonID > m_buttonOffset + 15) {
            hasNext = true;
            continue;
        }

        // Skip techs that have already been researched
        int techIdx = player->civilization.techIndex(tech);
        if (techIdx >= 0 && player->hasResearched(techIdx)) {
            continue;
        }

        // Skip techs whose prerequisites aren't met (e.g. Castle Age before Feudal)
        if (tech->RequiredTechCount > 0) {
            int satisfied = 0;
            for (int16_t reqId : tech->RequiredTechs) {
                if (reqId == -1) continue;
                if (player->hasResearched(reqId)) satisfied++;
            }
            if (satisfied < tech->RequiredTechCount) continue;
        }


        InterfaceButton button;
        button.type = InterfaceButton::Research;

        button.index = std::max(tech->ButtonID - 1, 0);
        button.tech = tech;
        button.iconId = tech->IconID;

        currentButtons.push_back(button);

    }


    if (hasNext) {
        InterfaceButton rightButton;
        rightButton.action = Command::NextPage;
        rightButton.index = 14;
        currentButtons.push_back(rightButton);
    }

}

void ActionPanel::addMilitaryButtons(const std::shared_ptr<Unit> &unit)
{
    if (unit->data()->InterfaceKind != genie::Unit::SoldiersInterface) {
        return;
    }

    // Gate lock/unlock button
    if (unit->data()->Class == genie::Unit::Gate) {
        Gate::Ptr gate = Gate::fromUnit(unit);
        if (gate) {
            InterfaceButton gateBtn;
            gateBtn.action = gate->isLocked ? Command::OpenGate : Command::CloseGate;
            gateBtn.index = 5;
            currentButtons.push_back(gateBtn);
        }
    }

    if (unit->data()->Class == genie::Unit::SiegeWeapon || unit->data()->Class == genie::Unit::UnpackedSiegeUnit) {
        InterfaceButton button;
        button.type = InterfaceButton::Other;
        button.interfacePage = 0;
        button.index = 0;
        button.action = Command::AttackGround;

        currentButtons.push_back(button);
    } else {
        InterfaceButton button;

        button.type = InterfaceButton::Other;
        button.interfacePage = 0;
        button.index = 0;
        button.action = Command::Patrol;
        currentButtons.push_back(button);

        button.type = InterfaceButton::Other;
        button.interfacePage = 0;
        button.index = 1;
        button.action = Command::Guard;
        currentButtons.push_back(button);


        button.type = InterfaceButton::Other;
        button.interfacePage = 0;
        button.index = 2;
        button.action = Command::Follow;
        currentButtons.push_back(button);
    }

    // Stance buttons
    if (unit->actions.hasAutoTargets()) {
        const Unit::Stance current = unit->stance;
        InterfaceButton button;
        button.showBorder = false;
        button.type = InterfaceButton::AttackStance;
        button.interfacePage = 0;

        if (current == Unit::Stance::Aggressive) {
            button.action = Command::AggressiveEnabled;
        } else {
            button.action = Command::Aggressive;
        }
        button.index = 5;
        currentButtons.push_back(button);

        if (current == Unit::Stance::Defensive) {
            button.action = Command::DefensiveEnabled;
        } else {
            button.action = Command::Defensive;
        }
        button.index = 6;
        currentButtons.push_back(button);

        if (current == Unit::Stance::StandGround) {
            button.action = Command::StandGroundEnabled;
        } else {
            button.action = Command::StandGround;
        }
        button.index = 7;
        currentButtons.push_back(button);

        if (current == Unit::Stance::NoAttack) {
            button.action = Command::NoAttackEnabled;
        } else {
            button.action = Command::NoAttack;
        }
        button.index = 8;
        currentButtons.push_back(button);

    }

    // Auto-scout button for scout-class units
    if (unit->data()->Class == genie::Unit::Scout ||
        unit->data()->ID == 448 || unit->data()->ID == 546) {
        InterfaceButton autoScoutBtn;
        autoScoutBtn.type = InterfaceButton::Other;
        autoScoutBtn.action = Command::AutoScout;
        autoScoutBtn.index = 10;
        autoScoutBtn.interfacePage = 0;
        currentButtons.push_back(autoScoutBtn);
    }
}

void ActionPanel::handleButtonClick(const ActionPanel::InterfaceButton &button)
{
    // Show help text for this button
    std::string help = helpTextId(button.action);
    if (!help.empty()) {
        lastHelpText = help;
        lastHelpTextTime = Engine::currentTimeMs();
    }

    if (button.type == InterfaceButton::CreateBuilding) {
        m_unitManager->startPlaceBuilding(button.unit->ID, m_humanPlayer.lock());
        currentButtons.clear();
        return;
    } else if (button.type == InterfaceButton::CreateUnit) {
        m_unitManager->enqueueProduceUnit(button.unit, m_selectedUnits);
    } else if (button.type == InterfaceButton::Research) {
        m_unitManager->enqueueResearch(button.tech, m_selectedUnits);
    } else if (button.type == InterfaceButton::AttackStance) {
        Unit::Stance newStance = Unit::Stance::Aggressive;
        switch(button.action) {
        case Command::Aggressive:
            newStance = Unit::Stance::Aggressive;
            break;
        case Command::Defensive:
            newStance = Unit::Stance::Defensive;
            break;
        case Command::StandGround:
            newStance = Unit::Stance::StandGround;
            break;
        case Command::NoAttack:
            newStance = Unit::Stance::NoAttack;
            break;
        default:
            return;
        }

        for (const Unit::Ptr &unit : m_selectedUnits) {
            unit->stance = newStance;
        }

        updateButtons();

    } else if (button.type == InterfaceButton::Other) {
        switch(button.action) {
        case Command::BuildMilitary:
            m_currentPage = genie::Unit::MilitaryBuildingsInterface;
            break;
        case Command::BuildCivilian:
            m_currentPage = genie::Unit::BuildingsInterface;
            break;
        case Command::PreviousPage:
            m_currentPage = 0;
            break;
        case Command::Stop:
            for (const Unit::Ptr &unit : m_unitManager->selected()) {
                unit->actions.clearActionQueue();
            }
            break;
        case Command::Kill:
            for (const Unit::Ptr &unit : m_unitManager->selected()) {
                unit->kill();
            }
            break;
        case Command::AttackGround:
            m_unitManager->selectAttackTarget();
            break;
        case Command::Garrison: {
            m_unitManager->selectGarrisonTarget();
            break;
        }
        case Command::Patrol:
            m_unitManager->selectPatrolTarget();
            break;
        case Command::Guard:
            m_unitManager->selectGuardTarget();
            break;
        case Command::Follow:
            m_unitManager->selectFollowTarget();
            break;
        case Command::Repair:
            m_unitManager->selectRepairTarget();
            break;
        case Command::Convert:
            m_unitManager->selectConvertTarget();
            break;
        case Command::Heal:
            m_unitManager->selectHealTarget();
            break;
        // Market buy/sell — dynamic prices shift by 3 per transaction
        case Command::SellWood: {
            auto player = m_unitManager->humanPlayer();
            if (player && player->resourcesAvailable(genie::ResourceType::WoodStorage) >= 100) {
                int revenue = player->marketPrices.sellPrice(1); // 1=Wood
                player->setAvailableResource(genie::ResourceType::WoodStorage,
                    player->resourcesAvailable(genie::ResourceType::WoodStorage) - 100);
                player->setAvailableResource(genie::ResourceType::GoldStorage,
                    player->resourcesAvailable(genie::ResourceType::GoldStorage) + revenue);
                player->marketPrices.onSell(1);
            }
            break;
        }
        case Command::SellFood: {
            auto player = m_unitManager->humanPlayer();
            if (player && player->resourcesAvailable(genie::ResourceType::FoodStorage) >= 100) {
                int revenue = player->marketPrices.sellPrice(0); // 0=Food
                player->setAvailableResource(genie::ResourceType::FoodStorage,
                    player->resourcesAvailable(genie::ResourceType::FoodStorage) - 100);
                player->setAvailableResource(genie::ResourceType::GoldStorage,
                    player->resourcesAvailable(genie::ResourceType::GoldStorage) + revenue);
                player->marketPrices.onSell(0);
            }
            break;
        }
        case Command::SellStone: {
            auto player = m_unitManager->humanPlayer();
            if (player && player->resourcesAvailable(genie::ResourceType::StoneStorage) >= 100) {
                int revenue = player->marketPrices.sellPrice(2); // 2=Stone
                player->setAvailableResource(genie::ResourceType::StoneStorage,
                    player->resourcesAvailable(genie::ResourceType::StoneStorage) - 100);
                player->setAvailableResource(genie::ResourceType::GoldStorage,
                    player->resourcesAvailable(genie::ResourceType::GoldStorage) + revenue);
                player->marketPrices.onSell(2);
            }
            break;
        }
        case Command::CollectWood:
        case Command::BuyFood:
        case Command::CollectFood: {
            auto player = m_unitManager->humanPlayer();
            // CollectWood buys wood, CollectFood/BuyFood buys food
            int resIdx = (button.action == Command::CollectWood) ? 1 : 0;
            int cost = player ? player->marketPrices.buyPrice(resIdx) : 130;
            if (player && player->resourcesAvailable(genie::ResourceType::GoldStorage) >= cost) {
                player->setAvailableResource(genie::ResourceType::GoldStorage,
                    player->resourcesAvailable(genie::ResourceType::GoldStorage) - cost);
                genie::ResourceType res = (resIdx == 1)
                    ? genie::ResourceType::WoodStorage : genie::ResourceType::FoodStorage;
                player->setAvailableResource(res, player->resourcesAvailable(res) + 100);
                player->marketPrices.onBuy(resIdx);
            }
            break;
        }
        case Command::CollectStone:
        case Command::BuyStone: {
            auto player = m_unitManager->humanPlayer();
            int cost = player ? player->marketPrices.buyPrice(2) : 130;
            if (player && player->resourcesAvailable(genie::ResourceType::GoldStorage) >= cost) {
                player->setAvailableResource(genie::ResourceType::GoldStorage,
                    player->resourcesAvailable(genie::ResourceType::GoldStorage) - cost);
                player->setAvailableResource(genie::ResourceType::StoneStorage,
                    player->resourcesAvailable(genie::ResourceType::StoneStorage) + 100);
                player->marketPrices.onBuy(2);
            }
            break;
        }
        case Command::Ungarrison: {
            // Eject all garrisoned units from selected buildings
            for (const Unit::Ptr &unit : m_selectedUnits) {
                auto building = std::dynamic_pointer_cast<Building>(unit);
                if (building) {
                    building->ungarrisonAll();
                }
            }
            break;
        }
        case Command::Disembark: {
            // Same as ungarrison — eject all units
            for (const Unit::Ptr &unit : m_selectedUnits) {
                auto building = std::dynamic_pointer_cast<Building>(unit);
                if (building) {
                    building->ungarrisonAll();
                }
            }
            break;
        }
        case Command::LineFormation:
            Unit::s_formation = Unit::Formation::Line;
            break;
        case Command::BoxFormation:
            Unit::s_formation = Unit::Formation::Box;
            break;
        case Command::FlankFormation:
            Unit::s_formation = Unit::Formation::Flank;
            break;
        case Command::SpreadOutFormation:
            Unit::s_formation = Unit::Formation::SpreadOut;
            break;
        case Command::PickUpRelic:
            m_unitManager->selectRepairTarget(); // reuse repair target selection for relic
            break;
        case Command::Pack:
        case Command::Unpack: {
            Player::Ptr human = m_humanPlayer.lock();
            if (!human) break;
            for (const Unit::Ptr &unit : m_selectedUnits) {
                if (!unit) continue;
                // Trebuchet packed (331) <-> unpacked (42)
                int currentId = unit->data()->ID;
                int swapId = -1;
                Time transformTime = 11100; // ~11.1 seconds default pack/unpack
                if (currentId == 331) {
                    swapId = 42;    // packed -> unpacked
                } else if (currentId == 42) {
                    swapId = 331;   // unpacked -> packed
                }
                if (swapId >= 0) {
                    unit->actions.clearActionQueue();
                    Task transformTask;
                    transformTask.target = unit; // self-target
                    unit->actions.setCurrentAction(
                        std::make_shared<ActionTransform>(unit, transformTask, swapId, transformTime));
                }
            }
            m_buttonsDirty = true;
            break;
        }
        case Command::SetRallyPoint:
            m_unitManager->selectRallyTarget();
            break;
        case Command::RemoveRallyPoint:
            for (const Unit::Ptr &unit : m_selectedUnits) {
                Building::Ptr building = Building::fromUnit(unit);
                if (!building) continue;
                building->waypoint = MapPos(building->position().x + 24, building->position().y + 24);
                building->rallyTarget.reset();
                building->hasRallyPoint = false;
            }
            m_buttonsDirty = true;
            break;
        case Command::RingTownBell: {
            if (m_bellActive) break;
            m_garrisonedByBell.clear();
            m_bellActive = true;

            // Find all TCs belonging to the human player
            std::vector<Unit::Ptr> townCenters;
            for (const Unit::Ptr &u : m_unitManager->units()) {
                if (u->playerId() != m_humanPlayerId) continue;
                if (u->data()->ID == Unit::TownCenter) {
                    townCenters.push_back(u);
                }
            }
            if (townCenters.empty()) break;

            constexpr float BELL_RANGE = 23.f * 48.f; // 23 tiles in pixels

            for (const Unit::Ptr &u : m_unitManager->units()) {
                if (u->playerId() != m_humanPlayerId) continue;
                if (u->data()->Class != genie::Unit::Civilian) continue;

                // Find nearest TC within range
                Unit::Ptr nearestTC;
                float nearestDist = BELL_RANGE;
                for (const Unit::Ptr &tc : townCenters) {
                    float d = u->position().distance(tc->position());
                    if (d < nearestDist) {
                        nearestDist = d;
                        nearestTC = tc;
                    }
                }
                if (!nearestTC) continue;

                m_garrisonedByBell.push_back({u});

                u->actions.clearActionQueue();
                Task garrisonTask;
                garrisonTask.target = nearestTC;
                u->actions.setCurrentAction(std::make_shared<ActionGarrison>(u, garrisonTask));
            }
            break;
        }
        case Command::CloseGate:
        case Command::OpenGate: {
            const bool newLocked = (button.action == Command::CloseGate);
            for (const Unit::Ptr &u : m_selectedUnits) {
                Gate::Ptr gate = Gate::fromUnit(u);
                if (gate) {
                    gate->isLocked = newLocked;
                    if (newLocked) gate->setOpen(false);
                }
            }
            m_buttonsDirty = true;
            break;
        }
        case Command::AbortTownBell: {
            if (!m_bellActive) break;
            m_bellActive = false;

            // Ungarrison all TCs
            for (const Unit::Ptr &u : m_unitManager->units()) {
                if (u->playerId() != m_humanPlayerId) continue;
                if (u->data()->ID == Unit::TownCenter) {
                    auto tcBuilding = Building::fromUnit(u);
                    if (tcBuilding) tcBuilding->ungarrisonAll();
                }
            }

            m_garrisonedByBell.clear();
            break;
        }
        case Command::AutoScout: {
            for (const Unit::Ptr &unit : m_selectedUnits) {
                if (!unit) continue;
                unit->actions.clearActionQueue();
                unit->actions.setCurrentAction(
                    std::make_shared<ActionAutoScout>(unit));
            }
            break;
        }
        case Command::SignalFlare: {
            if (m_unitManager) {
                m_unitManager->selectFlareTarget();
            }
            break;
        }
        case Command::FindIdleVillager: {
            if (!m_unitManager) break;
            static int lastIdleIdx = -1;
            int startIdx = lastIdleIdx + 1;
            const auto &allUnits = m_unitManager->units();
            bool found = false;
            for (size_t i = 0; i < allUnits.size(); i++) {
                int idx = (startIdx + i) % allUnits.size();
                const Unit::Ptr &unit = allUnits[idx];
                if (!unit || unit->playerId() != m_humanPlayerId) continue;
                if (unit->data()->Type < genie::Unit::CombatantType) continue;
                if (unit->data()->Speed <= 0) continue;
                if (!unit->actions.currentAction()) {
                    lastIdleIdx = idx;
                    m_unitManager->setSelectedUnits({unit});
                    found = true;
                    break;
                }
            }
            if (!found) {
                lastHelpText = "No idle villagers";
                lastHelpTextTime = 5000;
            }
            break;
        }
        default:
            WARN << "Unhandled action" << button.action;
            break;
        }
    }
}

ScreenPos ActionPanel::buttonPosition(const int index) const
{
    ScreenPos position;
    position.x = index % 5;
    position.x = (position.x) * m_buttonSize + rect().x;
    position.y = std::floor(index / 5.f);
    position.y *= m_buttonSize;
    position.y += rect().y;
    return position;
}

ScreenRect ActionPanel::buttonRect(const int index) const
{
    ScreenRect rect;
    const ScreenPos screenPos = buttonPosition(index);
    rect.x = screenPos.x;
    rect.y = screenPos.y;
    rect.width = m_buttonSize;
    rect.height = m_buttonSize;
    return rect;
}
