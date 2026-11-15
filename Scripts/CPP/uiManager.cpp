/* Jenova C++ Node Base Script (Meteora) - HUD Manager */

#include <Godot/godot.hpp>
#include <Godot/classes/node.hpp>
#include <Godot/classes/canvas_layer.hpp>
#include <Godot/variant/variant.hpp>
#include <Godot/classes/label.hpp>
#include <Godot/classes/progress_bar.hpp>
#include <Godot/classes/color_rect.hpp>         
#include <Godot/classes/tween.hpp>      
#include <Godot/classes/interval_tweener.hpp>
#include <Godot/classes/callback_tweener.hpp>        
#include <Godot/classes/property_tweener.hpp>  
#include <Godot/classes/method_tweener.hpp>    
#include <Godot/classes/texture_rect.hpp>
#include <Godot/classes/resource_loader.hpp>
#include <Godot/classes/texture2d.hpp>
#include <Godot/classes/rich_text_label.hpp>
#include <Godot/classes/button.hpp>
#include <Godot/classes/viewport.hpp>
#include <Godot/classes/control.hpp>
#include <Godot/classes/node2d.hpp>
#include <Godot/classes/input.hpp>
#include <Godot/classes/camera3d.hpp>
#include <Godot/classes/node3d.hpp>
#include <Godot/classes/atlas_texture.hpp>
#include <Godot/classes/style_box_flat.hpp>
#include <Godot/classes/panel_container.hpp>
#include <Godot/classes/h_box_container.hpp>
#include <Godot/classes/packed_scene.hpp>
#include <vector>
#include "Scripts/CPP/UI/Fichas.hpp"
#include "Scripts/CPP/Inventario/Inventario.hpp"

using namespace godot;
using namespace jenova::sdk;
using namespace ui;

namespace {
	CanvasLayer* self = nullptr;

	// Dice Animation States
	bool anim_dados = false;
	float tiempo_dados = 0.0f;
	float tick_view = 0.0f;
	
	// Event Result States
	bool wait_resultado = false;
	float tiempo_espera = 0.0f;
	
	// Event Memory
	int save_prob = 0;
	int save_tirada = 0;
	bool save_exito = false;
	int save_index = 0;

	Control* m_event_screen = nullptr;

	// Character cards: AUTO = the ones of whoever is on screen (+ the active hero); OCULTAS = only
	// the pinned ones
	bool fichas_auto = true;
	bool tab_prev = false;
	float t_fichas = 0.0f;
	std::vector<String> fichas_fijadas;

}

JENOVA_SCRIPT_BEGIN

void ActualizarStatsUI(Caller* instance);
void ActualizarMarcadorTurno(Caller* instance);
void UpdateWarriorUI(Caller* instance);
void InicializarTodaLaUI(Caller* instance);
void AplicarResultadoEvento(Caller* instance, int option_index, bool success);

// ==========================================
// UTILITY FUNCTIONS
// ==========================================

Node* GetPartyManager() {
	if (!self) return nullptr;
	Node* main_node = self->get_parent();
	return main_node ? main_node->get_node_or_null("PartyManager") : nullptr;
}

// Heroes that get a card (alive and not escaped)
TypedArray<Node> GetPersonajesValidos() {
	TypedArray<Node> valid_characters; 
	Node* pm = GetPartyManager();
	if (!pm) return valid_characters;

	TypedArray<Node> children = pm->get_children();
	for (int i = 0; i < children.size(); i++) {
		Node* p = Object::cast_to<Node>(children[i]); 
		if (p && p->get_name() != StringName("DatosEquipo")) {
			valid_characters.push_back(p);
		}
	}
	return valid_characters;
}

// ==========================================
// MAIN UI FUNCTIONS
// ==========================================

Control* PanelFichas() {
	return self ? Object::cast_to<Control>(self->get_node_or_null("PanelFichas")) : nullptr;
}

Node* GetDungeon() {
	return self ? self->get_parent()->get_node_or_null("ContenedorMapa/SubViewport/DungeonManager") : nullptr;
}

