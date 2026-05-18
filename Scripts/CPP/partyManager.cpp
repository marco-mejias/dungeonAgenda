/* Jenova C++ Node Base Script (Meteora) - PartyManager */

#include <Godot/godot.hpp>
#include <Godot/classes/node.hpp>
#include <Godot/variant/utility_functions.hpp>
#include <Godot/classes/node2d.hpp>
#include <Godot/classes/canvas_layer.hpp>

using namespace godot;
using namespace jenova::sdk;

namespace {
	Node* self = nullptr;
	Node* act_char = nullptr;
	
	Node* m_ui_manager = nullptr;
	Node* m_dungeon_manager = nullptr;
}

JENOVA_SCRIPT_BEGIN

// ==========================================
// ACTIVE CHARACTER MANAGEMENT
// ==========================================

Variant GetActiveCharacter(Caller* instance) {
	return act_char; 
}

void SetActiveCharacter(Caller* instance, Node* new_char) {
	if (!new_char) return;
	
	bool has_moved = new_char->has_meta("ha_movido") ? (bool)new_char->get_meta("ha_movido") : false;
	
	if (!has_moved) {
		act_char = new_char;
		UtilityFunctions::print("[PartyManager] Active character: ", new_char->get_meta("nombre_display"));
	} else {
		UtilityFunctions::print("[PartyManager] ", new_char->get_meta("nombre_display"), " is exhausted this turn.");
	}
}

Variant GetCurrentStat(Caller* instance, String stat_name) {
	if (!act_char || !act_char->has_meta(stat_name)) return Variant();
	return act_char->get_meta(stat_name);
}

Vector2i GetActiveGridPos(Caller* instance) {
	if (!act_char || !act_char->has_meta("grid_pos")) return Vector2i(0, 0);
	return act_char->get_meta("grid_pos");
}

void SetActiveGridPos(Caller* instance, int x, int y) {
	if (!act_char) return;
	act_char->set_meta("grid_pos", Vector2i(x, y));
}

// ==========================================
// TURN FLOW MANAGEMENT
// ==========================================

