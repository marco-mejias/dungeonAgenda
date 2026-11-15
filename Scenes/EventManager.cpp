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
#include "Scripts/CPP/Inventario/Inventario.hpp"

using namespace godot;
using namespace jenova::sdk;

JENOVA_SCRIPT_BEGIN

void OnAwake(Caller* instance) {}
void OnDestroy(Caller* instance) {}
void OnReady(Caller* instance) {}

// === THE EVENT CORE === tipo: "COFRE", "PUERTA", "EVENTO" (manual, costs PA) or "TRAMPA"
// (automatic, free)
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

	Node* feel = self_node->get_node_or_null("/root/Feel");
	bool debug_mode = dungeon->has_meta("debug_mode") && (bool)dungeon->get_meta("debug_mode");
	bool explorando = dungeon->has_meta("modo_exploracion") && (bool)dungeon->get_meta("modo_exploracion");
	int coste = (debug_mode || explorando || tipo == "TRAMPA" || tipo == "HOGUERA") ? 0 : 2;
	Node* zonas = self_node->get_node_or_null("/root/Zonas");

	int pa = act_char->has_meta("pa_actual") ? (int)act_char->get_meta("pa_actual") : 0;
	if (pa < coste) {
		UtilityFunctions::print("[EventManager] Necesitas ", coste, " PA para interactuar.");
		if (audio) audio->call("play_ui", "no");
		if (feel) feel->call("sacudir_ficha", String(act_char->get_name()));
		return;
	}

	bool interactuo = false;
	Vector3 pos_mundo(x + 0.5f, 0.45f, y + 0.5f);

	// === A. CHESTS ===
	if (tipo == "COFRE") {
		// Loot is rolled only once per chest ("cofre" table in the Catalogo). Whatever doesn't fit
		// in the backpack stays inside and the chest stays closed until someone takes it.
		Dictionary pendientes = dungeon->get_meta("botin_cofres", Dictionary());
		Vector2i celda(x, y);
		Array botin;
		if (pendientes.has(celda)) {
			botin = pendientes[celda];
		} else {
			Node* cat = inventario::Catalogo(self_node);
			int piso = pm->get_meta("piso_actual", 1);
			if (cat) botin = cat->call("tirar_botin", String("cofre"), piso);
		}
		// The boss key (if it is this chest) is always taken, whether the rest fits or not
		if (zonas) zonas->call("al_abrir_cofre", x, y, act_char);
		Array restos = inventario::EntregarBotin(act_char, botin, pos_mundo);
		if (restos.is_empty()) {
			pendientes.erase(celda);
			dungeon->call("AplicarEstadoCofre", x, y);
			interactuo = true;
		} else {
			pendientes[celda] = restos;
			UtilityFunctions::print("[EventManager] Cofre: no cabe todo en la mochila de ", act_char->get_name(), ".");
			if (audio) audio->call("play_ui", "no");
			if (feel) feel->call("sacudir_ficha", String(act_char->get_name()));
			interactuo = restos.size() < botin.size(); // If nothing was taken, no PA is spent
		}
		dungeon->set_meta("botin_cofres", pendientes);
	}
	// === B. DOORS ===
	else if (tipo == "PUERTA") {
		// The boss room door only opens with the key
		if (zonas && !(bool)zonas->call("puede_abrir", x, y, act_char)) {
			if (audio) audio->call("play_ui", "no");
			if (feel) feel->call("sacudir_ficha", String(act_char->get_name()));
			return;
		}
		dungeon->call("AplicarEstadoPuerta", x, y);
		if (feel) feel->call("sacudir", 0.12);
		interactuo = true;
	}
	// === B2. CAMPFIRE: rest (heals, recharges abilities and ends the day) ===
	else if (tipo == "HOGUERA") {
		if (!zonas || !(bool)zonas->call("descansar", act_char)) {
			if (audio) audio->call("play_ui", "no");
			return;
		}
		interactuo = true;
	}
	// === C. EVENTS AND TRAPS (text animation + event screen) ===
	else if (tipo == "EVENTO" || tipo == "TRAMPA") {
		Ref<Resource> evento = ResourceLoader::get_singleton()->load(res_path);
		if (evento.is_valid() && ui_manager) {
			dungeon->set_meta("bloqueo_evento", true);
			if (tipo == "TRAMPA" && feel) {
				feel->call("destello_pantalla", Color(1.0, 0.1, 0.08, 0.4), 0.45);
				feel->call("sacudir", 0.45);
			}
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

	// === D. PA SPENDING AND UPDATE ===
	if (!interactuo) return;
	if (audio) audio->call("play_ui", "clic");

	act_char->set_meta("pa_actual", pa - coste);
	if (ui_manager) ui_manager->call("UpdateWarriorUI");

	// During an event the turn closes when it ends (FinalizarEvento)
	bool en_evento = dungeon->has_meta("bloqueo_evento") && (bool)dungeon->get_meta("bloqueo_evento");
	// The hero switch waits for the door, the loot or the event to finish (DungeonManager)
	if (!en_evento && pa - coste <= 0) dungeon->call("PedirFinDeTurno");
	dungeon->call("RefrescarLuces");
}

JENOVA_SCRIPT_END
