# Gameplay Tech Debt

**Реальный статус: ~70% до играбельности. 12 коммитов за ночь 14 июля исправили gathering, trigger system (48/49), AI, performance.**

**Дата обновления: 2026-07-14 (ночной прогон завершён)**

---

## P0 — БЛОКЕРЫ (без этого игра неиграбельна)

### 1. Сбор ресурсов — нужна отладка

> **ВАЖНО:** Код сбора ресурсов СУЩЕСТВУЕТ и выглядит функциональным. Нужно определить, ГДЕ ИМЕННО цепочка ломается, а не переписывать всё с нуля.

Gathering framework реализован: `ActionGather`, `ActionDropOff`, `findDropSite()`, авто-сбор после постройки drop-off зданий, food decay, herdable auto-conversion. Но в gameplay сбор не работает — нужно найти точку разрыва.

**5 типов ресурсов (все не работают через правый клик):**

1. **Мясо** — овцы/олени/кабаны: крестьянин убивает но не собирает
2. **Ягоды** — кусты: клик ничего не делает
3. **Золото** — жилы: клик ничего не делает
4. **Камень** — жилы: клик ничего не делает
5. **Дерево** — рубка: клик ничего не делает

**Последствия:**
- Экономика мертва — после стартовых ресурсов ничего нельзя сделать
- AI тоже не собирает — `BasicAI::assignIdleVillagers()` вызывает `findTaskWithTarget()` — тот же код
- Нельзя построить ничего после первого дома
- Нельзя натренировать юнитов после стартовых
- Игра заканчивается через 2 минуты когда ресурсы кончаются

---

#### 1.1 Полная цепочка правого клика (от мыши до ActionGather)

**Шаг 1: Обнаружение цели под курсором**

Файл: `src/mechanics/UnitManager.cpp:1109-1137` — `onCursorPositionChanged()`

```
Каждый кадр при движении мыши:
1. m_tasksUnderCursor.clear()
2. forEachUnitAt(pos, camera, lambda) — ищет юниты под курсором
3. Для каждого найденного target:
   - Проверяет: не является ли target единственным выделенным юнитом
   - Для каждого выделенного unit: unit->actions.findTaskWithTarget(target)
   - Если task валиден → m_tasksUnderCursor.add(task)
```

**ПОТЕНЦИАЛЬНЫЙ БАГ в лямбде (строка 1128):** Если `findTaskWithTarget` вернёт невалидный task для ПЕРВОГО выделенного юнита, лямбда делает `return false` — это выходит из ВСЕЙ лямбды, не проверяя остальных выделенных юнитов. Но так как все крестьяне одного типа, это вряд ли является причиной бага.

**Шаг 2: Эллиптический хит-тест**

Файл: `src/mechanics/UnitManager.cpp:921-941` — `forEachUnitAt()`

```cpp
void UnitManager::forEachUnitAt(const ScreenPos &position, const CameraPtr camera,
    const std::function<bool(const Unit::Ptr &)> &action)
{
    for (auto it = m_units.rbegin(); it != m_units.rend(); it++) {
        Unit::Ptr unit = *it;
        if (!unit->isVisible) continue;        // ← Ресурс ДОЛЖЕН быть visible!

        const ScreenPos unitPos = camera->absoluteScreenPos(unit->position());
        double radiusX = std::max(unit->data()->OutlineSize.x * TILE_SIZE_HORIZONTAL / 2., 15.0);
        double radiusY = std::max(unit->data()->OutlineSize.y * TILE_SIZE_VERTICAL / 2., 15.0);
        double dx = (position.x - unitPos.x) / radiusX;
        double dy = (position.y - unitPos.y) / radiusY;

        if (dx * dx + dy * dy <= 1.0) {  // Эллипс
            if (action(unit)) break;
        }
    }
}
```

**Проверить:**
- `unit->isVisible` — устанавливается в `true` в `UnitsRenderer.cpp:75-134` ТОЛЬКО для юнитов попавших в кадр. Если ресурс НЕ был отрисован, `isVisible` остаётся `false` и хит-тест его пропустит!
- `OutlineSize.x` и `OutlineSize.y` — минимум 15px, но если OutlineSize == 0, эллипс будет 15x15px в экранных координатах. Для дерева этого может быть достаточно, для мелких ресурсов (ягоды) — нет.
- Позиция `unitPos` — камера преобразует map→screen. Ресурс может быть на правильной map-позиции, но screen-позиция не совпадает с кликом из-за бага в преобразовании координат.

**Шаг 3: Поиск подходящего задания**

Файл: `src/mechanics/UnitActionHandler.cpp:75-185` — `findTaskWithTarget()` → `findMatchingTask()`

