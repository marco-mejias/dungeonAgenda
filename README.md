# DUNGEON AGENDA ⚔️ - Tactical C++ Dungeon Crawler made in Godot / Jenova Framework

A tactical dungeon crawler where you lead a party of four heroes down a procedural dungeon, one floor at a time.
Every floor is built like a cave from a souls-like: a safe campfire at the entrance, enemy camps guarding their chests, and a locked boss room at the back with the stairs behind it. Explore freely, sneak past the guards reading their vision cones, or start a fight on your terms and roll the d20.
The core of the game (dungeon generation, vision, pathfinding, combat, inventory and rendering) is written in C++ through the **Jenova Framework**, which runs native C++ scripts inside Godot 4.7, while the higher level systems (stealth, tactics, progression, UI) are written in GDScript. This project was a deep dive into procedural generation, tactical systems, UI/UX and performance.

### The GIFs look a bit laggy due to compression techniques, sorry for the inconvenience

https://github.com/user-attachments/assets/a42db60c-42de-4637-9d8c-c5b1b3250283

<!-- Add a gameplay GIF here: drag the file into a GitHub issue or the README editor and paste the link -->

## 🛠️ Technologies

* Godot Engine 4.7
* Jenova Framework (C++ scripting for Godot)
* C++ / MSVC
* GDScript
* Godot Shading Language
* GitHub

---

## 💡 Core Elements

<!-- Add a GIF of exploring + the campfire here -->

I designed the game and built its systems, art direction, UI and feel. I would like to highlight:

### 1. Procedural Dungeons with Structure
Every floor is generated from scratch: big rooms in L and T shapes, corridors 1 to 3 tiles wide, loops, columns, two heights joined by steps, doors and decoration.
On top of the layout, a zoning pass gives each room a role: the **entrance** with a campfire to rest, **camps** with an enemy squad around a chest, and the **boss room**, sealed with a locked door. The key is always in the deepest camp's chest, and the generator checks that every room stays reachable (and that the boss room is only reachable with the key) before accepting the floor.
Days count: the longer you take, the more the dungeon stirs and sends reinforcements to the camps still standing. Rush for the boss or clear every zone?

### 2. Stealth & Vision Cones
Every enemy looks in one direction and has a role: **sentinels** never turn, **lookouts** turn every round (you can see their next cone as a ghost), and **sleepers** only wake up if you end next to them.
Their sight is drawn on the floor as a soft pulsing light cone, cut by walls. The cone is not just a visual: detection uses **the same rays** that draw it, so if your tile is inside the light, they see you. Heroes always see further than enemies, so you spot them first.
When a hero sees an unaware enemy the game enters **stealth mode**: turns and action points like in combat, but enemies only keep watch. You can hide against walls, put out your torch, and strike from behind for a guaranteed critical, or let them spot you and fight.

<!-- Add a GIF of the vision cones here -->

### 3. Tactical Combat
Combat is turn-based with action points and a d20 roll for every attack, shown on a small die over the attacker's head.
Positioning matters: **flanking** and **height** give advantage, **cover** gives disadvantage to ranged attacks, moving away from an enemy gives it an **attack of opportunity**, and pushes against walls or off high rooms stun and knock down.
Enemies act by AI profile (tank, melee, caster, coward) using BFS distance fields, and they come in **squads**: wake one up and the whole camp joins the fight.

### 4. Heroes, Progression & Items
Four classes with their own starting ability. Killing grants XP; on level up you **draft 1 of 3 abilities**, and repeated passives stack into combos.
Each hero has an inventory with weight and equipment slots that change their stats and their sprite. The backpack is a physical sack where items fall, stack and can be thrown, and everything is data-driven: items, enemies, loot tables, events and abilities are `.tres` resources, editable from a custom **database panel** inside the editor.
Survival is still there: hunger, sanity, status effects (burning, bleeding, stunned...) and narrative events resolved with a d20 roll on one of your stats.

