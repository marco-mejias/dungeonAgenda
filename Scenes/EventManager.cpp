/* Jenova C++ Node Base Script (Meteora) - EventManager */

#include <Godot/godot.hpp>
#include <Godot/classes/node.hpp>
#include <Godot/classes/resource_loader.hpp>
#include <Godot/classes/resource.hpp>
#include <Godot/classes/label.hpp>
#include <Godot/classes/tween.hpp>
#include <Godot/classes/property_tweener.hpp>
#include <Godot/classes/callback_tweener.hpp>
#include <Godot/classes/interval_tweener.hpp>
#include <Godot/classes/viewport.hpp>
#include <Godot/variant/utility_functions.hpp>

using namespace godot;
using namespace jenova::sdk;

JENOVA_SCRIPT_BEGIN

void OnAwake(Caller* instance) {}
void OnDestroy(Caller* instance) {}
void OnReady(Caller* instance) {}

// === EL NÚCLEO DE EVENTOS ===
// tipo: "COFRE", "PUERTA", "EVENTO" (manual, cuesta PA) o "TRAMPA" (automático, gratis)
void Interactuar(Caller* instance, int x, int y, String tipo, String res_path) {
	Node* self_node = GetSelf<Node>(instance);
	Node* dungeon = self_node->get_parent();
	Node* pm = self_node->get_node_or_null("/root/Main/PartyManager");
	Node* ui_manager = self_node->get_node_or_null("/root/Main/CanvasLayer");
	Node* audio = self_node->get_node_or_null("/root/AudioManager");
	if (!dungeon || !pm) return;

	Node* act_char = Object::cast_to<Node>(pm->call("GetActiveCharacter"));
	if (!act_char) {
		UtilityFunctions::print("[EventManager] Error: Personaje no encontrado.");
		return;
	}

	bool debug_mode = dungeon->has_meta("debug_mode") && (bool)dungeon->get_meta("debug_mode");
	int coste = (debug_mode || tipo == "TRAMPA") ? 0 : 2;

	int pa = act_char->has_meta("pa_actual") ? (int)act_char->get_meta("pa_actual") : 0;
	if (pa < coste) {
		UtilityFunctions::print("[EventManager] Necesitas ", coste, " PA para interactuar.");
		if (audio) audio->call("play_ui", "no");
		return;
	}

	bool interactuo = false;

	// === A. COFRES ===
	if (tipo == "COFRE") {
		int roll = UtilityFunctions::randi() % 3;
		if (roll == 0) {
			int oro = (UtilityFunctions::randi() % 20) + 10;
			act_char->set_meta("oro", (int)act_char->get_meta("oro", 0) + oro);
			UtilityFunctions::print("[EventManager] Cofre: +", oro, " de oro.");
		} else if (roll == 1) {
			int comida = (UtilityFunctions::randi() % 2) + 1;
			act_char->set_meta("comida_bolsa", (int)act_char->get_meta("comida_bolsa", 0) + comida);
			UtilityFunctions::print("[EventManager] Cofre: +", comida, " de comida.");
		} else {
			TypedArray<Resource> inv = act_char->get_meta("items_lista", TypedArray<Resource>());
			int max_s = act_char->get_meta("max_slots", 3);
			if (inv.size() < max_s) {
				Ref<Resource> item = ResourceLoader::get_singleton()->load("res://Data/Items/Espada.tres");
				if (item.is_valid()) {
					inv.push_back(item);
					act_char->set_meta("items_lista", inv);
					UtilityFunctions::print("[EventManager] Cofre: ¡una espada!");
				}
			} else {
				UtilityFunctions::print("[EventManager] Cofre: había algo, pero no te cabe en la mochila.");
			}
		}
		dungeon->call("AplicarEstadoCofre", x, y);
		interactuo = true;
	}
	// === B. PUERTAS ===
	else if (tipo == "PUERTA") {
		dungeon->call("AplicarEstadoPuerta", x, y);
		interactuo = true;
	}
	// === C. EVENTOS Y TRAMPAS (animación de texto + pantalla de evento) ===
	else if (tipo == "EVENTO" || tipo == "TRAMPA") {
		Ref<Resource> evento = ResourceLoader::get_singleton()->load(res_path);
		if (evento.is_valid() && ui_manager) {
			dungeon->set_meta("bloqueo_evento", true);
			dungeon->call("ApagarLucesMapa");

			Label* texto = memnew(Label);
			texto->set_text(tipo == "TRAMPA" ? String::utf8("\xC2\xA1TRAMPA!") : String("EVENTO"));
			texto->add_theme_font_size_override("font_size", 120);
			texto->add_theme_color_override("font_color", Color(1.0, 0.1, 0.1));
			texto->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			texto->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
			texto->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
			ui_manager->add_child(texto);

			Vector2 pantalla = ui_manager->get_viewport()->get_visible_rect().get_size();
			texto->set_size(Vector2(pantalla.x, 200));
			texto->set_position(Vector2(0, pantalla.y + 100));

			ui_manager->set_meta("heroe_en_peligro", act_char);

			Ref<Tween> tween = self_node->create_tween();
			tween->tween_property(texto, "position:y", pantalla.y / 2.0 - 100.0, 0.4)->set_trans(Tween::TRANS_EXPO)->set_ease(Tween::EASE_OUT);
			tween->tween_interval(0.6);
			tween->tween_property(texto, "position:y", -300.0, 0.3)->set_trans(Tween::TRANS_EXPO)->set_ease(Tween::EASE_IN);
			tween->parallel()->tween_callback(Callable(ui_manager, StringName("MostrarEventoVisual")).bind(evento));
			tween->tween_callback(Callable(texto, StringName("queue_free")));

			dungeon->call("AplicarEstadoEvento", x, y);
			interactuo = true;
		}
	}

	// === D. GASTO DE PA Y ACTUALIZACIÓN ===
	if (!interactuo) return;
	if (audio) audio->call("play_ui", "clic");

	act_char->set_meta("pa_actual", pa - coste);
	if (ui_manager) ui_manager->call("UpdateWarriorUI");

	// Durante un evento el turno se cierra al terminarlo (FinalizarEvento)
	bool en_evento = dungeon->has_meta("bloqueo_evento") && (bool)dungeon->get_meta("bloqueo_evento");
	if (!en_evento && pa - coste <= 0) pm->call("TerminarTurnoPersonaje");
	dungeon->call("RefrescarLuces");
}

JENOVA_SCRIPT_END