// Refreshes every card with the current stats
void ActualizarStatsUI(Caller* instance) {
	Control* panel = PanelFichas();
	Node* pm = GetPartyManager();
	if (!panel || !pm) return;
	Node* activo = Object::cast_to<Node>(pm->call("GetActiveCharacter"));
	TypedArray<Node> characters = GetPersonajesValidos();
	for (int i = 0; i < characters.size(); i++) {
		Node* p = Object::cast_to<Node>(characters[i]);
		Control* f = Object::cast_to<Control>(panel->get_node_or_null(NodePath(String(p->get_name()))));
		if (f) RellenarFichaHeroe(f, p, activo);
	}
}

// Which cards are visible right now (the logic lives in UI/Fichas.hpp)
void ActualizarVisibilidadFichas() {
	ActualizarVisibilidad(PanelFichas(), GetPartyManager(), GetDungeon(), GetPersonajesValidos(), fichas_auto, fichas_fijadas);
}

// Floor and day counter
void ActualizarMarcadorTurno(Caller* instance) {
	if (!self) return;
	
	Node* pm = GetPartyManager();
	if (!pm) return;
	
	int floor = pm->has_meta("piso_actual")  ? (int)pm->get_meta("piso_actual")  : 1;
	int turn  = pm->has_meta("turno_actual") ? (int)pm->get_meta("turno_actual") : 1;
	
	Label* turn_marker = Object::cast_to<Label>(self->get_node_or_null("MarcadorTurnos"));
	if (turn_marker) {
		turn_marker->set_text("Piso " + String::num_int64(floor) + String::utf8("   \xC2\xB7   D\xC3\xAD" "a ") + String::num_int64(turn));
	}
}

// The transition does it halfway through the fade (with the screen black)
void EjecutarCambioDePiso(Caller* instance) {
	if (!self) return;

	UtilityFunctions::print("[UI] The party has rested. Preparing next floor...");

	CanvasLayer* campfire_layer = Object::cast_to<CanvasLayer>(self->get_parent()->get_node_or_null("CanvasLayer3"));
	if (campfire_layer) campfire_layer->set_visible(false);

	Node* pm = GetPartyManager();
	if (pm) {
		int floor = pm->has_meta("piso_actual") ? (int)pm->get_meta("piso_actual") : 1;
		pm->set_meta("piso_actual", floor + 1);
		pm->set_meta("turno_actual", 1);
		pm->call("FinDePiso"); // The "until the end of the floor" effects end

		TypedArray<Node> children = pm->get_children();
		for (int i = 0; i < children.size(); i++) {
			Node* child = Object::cast_to<Node>(children[i]);
			if (child && child->get_name() != StringName("DatosEquipo")) {
				child->set_meta("ha_movido", false);
				child->set_meta("ha_escapado", false);
				child->set_meta("in_combat", false);

				if (!(bool)child->get_meta("esta_muerto")) {
					int current_hp = child->has_meta("hp_actual") ? (int)child->get_meta("hp_actual") : 0;
					int max_hp     = child->has_meta("hp_max")    ? (int)child->get_meta("hp_max")    : 100;
					child->set_meta("hp_actual", UtilityFunctions::min(max_hp, current_hp + 10));
					child->set_meta("pa_actual", child->get_meta("pa_max", 3));

					int current_hunger = child->has_meta("hambre_actual")  ? (int)child->get_meta("hambre_actual")  : 10;
					int current_sanity = child->has_meta("sanidad_actual") ? (int)child->get_meta("sanidad_actual") : 100;
					
					child->set_meta("hambre_actual", UtilityFunctions::min(10, current_hunger + 3));
					child->set_meta("sanidad_actual", UtilityFunctions::min(100, current_sanity + 50));
				}
			}
		}
	}
	

	// DIRECT C++ CALLS
	ActualizarMarcadorTurno(instance);
	ActualizarStatsUI(instance);
	UpdateWarriorUI(instance);

	Node* dungeon = self->get_parent()->get_node_or_null("ContenedorMapa/SubViewport/DungeonManager");
	if (dungeon) {
		dungeon->call("AvanzarAlSiguientePiso");
	}

	Node* audio = self->get_node_or_null(NodePath("/root/AudioManager"));
	if (audio) audio->call("play_music", "dungeon");
}