### 5. UI & Game Feel
The HUD is a dark grimoire made of loose leather pieces: the hero plate, the hand of ability cards, the belt of consumables and a wax seal to end the turn. Everything reacts to the mouse, damage shows as "ghost health", and the bottom slides away when an event opens.
On top sits a layer of juice: hit stop, screen shake, zoom punches, floating indicators and loot flying to the hero card. The world uses a PS1-style shader with vertex snapping, affine textures, per-pixel line of sight with cone-shaped shadows, and a dithered fog of war.

---

## ⚙️ Game Architecture & Systems

The game is split into **C++ managers** (the core, with the heavy logic) and **GDScript autoloads** (the rules on top, easy to iterate).

**C++ (Jenova):**
* **DungeonManager:** The brain. Owns the logic grid, generates the floor, processes clicks and coordinates the other managers.
* **Mazmorra:** The pure-C++ dungeon model: generation, zones, vision and fog, line of sight. It knows nothing about Godot nodes.
* **DungeonRenderer:** The "visual hand". Builds the 3D world with MultiMeshes and paints overlays, fog and bubbles, with no game logic.
* **PathManager:** AStarGrid2D with the static walls loaded once per floor; dynamic entities are injected only while a path is computed.
* **PartyManager / EnemyManager / CombatManager:** Turn flow, survival, the enemy turn and the animated attack sequences.
* **CameraManager & InputManager:** Pseudo-isometric camera anchored to the active hero, combat framing, and mouse-to-tile picking.

**GDScript autoloads:**
* **Modo, Sigilo, Tactica, Zonas:** Exploring/stealth/combat modes, vision cones and detection, positioning rules and floor zones.
* **Progreso, Estados, Especiales:** XP, levels and abilities, status effects and the more chaotic items.
* **Catalogo, Estilo, Feel, Indicadores...:** Data loading, the UI theme, game feel and floating indicators.

### Performance
Calling C++ from GDScript in Jenova costs around 0.8 ms per call, so the design keeps that boundary thin: the static map is fetched once and versioned, line of sight is mirrored in GDScript (validated against the C++ version on every pair of tiles), and per-frame queries are cached.
* HUD: from ~18 to ~75 FPS.
* Vision cones: from ~460 ms to ~3 ms per rebuild.
* Detection: from 14 ms to 1.3 ms.

### Working with Jenova
Jenova compiles C++ scripts attached to Godot nodes from inside the editor. Its rules shaped the code: the state of each entity lives in node metadata, shared logic lives in header-only modules (`Scripts/CPP/...`), and functions exposed to `call()` avoid parameter types that the bridge doesn't handle well.

### Testing
A playable smoke test (`Tools/test_play.gd`) launches the real game, plays through exploration, inventory, stealth, combat, events, level ups and a floor change, and checks around 180 conditions while saving screenshots.

---

## 🎮 Controls

| Input | Action |
|---|---|
| Left click | Move / interact / attack |
| Right click (hold) | Look around |
| Q / E · Wheel | Rotate camera · Zoom |
| 1 · 2 · R | Attack · Help an ally · Retreat |
| H · T | Hide · Torch on/off |
| Z X C V | Abilities |
| 5 - 9 | Consumables |
| 3 · 4 | End turn · End day / rest |
| I · M | Backpack · Minimap |
| Shift (hold) | Full stats on hover |
| Esc · F11 | Pause · Fullscreen |

---

## 🚀 Build Instructions

**Prerequisites:**
* Windows x64
* Godot 4.7 with the [Jenova Framework](https://github.com/Jenova-Framework/J.E.N.O.V.A) (MSVC build)

**Steps:**
1. Clone the repository.
2. Open the project folder with the Jenova build of Godot 4.7.
3. Open Jenova's **Package Manager** and install the packages the project lists (the MSVC toolchain and the Godot SDK).
4. Click **Jenova → Build** to compile the C++ scripts.
5. Run the project (F5). The main scene is `Scenes/menu_principal.tscn`.

Sounds are not included (third-party licences, see [CREDITS.md](CREDITS.md)); the game runs silent without them.
