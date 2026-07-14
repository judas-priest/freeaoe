# Gameplay Tech Debt

**Реальный статус: ~30% до играбельности. 73% была ложь — считал по количеству фич, а не по важности.**

---

## P0 — БЛОКЕРЫ (без этого игра неиграбельна)

### 1. Сбор ресурсов — НИЧЕГО не работает

Gathering НИКОГДА не работал. Ни на десктопе (ПКМ), ни на Android (лонг-тап). Все 5 типов:

1. **Мясо** — овцы/олени/кабаны: крестьянин убивает но не собирает
2. **Ягоды** — кусты: клик ничего не делает
3. **Золото** — жилы: клик ничего не делает
4. **Камень** — жилы: клик ничего не делает
5. **Дерево** — рубка: клик ничего не делает

**Последствия:**
- Экономика мертва — после стартовых ресурсов ничего нельзя сделать
- AI тоже не собирает — `assignIdleVillagers` вызывает `findTaskWithTarget` который тоже сломан
- Нельзя построить ничего после первого дома
- Нельзя натренировать юнитов после стартовых
- Игра заканчивается через 2 минуты когда ресурсы кончаются

**План отладки:**
1. Добавить логи в цепочку ПКМ → task → gather:
   - `UnitManager::onRightClick` — есть ли `m_tasksUnderCursor`?
   - `UnitManager::onCursorPositionChanged` → `forEachUnitAt` — находит ли ресурс под курсором?
   - `UnitActionHandler::findTaskWithTarget` → `findMatchingTask` — находит ли gather task? Почему нет?
2. Проверить `OutlineSize` у ресурсов — если 0, ellipse hit test не найдёт их
3. Проверить `TargetDiplomacy` у gather tasks — Gaia ресурсы должны матчить
4. Проверить `canMatchGenieUnitID` — может villager task list не содержит gather tasks

**Где копать:**
- `src/mechanics/UnitManager.cpp:551-600` — onRightClick, m_tasksUnderCursor
- `src/mechanics/UnitManager.cpp:901-917` — forEachUnitAt (ellipse hit test)
- `src/mechanics/UnitActionHandler.cpp:75-180` — findTaskWithTarget → findMatchingTask
- `src/actions/IAction.cpp:53-93` — assignTask для Hunt/GatherRebuild
- `src/actions/ActionGather.cpp` — сам процесс сбора

---

## P1 — Серьёзные проблемы

### 2. AI не собирает ресурсы
- `BasicAI::assignIdleVillagers()` использует `findTaskWithTarget` — тот же сломанный код
- Даже если пофиксить gathering для игрока, AI останется без экономики
- **Фикс:** починить P0, AI заработает автоматически

### 3. Иконка века на Android
- IV (Imperial) показывается вместо II (Feudal)
- IconID=30 правильный для десктопа, неправильный на Android
- **Нужно:** debug лог на Android, проверить какой SLP загружен для Technology icons

### 4. Elevation/slopes отключены
- Terrain только плоский — slopes вызывают чёрные тайлы
- Workaround: `tile.elevation = 2` для всех тайлов в RandomMapGenerator
- **Причина:** filtermap/slpTemplate lookup для некоторых slope combinations возвращает мусор
- **Нужно:** debug какие slope values вызывают проблему, возможно ограничить elevation range

---

## P2 — Performance

### 5. PNG terrain с диска
- Каждый новый тайл загружает PNG файл с диска через stbi_load
- На random map с 144x144 тайлами = тысячи загрузок
- **Фикс:** кэшировать PNG в память при первой загрузке

### 6. createText() каждый кадр
- `drawUi()` создаёт новые Text объекты каждый кадр для ресурсов, fps, age и т.д.
- **Фикс:** создать Text один раз, обновлять только string

### 7. Нет sprite batching
- Каждый юнит/здание = отдельный SDL_RenderCopy вызов
- **Фикс:** группировать по текстуре, батчить

---

## P3 — Мелочи

### 8. Сбор ресурсов — drop-off
- Даже если gathering заработает, нужно проверить что drop-off (несёт ресурсы в TC/Lumber Camp/Mill/Mining Camp) работает корректно

### 9. Cursor context
- Курсор не меняется при наведении на ресурс (должен показывать иконку сбора)

### 10. Scenario editor triggers
- 26 из 49 реализованы, см. `2026-07-14-scenario-triggers-techdebt.md`
- 8 easy wins за ~2-3 часа откроют большинство кампаний
