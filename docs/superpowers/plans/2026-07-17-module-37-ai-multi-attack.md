# Module 37: AI Multi-Prong Attacks — Implementation Plan

> ALREADY IMPLEMENTED in this session.

**What was done:**
- Split idle military into 2 groups when army >= 12 units and 2 targets available
- 2/3 of army attacks primary target (enemy TC), 1/3 attacks secondary target (another building)
- Extracted common dispatch logic into lambda `sendToTarget`
- Falls back to single-group attack for < 12 units or single target

**File:** `src/ai/BasicAI.cpp:594-647`