```
findTaskWithTarget(target) вызывает:
  findMatchingTask(player, target, availableActions())

availableActions() (строки 17-54):
  1. Загружает tasks из DataManager::getTasks(unitID) для юнита
  2. Пропускает Combat tasks если у юнита нет атак
  3. Если юнит в TaskSwapGroup — добавляет tasks от ВСЕХ swappable юнитов
     (крестьяне все в одной группе: лесоруб, фермер, шахтёр и т.д.)

findMatchingTask(player, target, tasks) (строки 80-185):
  Для каждого task в списке:
  1. Дипломатия:
     - TargetSelf (0): target.playerId == own → Ok
     - TargetNeutralsEnemies (2): target.playerId != own → Ok
     - TargetGaiaOnly (3): target.playerId == 0 → Ok
       ИСКЛЮЧЕНИЕ: свои DomesticAnimal тоже Ok
     - TargetSelfAllyGaia (4): own/ally/gaia → Ok
     - TargetGaiaNeutralEnemies (5): не свой и не союзный → Ok
     - TargetAnyDiplo (6): всегда Ok
  2. Пропускает Garrison actions
  3. Если target не достроен → только Build action
  4. canMatchGenieUnitID(UnitID) → точное совпадение ID или swap group
  5. ClassID == target.Class → совпадение по классу (Tree=15, GoldMine=8, StoneMine=9, BerryBush=7, PreyAnimal=3, DomesticAnimal=29)
  6. Если ничего не нашёл → пробует generic Combat task для врагов
```

**Самые вероятные причины провала:**

1. **Ресурсы Gaia (player 0):** Tasks для gathering имеют `TargetDiplomacy == TargetGaiaOnly (3)`. Если ресурс почему-то НЕ Gaia (playerId != 0), дипломатия не пропустит.

2. **Task list пуст:** `DataManager::getTasks(83)` (83 = Male Villager) может вернуть пустой список, если:
   - Dat файл неправильно распарсен
   - UnitHeaders[83].TaskList не содержит gather tasks
   - HD Edition dat имеет другую структуру tasks

3. **TaskSwapGroup не работает:** Крестьянин ID=83 должен быть в TaskSwapGroup вместе с лесорубами (ID=218), шахтёрами (ID=579), фермерами и т.д. Если `swappableUnits()` возвращает пустой список, gather tasks не добавятся.

4. **ClassID не совпадает:** Task ожидает ClassID=15 (Tree), но реальное дерево имеет другой Class в HD Edition dat.

5. **UnitID не совпадает:** Task указывает конкретный UnitID (не -1), и `canMatchGenieUnitID` не находит совпадение.

---

#### 1.2 План отладки (пошаговый)

**Фаза 1: Определить где ломается (добавить логи)**

```cpp
// В UnitManager::onCursorPositionChanged (строка 1116):
DBG << "=== CURSOR AT" << pos.x << pos.y << "===";

// В лямбде forEachUnitAt (строка 1116):
DBG << "Found unit under cursor:" << target->debugName
    << "playerId=" << target->playerId()
    << "class=" << target->data()->Class
    << "canBeGathered=" << target->data()->CanBeGathered;

// В findMatchingTask (строка 80):
DBG << "findMatchingTask: checking" << potentials.size() << "tasks for target"
    << target->debugName << "playerId=" << target->playerId();

// Перед каждым continue в цикле дипломатии:
DBG << "  Diplomacy FAIL: task.TargetDiplomacy=" << action->TargetDiplomacy
    << "target.playerId=" << target->playerId();

// После проверки ClassID:
DBG << "  ClassID check: task.ClassID=" << action->ClassID
    << "target.Class=" << target->data()->Class
    << (action->ClassID == target->data()->Class ? "MATCH" : "NO MATCH");
```

**Фаза 2: Проверить dat-файл**

```cpp
// В main() или GameState конструкторе — дамп tasks для крестьянина:
const auto &tasks = DataManager::Inst().getTasks(83);
LOG << "Villager (83) has" << tasks.size() << "tasks:";
for (const auto &t : tasks) {
    LOG << "  ActionType=" << t.ActionType << " ClassID=" << t.ClassID
        << " UnitID=" << t.UnitID << " TargetDiplomacy=" << t.TargetDiplomacy
        << " ResourceIn=" << t.ResourceIn << " ResourceOut=" << t.ResourceOut;
}
// Также проверить TaskSwapGroup:
LOG << "Villager TaskSwapGroup=" << DataManager::Inst().getUnit(83).Action.TaskSwapGroup;
```

**Фаза 3: Проверить свойства ресурсов**

```cpp
// При создании юнита в UnitFactory (строка 210+):
if (gunit.CanBeGathered) {
    LOG << "Created gatherable unit:" << gunit.Name
        << "ID=" << gunit.ID << "Class=" << gunit.Class
        << "OutlineSize=" << gunit.OutlineSize.x << "x" << gunit.OutlineSize.y
        << "playerId=" << unit->playerId();
    for (const auto &rs : gunit.ResourceStorages) {
        LOG << "  Resource: Type=" << rs.Type << " Amount=" << rs.Amount;
    }
}
```

**Фаза 4: Проверить ActionGather execution**

```cpp
// В ActionGather::update (строка 35):
DBG << "ActionGather::update: unit=" << unit->debugName
    << "target=" << (target ? target->debugName : "null")
    << "resourceType=" << m_resourceType
    << "unit.resources=" << unit->resources[m_resourceType]
    << "target.resources=" << (target ? target->resources[m_resourceType] : -1)
    << "capacity=" << unit->data()->ResourceCapacity;
```

---

#### 1.3 Ключевые файлы и строки

