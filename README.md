# DUNGEON AGENDA ⚔️ - Tactical RPG & C++ Framework

A top-down 2D action and adventure CRPG built in Godot 4.6 utilizing a high-performance C++ backend alongside GDScript. Where every decision counts! Control the 4 members of your party and descend into an (almost) infinite procedural dungeon. But be careful: the dungeon is plagued with monsters, treasures, traps, and hundreds of terrifying creatures. Gather resources, sharpen your weapons, manage your party's sanity, and take a nap from time to time to unravel the mysteries and make your way out.

The game is based in survival Point and Click games like 60 Seconds or Reigns, while offering a twist inspired in classical RPG games.

## 🛠️ Technologies 

* **Game Engine:** Godot Engine 4.6
* **Languages:** C++ (Core Logic) & GDScript (Input/High-level routing)
* **Framework:** Jenova SDK (Native C++ integration for Godot)
* **Version Control:** Git & GitHub

---

https://github.com/user-attachments/assets/a42db60c-42de-4637-9d8c-c5b1b3250283

## 🧩 Core Gameplay Mechanics

Dungeon Agenda blends tactical grid-based exploration with intense survival management.

### 1. Tactical Grid Movement & Exploration
The game relies on a turn-based procedural grid. Characters navigate the dungeon using an action-point system based on their Speed stat. The map is covered in a dense *Fog of War*, requiring players to strategically position their characters to reveal paths, avoid traps, and find the exit stairs.

### 2. Deep Survival System
Time is your worst enemy. As turns pass, characters consume Food. If a character starves, they die permanently. Furthermore, characters suffer from Isolation Sanity Loss—if a party member ends their turn too far from the rest of the group, their sanity drops drastically, severely halving their stats and making subsequent events much harder to overcome.

### 3. Risk/Reward Interactive Events
Stepping on specific dungeon tiles triggers narrative events. The player must choose an action that relies on a specific character stat (Strength, Magic, Luck...). The system calculates a success probability and performs a visual "Dice Roll". Success grants loot or heals, while failure heavily penalizes the character's sanity or HP. NOTE: The game has visual squares indicating where the events are. This was for debugging purposes, but shall not be included in the final product.

### 4. The Campfire
Upon successfully escaping a floor with the surviving party members, players enter the Campfire phase. Here, the party rests, slightly recovers HP, Sanity, and Food, and prepares for the generation of the next, more difficult floor.

---

## ⚙️ Engine Architecture & C++ Systems

### 1. Modular Manager Pattern
The core game loop is completely decoupled, dividing responsibilities into highly specialized C++ singletons:
* **DungeonManager (Environment & Navigation):** The backbone of the procedural world. It computes grid generation, manages the `AStarGrid2D` pathfinding, and executes complex Raycasting algorithms to calculate strict Line-of-Sight (LoS) for the Fog of War in real-time.
* **PartyManager (State & Logic):** The "Brain" of the game. Entirely agnostic to the visual representation, this module tracks character stats, inventory, survival metrics (hunger/sanity), and turn flow. It acts as the single source of truth for the game state.
* **HUDManager (UI & Feedback):** A purely reactive visual director. It listens to state changes from the PartyManager and updates the interface smoothly using Godot's Tween engine, maintaining a clean separation between data and presentation.

### 2. Hybrid Codebase (C++ & GDScript)
The project utilizes a "best of both worlds" approach. Computationally heavy tasks (like procedural algorithms, visibility checks, and deep state management) are written in **C++** for maximum optimization. Meanwhile, **GDScript** is used exclusively as a lightweight router for handling user input and high-level scene composition, keeping iteration times fast. Although, the decision to use C++ was at first purely for learning purposes.

### 3. Data-Driven Design
Content creation is highly flexible thanks to a data-driven approach using Godot's `Resource` system (`.tres` files). Characters, items, and narrative events are defined as external data objects. This allows designers to add new encounters, tweak enemy stats, or create new loot without ever needing to touch or recompile the C++ source code.

### 4. Robust Input & Event Routing
The engine employs a sophisticated input interception layer to manage the overlap between the World and the User Interface. UI components act as shields: clicks on HUD elements are consumed instantly, preventing the underlying C++ pathfinding algorithms from triggering invalid actions. This guarantees a fluid, bug-free tactical experience.

---

## 🚀 Roadmap & Future Improvements

While the core exploration and survival loops are fully functional, the game is in a REEEEALLLYYYY early stage, working more like a technical demo. While the game keeps advancing, changes will be made like:

* **Advanced Procedural Generation and Movement:** Moving from random placement to structured algorithms (BSP Trees or Cellular Automata) for more organic room and corridor generation, while also improving the movement and vision algorythm.
* **Equipment & Inventory Expansion:** While not very useful, the game features an inventory system, which will be a main mechanic in the feature for equipping specific gear (Weapons, Armor, Accessories). In the future, items will be actually useful and while in the campfire, characters will be able to exchange gear.
* **Audio Engine Integration:** Connecting an event-driven audio system for UI clicks, dice rolls, footsteps, background music....
* **MVC Arquitecture:** While the game feaures a core manager pattern, currently is a bit of spaghetti code...But another build is currently being made in paral·lel featuring a MVC model
* **AND MORE OF EVERYTHING:** Actually the game features only 3 types of events and one item. While not very exciting, because the easy making architecture and incorporation, plans are made to add hundreds of them.

---

## 💻 Build & Installation Instructions

**Prerequisites:**
* Godot Engine 4.x (Tested on 4.6)
* Visual Studio Community (with Desktop development with C++ workload)
* [Jenova SDK Plugin](https://github.com/jenova-project/Jenova) installed in your Godot editor.

**Setup Steps:**
1. Clone this repository to your local machine

2. Ensure the Jenova Plugin is enabled in Project > Project Settings > Plugins.

3. The C++ scripts (.cpp) are attached directly to the Manager nodes. Jenova will automatically compile them in the background.

4. Wait for the Jenova compiler console to output [Jenova] Build Successful.

5. Press F5 (or the Play button) in Godot to run the game!