void TerminarTurnoPersonaje(Caller* instance) {
	if (!self || !act_char) return;

	act_char->set_meta("ha_movido", true);
	UtilityFunctions::print("[PartyManager] Action completed for ", act_char->get_meta("nombre_display"));

	TypedArray<Node> children = self->get_children();
	bool all_have_moved = true;
	
	// 1. CHECK IF THE DAY ENDS
	for (int i = 0; i < children.size(); i++) {
		Node* child = Object::cast_to<Node>(children[i]);
		if (child && child->get_name() != StringName("DatosEquipo")) {
			bool moved    = child->has_meta("ha_movido")   ? (bool)child->get_meta("ha_movido")   : false;
			bool escaped  = child->has_meta("ha_escapado") ? (bool)child->get_meta("ha_escapado") : false;
			bool is_dead  = child->has_meta("esta_muerto") ? (bool)child->get_meta("esta_muerto") : false; 
			
			// If anyone hasn't moved, hasn't escaped, and isn't dead... the global turn continues
			if (!moved && !escaped && !is_dead) {
				all_have_moved = false;
				break;
			}
		}
	}

	// 2. IF EVERYONE HAS MOVED (NIGHT FALLS)
	if (all_have_moved) {
		UtilityFunctions::print("[PartyManager] All characters have moved.");
		
		// Add turn to memory
		int current_turn = self->get_meta("turno_actual");
		current_turn += 1;
		self->set_meta("turno_actual", current_turn);
		
		// SURVIVAL LOGIC
		for (int i = 0; i < children.size(); i++) {
			Node* child = Object::cast_to<Node>(children[i]);
			if (child && child->get_name() != StringName("DatosEquipo")) {
				bool escaped = child->has_meta("ha_escapado") ? (bool)child->get_meta("ha_escapado") : false;
				bool is_dead = child->has_meta("esta_muerto") ? (bool)child->get_meta("esta_muerto") : false;

				if (!escaped && !is_dead) {
					// 1. HUNGER: Subtract 1
					int hunger = (int)child->get_meta("hambre_actual");
					hunger -= 1;
					if (hunger <= 0) {
						hunger = 0;
						child->set_meta("esta_muerto", true);
						UtilityFunctions::print("[Survival] ", child->get_name(), " HAS STARVED TO DEATH.");
						
						// Hide visual token on the map
						Node* main_node = self->get_parent();
						if (main_node) {
							Node2D* visual_token = Object::cast_to<Node2D>(main_node->get_node_or_null(NodePath("DungeonManager/" + String(child->get_name()))));
							if (visual_token) visual_token->set_visible(false);
						}
					}
					child->set_meta("hambre_actual", hunger);

					// 2. ISOLATION SANITY (If they just survived hunger)
					if (hunger > 0) {
						Vector2i my_pos = child->get_meta("grid_pos");
						int min_dist = 999;
						
						for (int j = 0; j < children.size(); j++) {
							if (i == j) continue; // Don't count oneself
							Node* companion = Object::cast_to<Node>(children[j]);
							if (companion && companion->get_name() != StringName("DatosEquipo")) {
								bool c_escaped = companion->has_meta("ha_escapado") ? (bool)companion->get_meta("ha_escapado") : false;
								bool c_dead    = companion->has_meta("esta_muerto") ? (bool)companion->get_meta("esta_muerto") : false;
								
								if (!c_escaped && !c_dead) {
									Vector2i c_pos = companion->get_meta("grid_pos");
									// Calculate distance in tiles (Manhattan)
									int dist = abs(my_pos.x - c_pos.x) + abs(my_pos.y - c_pos.y);
									if (dist < min_dist) min_dist = dist;
								}
							}
						}
						
						// If the closest companion is more than 3 tiles away, panic ensues
						if (min_dist > 3 && min_dist != 999) {
							int sanity = (int)child->get_meta("sanidad_actual");
							child->set_meta("sanidad_actual", UtilityFunctions::max(0, sanity - 15));
							UtilityFunctions::print("[Survival] ", child->get_name(), " loses 15 sanity due to isolation.");
						}
						
						// Reset movement ONLY if still alive
						child->set_meta("ha_movido", false);
					}
				}
			}
		}
		
		UtilityFunctions::print("[PartyManager] New day. Current turn: ", current_turn);
		
		if (m_ui_manager) {
			m_ui_manager->call("ActualizarMarcadorTurno");
			m_ui_manager->call("ActualizarStatsUI");
		}
	} 
	
	// 3. AUTOMATIC SELECTION OF THE NEXT ALIVE CHARACTER
	act_char = nullptr; 
	for (int i = 0; i < children.size(); i++) {
		Node* companion = Object::cast_to<Node>(children[i]);
		if (companion && companion->get_name() != StringName("DatosEquipo")) {
			bool c_moved   = companion->has_meta("ha_movido")   ? (bool)companion->get_meta("ha_movido")   : false;
			bool c_escaped = companion->has_meta("ha_escapado") ? (bool)companion->get_meta("ha_escapado") : false;
			bool c_dead    = companion->has_meta("esta_muerto") ? (bool)companion->get_meta("esta_muerto") : false;

			if (!c_moved && !c_escaped && !c_dead) {
				act_char = companion; // Select the first one that can still play
				break;
			}
		}
	}

	// 4. FINAL UI UPDATE
	if (m_ui_manager) m_ui_manager->call("UpdateWarriorUI");
}

void CambiarPersonajeActivo(Caller* instance, String new_name) {
	if (!self) return;
	
	Node* child = self->get_node_or_null(NodePath(new_name));
	
	if (child) {
		
		bool is_dead = child->has_meta("ha_muerto") ? (bool)child->get_meta("ha_muerto") : false;
		if (is_dead) {
			UtilityFunctions::print("[PartyManager] ", new_name, " is dead.");
			return;
		}
		
		bool has_moved = child->has_meta("ha_movido") ? (bool)child->get_meta("ha_movido") : false;
		if (has_moved) {
			UtilityFunctions::print("[PartyManager] ", new_name, " has already moved. Choose another.");
			return;
		}
		
		bool has_escaped = child->has_meta("ha_escapado") ? (bool)child->get_meta("ha_escapado") : false;
		if (has_escaped) {
			UtilityFunctions::print("[PartyManager] ", new_name, " is already waiting on the floor below.");
			return;
		}
		
		act_char = child;
		UtilityFunctions::print("[PartyManager] Manual selection: ", act_char->get_meta("nombre_display"));

		if (m_ui_manager) m_ui_manager->call("UpdateWarriorUI");
		
		if (m_dungeon_manager) {
			m_dungeon_manager->call("UpdateHighlightForWarrior", act_char);
		}
	}
}

// ==========================================
// CUSTOM EVENTS
// ==========================================