| Компонент | Файл | Строки | Что проверить |
|-----------|------|--------|---------------|
| Курсор → цель | `UnitManager.cpp` | 1109-1137 | m_tasksUnderCursor заполняется? |
| Хит-тест | `UnitManager.cpp` | 921-941 | isVisible, OutlineSize, позиция |
| Task matching | `UnitActionHandler.cpp` | 80-185 | Дипломатия, ClassID, UnitID |
| Available tasks | `UnitActionHandler.cpp` | 17-54 | getTasks(), TaskSwapGroup |
| Task data из dat | `DataManager.cpp` | 130-138 | UnitHeaders[id].TaskList |
| Правый клик | `UnitManager.cpp` | 551-602 | m_tasksUnderCursor не пуст? |
| Task assignment | `IAction.cpp` | 44-128 | Hunt/GatherRebuild case |
| Gather execution | `ActionGather.cpp` | 35-102 | update() loop |
| Drop-off | `ActionGather.cpp` | 118-172 | findDropSite(), maybeDropOff() |
| Resource init | `UnitFactory.cpp` | 210-219 | ResourceStorages заполнены? |
| AI gathering | `BasicAI.cpp` | 148-182 | assignIdleVillagers() |

---

#### 1.4 Что УЖЕ реализовано (перестать дублировать работу)

Следующие фичи сбора УЖЕ работают в коде — не нужно их реализовывать заново:

| Фича | Файл | Строки | Статус |
|------|------|--------|--------|
| ActionGather — сам процесс сбора | `ActionGather.cpp` | 35-102 | ✅ WorkRate * WorkValue1 * dt |
| ActionDropOff — отнести в здание | `ActionGather.cpp` | 175-220 | ✅ addResource → player |
| findDropSite — ближайший drop-off | `ActionGather.cpp` | 143-172 | ✅ По Action.DropSites |
| Авто-возврат к ресурсу после drop-off | `ActionGather.cpp` | 136-138 | ✅ Ставит новый ActionGather |
| Авто-атака перед сбором (охота) | `ActionGather.cpp` | 55-60 | ✅ prependAction(ActionAttack) |
| Herdable auto-conversion (овцы) | `UnitManager.cpp` | 293-296 | ✅ Gaia → player on LOS |
| Food decay на мёртвых животных | `Unit.cpp` | 126-131 | ✅ 0.016 food/sec |
| Авто-сбор после постройки drop-off | `ActionBuild.cpp` | 78-108 | ✅ Lumber/Mill/Mining Camp |
| Авто-deposit ресурсов в drop-off | `ActionBuild.cpp` | 60-75 | ✅ При постройке drop-off |
| AI gathering | `BasicAI.cpp` | 148-182 | ⚠️ Зависит от findTaskWithTarget |

---

#### 1.5 Специфика HD Edition dat-файла

В HD Edition (Steam) структура .dat файлов может отличаться от оригинальной AoE2:
- Tasks могут быть привязаны к ДРУГИМ unit ID (не 83 для крестьянина)
- ClassID для ресурсов может быть другим
- TaskSwapGroup нумерация может отличаться
- UnitHeaders могут быть в другом порядке

**Референсы для проверки:**
- openage docs (`doc/media/`) — описание форматов .dat включая task lists
- Advanced Genie Editor Wiki — визуальный редактор .dat файлов для проверки tasks
- geniedoc (aap) — документация структур данных

---

## P1 — Серьёзные проблемы

### 2. AI — ограниченный но работающий

> **ОБНОВЛЕНИЕ:** AI значительно лучше чем описывалось ранее. BasicAI уже реализует 9 поведений. Есть также полноценная AiScript система (110 conditions, 56 actions) но она НЕ ИСПОЛЬЗУЕТСЯ.

#### 2.1 BasicAI — что делает (9 функций, update каждые 5 сек)

| # | Функция | Файл:строки | Что делает |
|---|---------|-------------|------------|
| 1 | `trainVillagers()` | BasicAI.cpp:65-83 | Тренирует до 20 крестьян (ID=83) в TC |
| 2 | `buildHouses()` | BasicAI.cpp:85-146 | Строит дома когда headroom < 5, стоимость 30 дерева |
| 3 | `assignIdleVillagers()` | BasicAI.cpp:148-182 | Назначает idle крестьян на ближайший ресурс (CanBeGathered) |
| 4 | `buildDropOffSites()` | BasicAI.cpp:184-228 | Lumber Camp (562) у деревьев, Mining Camp (584) |
| 5 | `researchLoom()` | BasicAI.cpp:230-258 | Researches Loom (tech 22 или 8) в TC |
| 6 | `advanceAge()` | BasicAI.cpp:260-284 | Feudal→Castle→Imperial последовательно |
| 7 | `trainMilitary()` | BasicAI.cpp:324-372 | Militia(74)/Spearman(93) из Barracks, Archer(4) из Range |
| 8 | `attackWithArmy()` | BasicAI.cpp:286-322 | Атакует при 5+ idle military. Цель: вражеский TC |
| 9 | `buildStructure()` | BasicAI.cpp:374-432 | Хелпер: спиральный поиск места, collision check 5x5 |

#### 2.2 AI — что НЕ делает