// "Rest" button: fade to black with the floor title and, halfway, the real change
void DescansarYContinuar(Caller* instance) {
	if (!self) return;
	Node* transicion = self->get_parent()->get_node_or_null("CapaTransicion");
	Node* pm = GetPartyManager();
	int siguiente = (pm ? (int)pm->get_meta("piso_actual", 1) : 1) + 1;
	if (transicion) {
		transicion->call("fundido", "Piso " + String::num_int64(siguiente), Callable(self, StringName("EjecutarCambioDePiso")));
	} else {
		EjecutarCambioDePiso(instance);
	}
}

// Debug: takes the whole party out of the floor at once
void DebugForzarSalida(Caller* instance) {
	if (!self) return;
	
	Node* pm = GetPartyManager();
	Node* dungeon = self->get_parent()->get_node_or_null("ContenedorMapa/SubViewport/DungeonManager");

	if (!pm || !dungeon) return;

	if (dungeon->has_meta("bloqueo_evento") && (bool)dungeon->get_meta("bloqueo_evento")) {
		UtilityFunctions::print("[DEBUG] Cannot force exit! You must resolve the active event first.");
		return; 
	}

	UtilityFunctions::print("[DEBUG] Activating trap: Everyone to the exit.");

	if (!dungeon->has_meta("casilla_salida")) {
		UtilityFunctions::print("[DEBUG] Cannot force exit: No exit generated.");
		return;
	}

	TypedArray<Node> characters = GetPersonajesValidos();

	for (int i = 0; i < characters.size(); i++) {
		Node* p = Object::cast_to<Node>(characters[i]);
		bool has_escaped = p->has_meta("ha_escapado") ? (bool)p->get_meta("ha_escapado") : false;

		if (!has_escaped) {
			String node_name = p->get_name(); 
			Node2D* visual_token = Object::cast_to<Node2D>(dungeon->get_node_or_null(NodePath(node_name)));
			if (visual_token) {
				visual_token->set_visible(false);
			}

			pm->call("PersonajeEscapa", p);
		}
	}

	UpdateWarriorUI(instance);
}

// Main refresh: stats and which cards are visible
void UpdateWarriorUI(Caller* instance) {
	ActualizarStatsUI(instance);
	ActualizarVisibilidadFichas();
}

// Creates one card per hero and the enemy cards when the game starts
void InicializarTodaLaUI(Caller* instance) {
	Control* panel = PanelFichas();
	if (!panel) return;
	Ref<PackedScene> escena = ResourceLoader::get_singleton()->load("res://Scenes/ficha_compacta.tscn");
	if (escena.is_null()) return;

	auto conectar = [&](Control* ficha, const String& nombre, bool seleccionable) {
		Button* btn = Object::cast_to<Button>(ficha->get_node_or_null("BtnFicha"));
		if (!btn) return;
		if (seleccionable) btn->connect("pressed", Callable(self, StringName("SeleccionarPersonajeDesdeUI")).bind(nombre));
		btn->connect("mouse_entered", Callable(self, StringName("MostrarStats")).bind(nombre));
		btn->connect("mouse_exited", Callable(self, StringName("OcultarStats")).bind(nombre));
	};

	TypedArray<Node> characters = GetPersonajesValidos();
	for (int i = 0; i < characters.size(); i++) {
		Node* p = Object::cast_to<Node>(characters[i]);
		String name = p->get_name();
		if (panel->get_node_or_null(NodePath(name))) continue;

		Control* ficha = Object::cast_to<Control>(escena->instantiate());
		ficha->set_name(name);
		panel->add_child(ficha);

		TextureRect* portrait = Object::cast_to<TextureRect>(ficha->get_node_or_null("V/Cabecera/Retrato"));
		String path = p->get_meta("retrato_path", "");
		if (portrait && !path.is_empty()) {
			Ref<Texture2D> tex = ResourceLoader::get_singleton()->load(path);
			if (tex.is_valid()) portrait->set_texture(tex);
		}
		conectar(ficha, name, true);
	}

	// Enemy cards (reused depending on who is on screen)
	for (int k = 0; k < MAX_FICHAS_ENEMIGO; k++) {
		String name = "Enemigo" + String::num_int64(k);
		if (panel->get_node_or_null(NodePath(name))) continue;
		Control* ficha = Object::cast_to<Control>(escena->instantiate());
		ficha->set_name(name);
		const char* ocultar[] = { "V/Cabecera/Info/Hambre", "V/Cabecera/Info/Cordura", "V/Cabecera/Info/Fila/PA", "V/Detalle/Recursos", "V/Detalle/Slots" };
		for (const char* r : ocultar) {
			Control* c = Object::cast_to<Control>(ficha->get_node_or_null(r));
			if (c) c->set_visible(false);
		}
		ficha->set_visible(false);
		panel->add_child(ficha);
		conectar(ficha, name, false);
	}

	ActualizarStatsUI(instance);
	ActualizarVisibilidadFichas();
}