void RecibirDano(Caller* instance, int damage_amount, Node* target_hero) {
	UtilityFunctions::print("Taking damage....");
	if (!self) return;

	String name = target_hero->get_meta("nombre_display");

	int current_hp = 0;
	if (target_hero->has_meta("hp_actual")) {
		current_hp = (int)target_hero->get_meta("hp_actual");
	}

	current_hp -= damage_amount;
	if (current_hp < 0) current_hp = 0;

	// Save target HP
	target_hero->set_meta("hp_actual", current_hp);
	UtilityFunctions::print("[PartyManager] Effect: ", name, " takes ", damage_amount, " damage. Remaining HP: ", current_hp);

	// Refresh stats
	Node* ui = self->get_parent()->get_node_or_null("CanvasLayer");
	if (ui) ui->call("ActualizarStatsUI"); 
}

void PersonajeEscapa(Caller* instance, Node* hero) {
	if (!self || !hero) return;
	
	hero->set_meta("ha_escapado", true);
	UtilityFunctions::print("[PartyManager] ", hero->get_meta("nombre_display"), " has gone down the stairs.");

	// Check if EVERYONE has escaped
	TypedArray<Node> children = self->get_children();
	bool everyone_escaped = true;
	
	for (int i = 0; i < children.size(); i++) {
		Node* child = Object::cast_to<Node>(children[i]);
		if (child && child->get_name() != StringName("DatosEquipo")) {
			bool escaped = child->has_meta("ha_escapado") ? (bool)child->get_meta("ha_escapado") : false;
			if (!escaped) {
				everyone_escaped = false;
				break;
			}
		}
	}

	if (everyone_escaped) {
		UtilityFunctions::print("\n***********************************");
		UtilityFunctions::print(" EVERYONE SAFE! STARTING CAMPFIRE  ");
		UtilityFunctions::print("***********************************\n");
		
		CanvasLayer* campfire_layer = Object::cast_to<CanvasLayer>(self->get_parent()->get_node_or_null("CanvasLayer3"));
		if (campfire_layer) campfire_layer->set_visible(true);
	}
}

// ==========================================
// INITIALIZATION
// ==========================================

void OnAwake(Caller* instance) {
	self = GetSelf<Node>(instance);
	
	if (self) {
		Node* parent_node = self->get_parent();
		if (parent_node) {
			m_ui_manager      = parent_node->get_node_or_null("CanvasLayer");
			m_dungeon_manager = parent_node->get_node_or_null("DungeonManager");
		}
	}
}

void OnReady(Caller* instance) {
	if (!self) return;

	Node* team_data = self->get_node_or_null("DatosEquipo");
	if (!team_data) return;

	TypedArray<Resource> hero_list = team_data->get("lista_heroes");
	Node* dungeon = self->get_parent()->get_node_or_null("DungeonManager");

	for (int i = 0; i < hero_list.size(); i++) {
		Ref<Resource> res = hero_list[i];
		if (res.is_null()) continue;

		String name          = res->get("nombre_display");
		int hp               = res->get("vida_max");
		int spd              = res->get("velocidad");
		int def              = res->get("defensa");
		int str              = res->get("fuerza");
		int mag              = res->get("magia");
		int lck              = res->get("suerte");
		String portrait_path = res->get("retrato_path"); 


		Node* soul = memnew(Node);
		soul->set_name(name);
		soul->set_meta("nombre_display", name); 
		soul->set_meta("hp_max", hp);
		soul->set_meta("hp_actual", hp);
		soul->set_meta("velocidad", spd);
		soul->set_meta("retrato_path", portrait_path);
		soul->set_meta("ha_movido", false);
		soul->set_meta("defensa", def);
		soul->set_meta("fuerza", str);
		soul->set_meta("magia", mag);
		soul->set_meta("suerte", lck);
		soul->set_meta("hambre_max", 7);
		soul->set_meta("hambre_actual", 7);
		soul->set_meta("sanidad_max", 100);
		soul->set_meta("sanidad_actual", 100);
		soul->set_meta("esta_muerto", false);
		soul->set_meta("ha_escapado", false);
		
		// Currency and food variables (Shared or individual, here individual)
		soul->set_meta("oro", 0);
		soul->set_meta("comida_bolsa", 0);

		// SCALABLE SLOT CONFIGURATION
		int slots = 3; 
		if (soul->get_name() == StringName("Doom_Snailer")) {
			slots = 4; // The tank is a pack mule
		}
		soul->set_meta("max_slots", slots);

		// The inventory is a Godot Array that will store "ItemData"
		TypedArray<Resource> inventory;
		soul->set_meta("items_lista", inventory);

		self->add_child(soul); 
		
		// Initialize floor and turn memory
		self->set_meta("piso_actual", 1);
		self->set_meta("turno_actual", 1);

		if (dungeon) {
			dungeon->call("SpawnVisualCharacter", res, i); 
		}
	}
}

JENOVA_SCRIPT_END