| # | Недостаток | Сложность | Решение |
|---|-----------|-----------|---------|
| 1 | Не скаутит карту | Medium | Послать скаута в random direction |
| 2 | Не строит стены/башни | Medium | buildStructure() + wall placement |
| 3 | Не отступает при проигрыше | Easy | Проверить army HP < 30% → retreat |
| 4 | Не отстраивается после потерь | Easy | Уже есть: trainMilitary + buildStructure |
| 5 | Только 3 типа военных | Easy | Добавить Knight(38), Siege(36), Monk(125) |
| 6 | Не использует Siege Workshop / Stable | Easy | Добавить IDs: Stable=101, Siege=49 |
| 7 | Не исследует технологии (кроме Loom/Ages) | Medium | researchTechs() пустой (строка 434-438) |
| 8 | Макс 20 крестьян / 15 военных | Easy | Увеличить лимиты или сделать зависимыми от эпохи |
| 9 | Не собирает ресурсы (зависит от P0) | **Blocked** | Починить findTaskWithTarget |
| 10 | Не загружает .ai/.per скрипты | Hard | ScriptLoader существует но не вызывается |

#### 2.3 AiScript система (НЕИСПОЛЬЗУЕМАЯ)

Полноценная rule-based AI система существует но не активна:

- **Файлы:** `src/ai/AiScript.h/.cpp`, `AiRule.h/.cpp`, `ScriptLoader.h/.cpp`, `conditions/Conditions.h/.cpp`, `actions/Actions.h/.cpp`
- **Формат:** стандартные .ai/.per файлы AoE2
- **Парсер:** Flex/Bison (tokenizer.gen.flex + grammar)
- **Conditions:** 110+ (resource checks, unit counts, can-train/build/research, diplomacy, timer, goals)
- **Actions:** 56 (train, build, research, trade, attack, resign, set-goal, chat)
- **Инициализация:** `GameState.cpp:313` создаёт пустой AiScript для каждого AI игрока
- **Проблема:** `ScriptLoader` никогда не вызывается — скрипты не загружаются

**Для активации:**
1. В GameState: вызвать `ScriptLoader::load(ai_script_path)` после создания AiScript
2. AI скрипты лежат в game data: `Script.Ai/*.per`
3. Или сгенерировать default rules программно

---

### 3. Иконка века на Android

- IV (Imperial) показывается вместо II (Feudal)
- IconID=30 правильный для десктопа, неправильный на Android

**Детали:**
- Файл: `src/ui/ActionPanel.cpp:549-551` — `button.iconId = tech->IconID`
- SLP загружается через: `AssetManager::getInterfaceSlp(StandardSlpType::Technology, playerCiv)`
- Возможные причины:
  - HD Edition interface SLP имеет другой порядок фреймов
  - На Android загружается другой SLP файл (fallback на PNG)
  - IconID 30 указывает на Imperial icon в HD, но Feudal в standard edition

**План отладки:**
1. Добавить лог: `DBG << "Age icon: techId=" << tech->ID << " IconID=" << tech->IconID`
2. Проверить какой SLP загружен: `DBG << "Technology SLP frames=" << slp->getFrameCount()`
3. Сравнить frame 30 на десктопе vs Android
4. Если SLP разный — добавить маппинг IconID для HD Edition

---

### 4. Elevation/slopes — чёрные артефакты

Terrain только плоский — slopes вызывают чёрные тайлы. Elevation отключена в RandomMapGenerator.

**Текущее состояние:**

| Компонент | Файл:строки | Статус |
|-----------|-------------|--------|
| Elevation offset (screen Y) | `MapRenderer.cpp:202` | ✅ `rect.z = elevation * m_elevationHeight` |
| Slope enum | `MapTile.h:30-182` | ✅ 14 типов slopes (Flat, SouthUp, NorthUp...) |
| Random map elevation | `RandomMapGenerator.cpp:106` | ❌ `tile.elevation = 2` (hardcoded flat) |
| Campaign elevation | `Map.cpp:177` | ✅ Загружается из .scn файла |
| SLP slope rendering | `TerrainSprite.cpp:244-430` | ⚠️ filtermap/slpTemplate — артефакты |
| PNG slope rendering | `TerrainSprite.cpp:131-240` | ❌ Полностью игнорирует slopes |

**Корневая причина:**

SLP terrain рендеринг (строки 244-430) использует:
1. `slpTemplate = AssetManager::getSlpTemplateFile()->templates[slope.toGenie()]` — шаблон формы тайла для данного наклона
2. `filtermap = AssetManager::filtermapFile()->maps[slope.toGenie()]` — маппинг пикселей для наклонённого тайла
3. ICM (Inverse Color Map) — модель освещения для slopes

Некоторые slope combinations (например, диагональные: SouthWestUp, NorthEastUp) имеют невалидные индексы в filtermap/stemplet.dat, что приводит к чтению мусора → чёрные пиксели.

PNG terrain рендеринг (строки 131-240) — упрощённый: использует только diamond-маску без filtermap, **полностью игнорирует наклоны**.

**План исправления:**

1. **Минимальный фикс (Easy):** Ограничить elevation range 1-3 (вместо 0-7) и разрешить только простые slopes (N/S/E/W up, без диагональных)
2. **Средний фикс (Medium):** Добавить boundary check при обращении к filtermap/slpTemplate. Если index out of bounds → рендерить как Flat
3. **Полный фикс (Hard):** Портировать рабочий C-код рендеринга ландшафта из aoetwols как референс. Использовать geniedoc документацию stemplet.dat