// AUTO <-> OCULTAS card mode (TAB)
void AlternarModoFichas(Caller* instance) {
	fichas_auto = !fichas_auto;
	fichas_fijadas.clear();
	Button* btn = Object::cast_to<Button>(self->get_node_or_null("BtnFichas"));
	if (btn) btn->set_text(fichas_auto ? "FICHAS: AUTO" : "FICHAS: OCULTAS");
	Node* audio = self->get_node_or_null(NodePath("/root/AudioManager"));
	if (audio) audio->call("play_ui", "clic");
	ActualizarVisibilidadFichas();
}

// Clicking a hero on the map keeps their card visible (also in OCULTAS mode)
void FijarFicha(Caller* instance, String nombre) {
	if (!EstaFijada(fichas_fijadas, nombre)) fichas_fijadas.push_back(nombre);
	ActualizarVisibilidadFichas();
}

// Clicking a card selects that hero (unless something is moving)
void SeleccionarPersonajeDesdeUI(Caller* instance, String nombre) {
	if (!self || nombre.begins_with("Enemigo")) return;

	Node* dungeon = self->get_parent()->get_node_or_null("ContenedorMapa/SubViewport/DungeonManager");
	if (dungeon && dungeon->has_meta("is_moving") && (bool)dungeon->get_meta("is_moving")) {
		Node* audio = self->get_node_or_null(NodePath("/root/AudioManager"));
		if (audio) audio->call("play_ui", "no");
		return;
	}

	Node* pm = GetPartyManager();
	if (!pm) return;
	
	Node* audio = self->get_node_or_null(NodePath("/root/AudioManager"));
	if (audio) audio->call("play_ui", "clic");
	
	// 1. We change the active character in the game logic
	pm->call("CambiarPersonajeActivo", nombre);
}

// Shows the detailed stats of a card (hover)
void MostrarStats(Caller* instance, String nombre) {
	Control* panel = PanelFichas();
	Control* detalle = panel ? Object::cast_to<Control>(panel->get_node_or_null(NodePath(nombre + "/V/Detalle"))) : nullptr;
	if (!detalle) return;
	detalle->set_visible(true);
	Node* audio = self->get_node_or_null(NodePath("/root/AudioManager"));
	if (audio) audio->call("play_ui", "open");
}

// Hides them again
void OcultarStats(Caller* instance, String nombre) {
	Control* panel = PanelFichas();
	Control* detalle = panel ? Object::cast_to<Control>(panel->get_node_or_null(NodePath(nombre + "/V/Detalle"))) : nullptr;
	if (detalle) detalle->set_visible(false);
}

// ==========================================
// EVENT SCREEN FUNCTIONS
// ==========================================

