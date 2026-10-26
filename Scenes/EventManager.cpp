/* Jenova C++ Node Base Script (Meteora) - EventManager */

#include <Godot/godot.hpp>
#include <Godot/classes/node.hpp>
#include <Godot/classes/node2d.hpp>
#include <Godot/classes/resource_loader.hpp>
#include <Godot/classes/resource.hpp>
#include <Godot/classes/label.hpp>
#include <Godot/classes/tween.hpp>
#include <Godot/classes/property_tweener.hpp>
#include <Godot/classes/callback_tweener.hpp>
#include <Godot/classes/interval_tweener.hpp>
#include <Godot/classes/control.hpp>
#include <Godot/classes/window.hpp>
#include <Godot/variant/utility_functions.hpp>

using namespace godot;
using namespace jenova::sdk;

JENOVA_SCRIPT_BEGIN

void OnAwake(Caller* instance) {}
void OnDestroy(Caller* instance) {}
void OnReady(Caller* instance) {}

// === EL NÚCLEO DE EVENTOS ===
void Interactuar(Caller* instance, godot::Array paquete) {
	// Si el paquete no tiene los 5 datos, abortamos por seguridad
	if (paquete.size() < 5) return;

	// 1. Abrimos la caja y sacamos cada cosa
	int x = paquete[0];
	int y = paquete[1];
	godot::String tipo = paquete[2];
	godot::String res_path = paquete[3];
	bool debug_mode = paquete[4];

	godot::Node* self_node = GetSelf<godot::Node>(instance);
	godot::Node* padre_dungeon = self_node->get_parent();
	godot::Node* pm = self_node->get_node_or_null("/root/Main/PartyManager");
	godot::Node* ui_manager = self_node->get_node_or_null("/root/Main/CanvasLayer");

	if (!padre_dungeon || !pm) return;

	// 2. Cast Ultra-Seguro del personaje activo
	godot::Variant char_var = pm->call("GetActiveCharacter");
	Node* act_char = nullptr;
	if (char_var.get_type() != godot::Variant::NIL) {
		act_char = godot::Object::cast_to<Node>(char_var.operator godot::Object*());
	}
	
	if (!act_char) {
		godot::UtilityFunctions::print("[EventManager] Error: Personaje no encontrado.");
		return;
	}

	int coste_interaccion = 2; 
	if (debug_mode) coste_interaccion = 0;

	int pa = act_char->get_meta("pa_actual");
	if (pa < coste_interaccion) {
		godot::UtilityFunctions::print("[EventManager] Necesitas ", coste_interaccion, " PA para interactuar.");
		Node* audio = self_node->get_node_or_null("/root/AudioManager");
		if (audio) audio->call("play_ui", "no");
		return;
	}

	bool interactuo = false;

	// === A. LÓGICA DE COFRES ===
	if (tipo == "COFRE") {
		int roll = godot::UtilityFunctions::randi() % 3; 
		if (roll == 0) {
			int gold_gained = (godot::UtilityFunctions::randi() % 20) + 10;
			int current = act_char->has_meta("oro") ? (int)act_char->get_meta("oro") : 0;
			act_char->set_meta("oro", current + gold_gained);
		} else if (roll == 1) {
			int food_gained = (godot::UtilityFunctions::randi() % 2) + 1;
			int current = act_char->has_meta("comida_bolsa") ? (int)act_char->get_meta("comida_bolsa") : 0;
			act_char->set_meta("comida_bolsa", current + food_gained);
		} else {
			TypedArray<Resource> inv = act_char->has_meta("items_lista") ? (TypedArray<Resource>)act_char->get_meta("items_lista") : TypedArray<Resource>();
			int max_s = act_char->has_meta("max_slots") ? (int)act_char->get_meta("max_slots") : 3;
			if (inv.size() < max_s) {
				Ref<Resource> new_item = godot::ResourceLoader::get_singleton()->load("res://Data/Items/Espada.tres");
				if (new_item.is_valid()) {
					inv.push_back(new_item);
					act_char->set_meta("items_lista", inv);
				}
			}
		}
		// Avisamos al DungeonManager para que pinte el cofre abierto
		padre_dungeon->call("AplicarEstadoCofre", x, y);
		interactuo = true;
	}
	// === B. LÓGICA DE PUERTAS ===
	else if (tipo == "PUERTA") {
		godot::Variant vx = x;
		godot::Variant vy = y;
		padre_dungeon->call("AplicarEstadoPuerta", vx, vy);
		interactuo = true;
	}
	// === C. LÓGICA DE EVENTOS (Tu animación de Texto) ===
	else if (tipo == "EVENTO") {
		godot::Ref<godot::Resource> loaded_event = godot::ResourceLoader::get_singleton()->load(res_path);
		if (loaded_event.is_valid() && ui_manager) {
			padre_dungeon->set_meta("bloqueo_evento", true);

			Label* event_text = memnew(Label);
			event_text->set_text("EVENTO");
			event_text->add_theme_font_size_override("font_size", 120);
			event_text->add_theme_color_override("font_color", Color(1.0, 0.1, 0.1));
			event_text->set_horizontal_alignment(godot::HORIZONTAL_ALIGNMENT_CENTER);
			event_text->set_vertical_alignment(godot::VERTICAL_ALIGNMENT_CENTER);

			ui_manager->add_child(event_text);

			Vector2 screen_size = ui_manager->get_viewport()->get_visible_rect().get_size();
			event_text->set_size(Vector2(screen_size.x, 200));
			event_text->set_position(Vector2(0, screen_size.y + 100));

			ui_manager->set_meta("heroe_en_peligro", act_char);

			Ref<Tween> tween = self_node->create_tween();
			tween->tween_property(event_text, "position:y", screen_size.y / 2.0 - 100.0, 0.4)->set_trans(Tween::TRANS_EXPO)->set_ease(Tween::EASE_OUT);
			tween->tween_interval(0.6);
			tween->tween_property(event_text, "position:y", -300.0, 0.3)->set_trans(Tween::TRANS_EXPO)->set_ease(Tween::EASE_IN);
				
			Callable show_panel = Callable(ui_manager, StringName("MostrarEventoVisual")).bind(loaded_event);
			tween->parallel()->tween_callback(show_panel);
			tween->tween_callback(Callable(event_text, StringName("queue_free")));
			
			godot::Variant vx = x;
			godot::Variant vy = y;
			padre_dungeon->call("AplicarEstadoEvento", vx, vy);
			interactuo = true;
		}
	}

	// === D. GASTO DE PA Y ACTUALIZACIÓN ===
	if (interactuo) {
		Node* audio = self_node->get_node_or_null("/root/AudioManager");
		if (audio) {
			godot::Variant v_sonido = godot::String("clic");
			audio->call("play_ui", v_sonido);
		}

		act_char->set_meta("pa_actual", pa - coste_interaccion);
		
		if (ui_manager) ui_manager->call("UpdateWarriorUI");
		if (pa - coste_interaccion <= 0) pm->call("TerminarTurnoPersonaje");
		
		// Le pedimos al Dungeon que vuelva a calcular las luces amarillas
		padre_dungeon->call("RefrescarLuces");
	}
}

JENOVA_SCRIPT_END