**Справочные проекты:**
- **aoetwols** (aap, MIT) — рабочий C-код рендеринга тайлов со склонами
- **geniedoc** (aap) — документация stemplet.dat, blendomatic, tileedge, filtermap
- **openage** `doc/media/` — описание форматов .dat

---

## P2 — Performance

### 5. PNG terrain с диска

**Проблема:** stbi_load() вызывается для каждой уникальной комбинации (terrain_type × blend × slope).

**Файл:** `src/resource/TerrainSprite.cpp:131-240`

```
Цепочка:
1. texture(tile, target) вызывается для каждого тайла (строка 228 в MapRenderer)
2. Ищет в кэше: m_textures[tile] (строка 138)
3. Кэш промах → sourceImage.loadFromFile(m_pngPath) (строка 144)
4. loadFromFile → stbi_load(path, ...) (строка 114) — чтение PNG с диска!
5. Обработка diamond-маски + blending (строки 166-236)
6. Результат кэшируется в m_textures
```

**Кэш:** `std::unordered_map<MapTile, Drawable::Image::Ptr> m_textures` (TerrainSprite.h:81)
- Ключ — ВЕСЬ MapTile (terrain + blends + slopes)
- Разные blend combinations = разные cache entries = повторные disk loads
- На 144x144 карте: ~20k тайлов, ~10-30 уникальных terrain типов × blend combos = десятки stbi_load

**Фикс (3 уровня):**

1. **Quick (Easy):** Кэшировать сырые PNG данные в памяти при первой загрузке TerrainSprite (конструктор, строка 56-95). Вместо `loadFromFile` каждый раз — хранить `PngImageData m_cachedSource` как поле.

2. **Medium:** LRU cache eviction для m_textures. Текущий кэш растёт без ограничений — на длинных сессиях может съесть всю память.

3. **Full:** Texture atlas — загрузить все terrain PNGs в один большой атлас при старте. Tile rendering = просто выбор sub-rect из атласа.

---

### 6. createText() каждый кадр

**Проблема:** `drawUi()` создаёт новые Text объекты каждый кадр.

**Файлы и строки:**

| Место | Файл:строка | Частота |
|-------|-------------|---------|
| Help text tooltip | `Engine.cpp:665` | Каждый кадр при наведении |
| Post-game stats | `Engine.cpp:397` | Каждый кадр на экране статистики |
| UnitInfoPanel | `UnitInfoPanel::draw()` | Каждый кадр при выделении юнита |
| ActionPanel labels | `ActionPanel::draw()` | Каждый кадр |
| Minimap FPS/coords | `Minimap::draw()` | Каждый кадр |

**Влияние:**
- Каждый `createText()` = аллокация памяти + HarfBuzz text shaping + SDL_ttf glyph rendering
- На 60 FPS = 60+ аллокаций/кадр только на текст
- На Android с медленной памятью — заметные лаги

**Фикс:** Создать Text объекты ОДИН РАЗ в конструкторе. В draw() обновлять только `.string` если значение изменилось. SDL_ttf кэширует глифы по содержимому строки.

---

### 7. Нет sprite batching

**Проблема:** Каждый юнит/здание = отдельный `SDL_RenderCopy` вызов.

**Текущая архитектура:**

```
Terrain: MapRenderer.cpp:190-251
  Для каждого видимого тайла:
    draw(tileTexture)     — 1 SDL_RenderCopy
    draw(shadowMask)      — 1 SDL_RenderCopy
    draw(unexploredMask)  — 1 SDL_RenderCopy
  = 3 вызова × ~50 видимых тайлов = ~150 terrain calls

Units: UnitsRenderer.cpp:44-213
  Для каждого видимого entity:
    entity->render(Shadow)  — 1 SDL_RenderCopy
    entity->render(Normal)  — 1 SDL_RenderCopy
    (outline если выделен)  — 1-2 SDL_RenderCopy
  = 2-4 вызова × 50-100 юнитов = 100-400 unit calls

UI: Engine.cpp:573-792
  Top bar, bottom panel, buttons, text = ~20-30 calls

ИТОГО: 300-500+ SDL_RenderCopy/кадр
```

**Фикс (по приоритету):**

1. **Terrain batching (Medium):** Рендерить terrain в offscreen texture один раз, перерисовывать только при скролле камеры.
2. **Unit grouping (Hard):** Группировать юнитов по текстуре (SLP), batch с одинаковой текстурой.
3. **UI caching (Easy):** Кэшировать UI layout как текстуру, перерисовывать при изменении.

---

## P3 — Мелочи и polish

### 8. Drop-off ресурсов — РЕАЛИЗОВАН

> **ОБНОВЛЕНИЕ:** Drop-off полностью реализован. Не нужно переделывать.

| Компонент | Файл:строки | Статус |
|-----------|-------------|--------|
| findDropSite() — поиск ближайшего | ActionGather.cpp:143-172 | ✅ По unit.Action.DropSites |
| maybeDropOff() — инициация | ActionGather.cpp:118-141 | ✅ Move→DropOff→Move→Gather |
| ActionDropOff::update() | ActionGather.cpp:192-220 | ✅ player.addResource() |
| Auto-deposit при постройке | ActionBuild.cpp:60-75 | ✅ Ресурсы из крестьянина в здание |
| Auto-gather после постройки | ActionBuild.cpp:78-108 | ✅ Lumber/Mill/Mining Camp |