// The event screen is handled by Scripts/GDD/pantalla_evento.gd (d20 roll, win and lose images,
// effects). Only the usual entry points are left here, and they delegate to it.
Control* PantallaEvento() {
	if (!self) return nullptr;
	Node* main_node = self->get_parent();
	if (main_node) {
		CanvasLayer* capa = Object::cast_to<CanvasLayer>(main_node->get_node_or_null("CanvasLayer2"));
		if (capa) capa->set_visible(true);
		m_event_screen = Object::cast_to<Control>(main_node->get_node_or_null("CanvasLayer2/PantallaEvento"));
	}
	return m_event_screen;
}

// Closes the event screen
void CerrarEvento(Caller* instance) {
	Control* p = PantallaEvento();
	if (p) p->call("cerrar");
}

// Kept for compatibility: the screen does it now when the roll ends
void AplicarResultadoEvento(Caller* instance, int option_index, bool success) {}

// Closes the event screen instantly (when changing floors or from debug)
void CerrarEventoPorDebug(Caller* instance) {
	Control* p = PantallaEvento();
	if (p) p->call("cerrar_ya");
}

// The player picked an option on the event screen
void ResolverOpcion(Caller* instance, int option_index) {
	Control* p = PantallaEvento();
	if (p) p->call("resolver", option_index);
}

// CAREFUL: Jenova crashes if a function called with call()/Callable takes a Variant by value
void MostrarEventoVisual(Caller* instance, Resource* event_data) {
	Control* p = PantallaEvento();
	if (!p) {
		UtilityFunctions::print("[ERROR GRAVE] No encuentro CanvasLayer2/PantallaEvento.");
		return;
	}
	Ref<Resource> res = event_data;
	if (res.is_null()) return;
	self->set_meta("evento_actual", res);
	Node* heroe = self->has_meta("heroe_en_peligro") ? Object::cast_to<Node>(self->get_meta("heroe_en_peligro")) : nullptr;
	p->call("mostrar", res, heroe);
}


// ==========================================
// STARTUP FUNCTIONS
// ==========================================

void OnAwake(Caller* instance) {
	self = GetSelf<CanvasLayer>(instance);
	if (self && self->get_parent()) {
		m_event_screen = Object::cast_to<Control>(self->get_parent()->get_node_or_null("CanvasLayer2/PantallaEvento"));
	}
}

void OnReady(Caller* instance) {
	InicializarTodaLaUI(instance); // DIRECT C++ CALL
	ActualizarMarcadorTurno(instance); // DIRECT C++ CALL
	
	Button* btn_fichas = Object::cast_to<Button>(self->get_node_or_null("BtnFichas"));
	if (btn_fichas) {
		Callable fn = Callable(self, StringName("AlternarModoFichas"));
		if (!btn_fichas->is_connected("pressed", fn)) btn_fichas->connect("pressed", fn);
	}

	Button* btn_debug = Object::cast_to<Button>(self->get_node_or_null("BtnSalidaDebug"));
	if (btn_debug) {
		Callable debug_fn = Callable(self, StringName("DebugForzarSalida"));
		if (!btn_debug->is_connected("pressed", debug_fn)) {
			btn_debug->connect("pressed", debug_fn);
		}
	}
	
	Node* main_node = self->get_parent();
	if (main_node) {
		Button* btn_rest = Object::cast_to<Button>(main_node->get_node_or_null("CanvasLayer3/PantallaFogata/BtnDescansar"));
		if (btn_rest) {
			Callable rest_fn = Callable(self, StringName("DescansarYContinuar"));
			if (!btn_rest->is_connected("pressed", rest_fn)) {
				btn_rest->connect("pressed", rest_fn);
			}
		} 
	}
}

void OnProcess(Caller* instance, double delta) {
	if (!self) return;

	// TAB toggles the card mode
	bool tab = Input::get_singleton()->is_physical_key_pressed(KEY_TAB);
	if (tab && !tab_prev) AlternarModoFichas(instance);
	tab_prev = tab;

	t_fichas -= (float)delta;
	if (t_fichas <= 0.0f) {
		t_fichas = 0.1f;
		ActualizarVisibilidadFichas();
	}

}

void OnDestroy(Caller* instance) {
	self = nullptr;
	m_event_screen = nullptr;
}

JENOVA_SCRIPT_END