**Что проверить (minor):**
- [ ] Correct building match per resource (Lumber Camp for wood, Mill for food, Mining Camp for gold/stone)
- [ ] Drop-off на TC (Town Center принимает все типы ресурсов)
- [ ] Unit DropSites в dat файле правильно парсятся

---

### 9. Cursor context — framework есть, не привязан

**Файл:** `src/ui/MouseCursor.h:20-63`

19 типов курсоров определено:
```
Normal, Busy, Target, Action, Attack, TargetPos, WhatsThis,
Build, Axe, Protect, Horn, MoveTo, Disabled, Garrison,
Garrison2, Disembark, Embark, TargetCircle, Flag
```

**Проблема:** Курсор не меняется при наведении на ресурс. `setCursor(Type)` существует, но не вызывается из `onCursorPositionChanged`.

**Фикс (Easy):**
В `UnitManager::onCursorPositionChanged` (строка 1109-1137):
```cpp
if (!m_tasksUnderCursor.isEmpty()) {
    // Определить тип курсора по ActionType первого task
    switch (m_tasksUnderCursor.first().data->ActionType) {
        case genie::ActionType::GatherRebuild: cursor->setCursor(MouseCursor::Axe); break;
        case genie::ActionType::Hunt: cursor->setCursor(MouseCursor::Axe); break;
        case genie::ActionType::Combat: cursor->setCursor(MouseCursor::Attack); break;
        case genie::ActionType::Build: cursor->setCursor(MouseCursor::Build); break;
        case genie::ActionType::Repair: cursor->setCursor(MouseCursor::Build); break;
        case genie::ActionType::Heal: cursor->setCursor(MouseCursor::Protect); break;
        default: cursor->setCursor(MouseCursor::Action); break;
    }
} else {
    cursor->setCursor(MouseCursor::Normal);
}
```

---

### 10. Scenario triggers — 48/49 РЕАЛИЗОВАНЫ ✅

> **ОБНОВЛЕНИЕ ночь 14 июля:** Все triggers кроме AISignal реализованы за 3 коммита.

#### Implemented Conditions (18/19):
BringObjectToArea, BringObjectToObject, OwnObjects, OwnFewerObjects, ObjectsInArea, DestroyObject, CaptureObject, AccumulateAttribute, ResearchTechnology, Timer, ObjectSelected, PlayerDefeated, ObjectHasTarget, ObjectVisible, ObjectNotVisible, ResearchingTechnology, UnitsGarrisoned, DifficultyLevel

#### Missing Conditions (1/19):
| Condition | Сложность | Причина |
|-----------|-----------|---------|
| AISignal | Hard | Требует интеграции с AI scripting system |

#### Implemented Effects (29/~30):
ChangeDiplomacy, ResearchTechnology, SendChat, Sound, SendTribute, ActivateTrigger, DeactivateTrigger, CreateObject, TaskObject, DeclareVictory, KillObject, RemoveObject, ChangeView, Unload, ChangeOwnership, Patrol, DisplayInstructions, ClearInstructions, SetUnitStance, UseAdvancedButtons, DamageObject, PlaceFoundation, ChangeObjectName, ChangeObjectHP, ChangeObjectAttack (stub), HD_AttackMove (stub), HD_ChangeArmor (stub), HD_ChangeRange (stub), HD_ChangeSpeed (stub), HealObject

#### Missing Effects (1/~30):
| Effect | Сложность | Причина |
|--------|-----------|---------|
| AIScriptGoal | Hard | Требует AI scripting integration |
| UnlockGate/LockGate | Medium | Нужна gate state system (walls не реализованы) |

#### Stubs (логируют но не модифицируют данные):
ChangeObjectAttack, HD_AttackMove, HD_ChangeArmor, HD_ChangeRange, HD_ChangeSpeed — требуют mutable unit data

---

### 11. Garrison mechanics — БОЛЬШЕЙ ЧАСТЬЮ РЕАЛИЗОВАНЫ

> **ОБНОВЛЕНИЕ:** Garrison arrows и healing УЖЕ реализованы! Ранее помечались как "missing".

| Фича | Файл:строки | Статус |
|------|-------------|--------|
| Unit garrison в TC/Castle/Tower | ActionGarrison | ✅ |
| Ungarrison button | ActionPanel | ✅ |
| **Garrison arrows** | ActionAttack.cpp:271-277 | ✅ Extra projectiles proportional to garrisoned |
| **Garrison healing** | Building.cpp:291-306 | ✅ TC 0.1 HP/sec, Castle 0.2 HP/sec |
| **Relic gold generation** | Building.cpp:308-319 | ✅ 0.5 gold/sec per relic in Monastery |
| GarrisonCapacity from dat | data()->GarrisonCapacity | ✅ |
| Transport ships | ActionGarrison (same code) | ✅ |
| Garrison indicator (flag) | — | ❌ |
| Garrison icon overlay on building | — | ❌ |

---

### 12. Wall & Gate mechanics — НЕ реализованы

| Компонент | Статус | Что есть |
|-----------|--------|----------|
| Wall unit IDs | ✅ | Palisade=72, Stone=117, Fortified=155 |
| Gate unit ID | ✅ | Gate=487 |
| PlacingWall state | ✅ | Drag placement в UnitManager |
| Wall auto-orientation | ❌ | Нет поворота сегментов |
| Gate open/close | ❌ | Нет механики ворот |
| Wall connection to buildings | ❌ | Нет |

---

### 13. Pathfinding — A* реализован

> **ОБНОВЛЕНИЕ:** A* pathfinding ПОЛНОСТЬЮ реализован, вопреки ранним утверждениям о "no AStar".

**Файл:** `src/actions/ActionMove.cpp:697-1074`

```
findPath(start, end, coarseness):
  - A* с heuristic weight = 10
  - Grid-based: тайлы как ноды
  - Passability check по terrain + entities
  - 3 уровня coarseness (1, 2, 5, 10) — если не находит на мелком → ищет на крупном
  - Euclidean distance heuristic (строка 849)
  - Multithreaded pathfinding infrastructure (m_pathfindingThread, строка 91) но disabled (строка 1015)
  - DEBUG_PATHFINDING define для визуализации (строка 30)
```

**Ограничения:**
- Нет dynamic obstacle avoidance (юниты не обходят друг друга плавно)
- Coarseness=10 может давать странные пути
- Background threading не используется (TODO на строке 1015)

---

### 14. Combat — armor classes

**Файл:** `src/actions/ActionAttack.cpp`

Damage formula реализована (min 1 total, not per-class — fixed в commit):
```cpp
damage = sum(max(0, attack[class] - armor[class]))
if (damage < 1) damage = 1;
// + elevation bonus (+25% high ground, -25% uphill)
```

**Нужно проверить:** Все ли armor classes из dat файла правильно загружаются. AoE2 имеет 31 armor class (Infantry, Cavalry, Archer, Siege, Building, etc.).

---

### 15. Civ bonuses — data-driven но частично

**Файл:** `src/mechanics/Civilization.cpp`

Tech effects работают data-driven через dat файл. Бонусы цивилизаций (Britons +1 range, Mongols faster cavalry archers) применяются через EffectCommands при advancing ages.

**Проблема:** Некоторые бонусы могут не применяться если tech effect chain сломана.

**Проверить:** Создать игру за разные civs и сравнить unit stats с Wiki.

---

### 16. Missing mechanics (полный список)

| Механика | Статус | Сложность | Детали |
|----------|--------|-----------|--------|
| Line of sight sharing (allies) | ❌ | Medium | Нужно объединять visibility maps |
| Flare signal (Alt+click) | ❌ | Easy | Визуальный эффект на minimap |
| Multiplayer/Network | ❌ | Hard | Framework есть (TunnelToServer/Client) но не работает |
| Minimap fog of war | ⚠️ | Easy | Minimap рендерится но fog/enemy dots не проверены |
| Unit queue cancel buttons | ⚠️ | Easy | Queue показывается в UnitInfoPanel |
| Cursor change on hover | ❌ | Easy | MouseCursor framework есть, не привязан (см. пункт 9) |

---

## P4 — Android-specific

### 17. Touch input

**Файл:** `src/Engine.h:169-187`

```cpp
struct TouchState {
    enum class Phase { Idle, Pending, Dragging, WaitSecondTap, LongPress };
    Phase phase = Phase::Idle;
    bool pinching = false;
    static constexpr float DRAG_THRESHOLD = 50.f;
    static constexpr int64_t DOUBLE_TAP_MS = 700;
    static constexpr float DOUBLE_TAP_DIST = 100.f;
    static constexpr int64_t LONG_PRESS_MS = 600;
};
```

**Потенциальные проблемы:**
- Нет thread sync для m_touchState (SDL events могут приходить из другого потока)
- Multi-touch pinch: strict 2-finger check (3+ пальцев → игнорируется)
- Touch→screen координаты зависят от SDL_RenderGetLogicalSize

### 18. Screen scaling

**Файл:** `src/Engine.cpp:1533-1545`

```cpp
// Fixed width 1280, height по aspect ratio устройства
int logicalH = 1280 * sh / sw;
SDL_RenderSetLogicalSize(renderer, 1280, logicalH);
```

**Проблемы:**
- Нет DPI awareness — UI элементы одинакового размера на 720p и 4K экранах
- Нет `SDL_HINT_RENDER_LOGICAL_SIZE_MODE` (integer vs smooth scaling)
- Font size фиксированный (например `m_ageText->pointSize = 13`)

### 19. File paths

**Файл:** `android/app/src/main/java/org/freeaoe/FreeAoEActivity.java:35-42`

```java
File dataDir = new File(Environment.getExternalStorageDirectory(), "Download/aoe2data");
return new String[]{ "--game-path=" + dataDir.getAbsolutePath(), "--language=ru" };
```

**Проблемы:**
- Hardcoded path `/sdcard/Download/aoe2data`
- `getExternalStorageDirectory()` deprecated на Android 10+
- Hardcoded `--language=ru`
- Нет fallback если данных нет

### 20. Audio mutex

**Файл:** `src/audio/AudioPlayer.cpp:71-76`

`mp3StopCallback` fires on SDL audio thread. Использует `try_lock()` — если main thread держит mutex, callback silently fails. Может терять audio events.

### 21. Memory

- Texture cache `m_textures` (TerrainSprite.h:81) растёт без ограничений
- Нет cache eviction policy
- На Android с 2-4GB RAM — может вызвать OOM на длинных сессиях

### 22. Missing SDL hints

```cpp
// Уже установлены:
SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");  // ✅
SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");                       // ✅
SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");                       // ✅

// Отсутствуют:
SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");     // Back button handling
SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "0");       // Background behavior
SDL_SetHint(SDL_HINT_RENDER_LOGICAL_SIZE_MODE, "1");     // Integer scaling
```

---

## Справка: проекты и источники по реверсу AoE2

### Открытые движки
| Проект | Стек | Статус | Полезно для freeaoe |
|--------|------|--------|---------------------|
| **freeaoe** (sandsmark) | C++17, SFML, genieutils | Основа порта | — |
| **openage** (SFTtech) | C++20, Python3, Qt6, OpenGL, nyan | Геймплей нефункционален, упор на архитектуру | Документация форматов: `doc/media/` |
| **openempires** (jubalskaggs) | C99, SDL2 | На паузе (513 коммитов) | Сетевая модель |

### Инструменты и документация форматов
| Проект | Что даёт |
|--------|----------|
| **aoetwols** (aap, MIT) | C-парсеры: DRS (архивы), SLP (спрайты), BMP/PAL (палитры), ICM (освещение). Рендерит ландшафт. |
| **geniedoc** (aap) | Документация рендеринга ландшафта Genie Engine: stemplet.dat (шаблоны тайлов), view_icm (изометрическая цветовая модель, Matt Pritchard), blendomatic (наложение текстур), lightmaps/filtermaps, tileedge/blkedge, patternmasks |

### Статьи по реверсу
- **RedRocket** (redrocket.club/posts/age_of_empires/) — реверс мультиплеера AoE2:DE:
  - Lock-step модель: каждый клиент симулирует независимо, команды через 2 тика
  - Код DE 2019 ~ оригинал 1997
  - RTTI-иерархия классов: `AVTRIBE_Combat_Object` → `AVRGE_Combat_Object` (полезно для понимания архитектуры)
  - Метод: CheatEngine → RTTI → vtable → breakpoints
  - Уязвимость: нет серверных проверок команд
- **Хабр** (habr.com/ru/articles/500542/) — AoE **III** (не II), патчинг шейдеров Mac-порта через Hopper+LLDB

### Что из этого помогает с P0-P4

**P0 (Gathering):** openage docs (`doc/media/`) содержат описание форматов .dat включая task lists юнитов — может помочь понять почему `findMatchingTask` не находит gather tasks. Advanced Genie Editor визуально показывает все tasks для каждого юнита.

**P1 #4 (Elevation/slopes):** geniedoc документирует stemplet.dat, blendomatic, tileedge — именно эти форматы отвечают за рендеринг склонов. aoetwols имеет рабочий C-код рендеринга ландшафта со склонами — можно использовать как референс.

**P2 #5 (PNG terrain):** geniedoc описывает оригинальную систему тайлов (SLP-based, не PNG). HD Edition использует PNG, но формат наложения (blendomatic) тот же.

**RTTI-иерархия из RedRocket:** классы движка `AVTRIBE_*` / `AVRGE_*` — можно сопоставить с genieutils структурами для проверки правильности парсинга .dat файлов.

---

## Общая таблица прогресса (обновлено после ночного прогона)

| Категория | Статус | Коммит |
|-----------|--------|--------|
| P0: Gathering | ✅ **ИСПРАВЛЕН** — touch→cursor→task chain починен | `0f0693a` |
| P1: AI | ✅ **Улучшен** — Stable, Siege, Knights, 30 юнитов, tech research | `51b1ea0` |
| P1: Age icon | ⚠️ Логи добавлены, нужно проверить на устройстве | `dabfd12` |
| P1: Elevation | ✅ Boundary checks добавлены, безопасно для re-enable | `4df66d3` |
| P2: PNG cache | ✅ **Сделано** — preload в конструкторе | `be91435` |
| P2: Text cache | ✅ **Сделано** — no createText per frame | `4e47a5f` |
| P2: Batching | ❌ Не начато (terrain offscreen, sprite grouping) | — |
| P3: Cursor | ✅ **Сделано** — Axe/Build/Protect/Garrison по ActionType | `2877cf9` |
| P3: Triggers | ✅ **48/49** — только AISignal остался | `602f556` + `2389e05` + `f1235be` |
| P3: Garrison | ✅ 7/9 (arrows, healing, capacity — всё работает) | — |
| P3: Walls/Gates | ❌ Не реализованы (hard) | — |
| P3: Pathfinding | ✅ A* полностью работает | — |
| P3: Combat | ✅ Damage formula + elevation bonus | — |
| P4: Android SDL | ✅ Back button + pause hints добавлены | `7356af7` |

**Оставшаяся работа:**
1. Проверить gathering на реальном устройстве (логи покажут где ломается)
2. Sprite batching (P2 #7) — terrain offscreen texture
3. Walls/Gates — gate state system
4. Minimap fog of war — верификация
5. Age icon — проверить на устройстве по логам
