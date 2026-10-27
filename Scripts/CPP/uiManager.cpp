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

	// Fichas de personaje: AUTO = las de quien sale en pantalla (+ el activo); OCULTAS = solo las fijadas
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
void MostrarNumeroDado(Caller* instance, int roll, bool success);
void AplicarResultadoEvento(Caller* instance, int option_index, bool success);

// ==========================================
// UTILITY FUNCTIONS
// ==========================================

Node* GetPartyManager() {
	if (!self) return nullptr;
	Node* main_node = self->get_parent();
	return main_node ? main_node->get_node_or_null("PartyManager") : nullptr;
}

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

// Qué fichas se ven en este momento (la lógica está en UI/Fichas.hpp)
void ActualizarVisibilidadFichas() {
	ActualizarVisibilidad(PanelFichas(), GetPartyManager(), GetDungeon(), GetPersonajesValidos(), fichas_auto, fichas_fijadas);
}

void ActualizarMarcadorTurno(Caller* instance) {
	if (!self) return;
	
	Node* pm = GetPartyManager();
	if (!pm) return;
	
	int floor = pm->has_meta("piso_actual")  ? (int)pm->get_meta("piso_actual")  : 1;
	int turn  = pm->has_meta("turno_actual") ? (int)pm->get_meta("turno_actual") : 1;
	
	Label* turn_marker = Object::cast_to<Label>(self->get_node_or_null("MarcadorTurnos"));
	if (turn_marker) {
		turn_marker->set_text("PISO " + String::num_int64(floor) + String::utf8("   -   D\xC3\x8D" "A ") + String::num_int64(turn));
	}
}

// Lo hace la transición a mitad del fundido (con la pantalla en negro)
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
	

	// LLAMADAS DIRECTAS EN C++
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

// Botón "Descansar": fundido a negro con el rótulo del piso y, a mitad, el cambio real
void DescansarYContinuar(Caller* instance) {
	if (!self) return;
	Node* transicion = self->get_parent()->get_node_or_null("CapaTransicion");
	Node* pm = GetPartyManager();
	int siguiente = (pm ? (int)pm->get_meta("piso_actual", 1) : 1) + 1;
	if (transicion) {
		transicion->call("fundido", "PISO " + String::num_int64(siguiente), Callable(self, StringName("EjecutarCambioDePiso")));
	} else {
		EjecutarCambioDePiso(instance);
	}
}

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

void UpdateWarriorUI(Caller* instance) {
	ActualizarStatsUI(instance);
	ActualizarVisibilidadFichas();
}

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

	// Fichas de enemigo (se reutilizan según quién esté en pantalla)
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

void AlternarModoFichas(Caller* instance) {
	fichas_auto = !fichas_auto;
	fichas_fijadas.clear();
	Button* btn = Object::cast_to<Button>(self->get_node_or_null("BtnFichas"));
	if (btn) btn->set_text(fichas_auto ? "FICHAS: AUTO" : "FICHAS: OCULTAS");
	Node* audio = self->get_node_or_null(NodePath("/root/AudioManager"));
	if (audio) audio->call("play_ui", "clic");
	ActualizarVisibilidadFichas();
}

// Al hacer clic en un héroe del mapa su ficha se queda visible (también en modo OCULTAS)
void FijarFicha(Caller* instance, String nombre) {
	if (!EstaFijada(fichas_fijadas, nombre)) fichas_fijadas.push_back(nombre);
	ActualizarVisibilidadFichas();
}

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
	
	// 1. Cambiamos el personaje activo en la lógica del juego
	pm->call("CambiarPersonajeActivo", nombre);
}

void MostrarStats(Caller* instance, String nombre) {
	Control* panel = PanelFichas();
	Control* detalle = panel ? Object::cast_to<Control>(panel->get_node_or_null(NodePath(nombre + "/V/Detalle"))) : nullptr;
	if (!detalle) return;
	detalle->set_visible(true);
	Node* audio = self->get_node_or_null(NodePath("/root/AudioManager"));
	if (audio) audio->call("play_ui", "open");
}

void OcultarStats(Caller* instance, String nombre) {
	Control* panel = PanelFichas();
	Control* detalle = panel ? Object::cast_to<Control>(panel->get_node_or_null(NodePath(nombre + "/V/Detalle"))) : nullptr;
	if (detalle) detalle->set_visible(false);
}

// ==========================================
// EVENT SCREEN FUNCTIONS
// ==========================================

void CerrarEvento(Caller* instance) {
	if (!self || !m_event_screen) return;
	
	float screen_height = self->get_viewport()->get_visible_rect().size.y;
	
	Ref<Tween> tween = m_event_screen->create_tween();
	tween->tween_property(m_event_screen, "position", Vector2(628, screen_height), 0.4)->set_trans(Tween::TRANS_CUBIC)->set_ease(Tween::EASE_IN);
	tween->connect("finished", Callable(m_event_screen, StringName("hide")));
	
	// Fusión del MODO DIOS con el truco Anti-Errores de C++
	Node* root_node = self->get_node_or_null(NodePath("/root"));
	if (root_node) {
		Node* dungeon = root_node->find_child("DungeonManager", true, false);
		if (dungeon) {
			dungeon->call("BajarMapa");
			dungeon->call("FinalizarEvento");
		} else {
			godot::UtilityFunctions::print("[ERROR] No se encontró el DungeonManager al cerrar.");
		}
	}
}

// ¡AQUÍ ESTÁ EL Caller* instance QUE FALTABA!
void AplicarResultadoEvento(Caller* instance, int option_index, bool success) {
	if (!self) return;

	Ref<Resource> event_res = self->get_meta("evento_actual");
	Node* hero = Object::cast_to<Node>(self->get_meta("heroe_en_peligro"));
	if (event_res.is_null() || !hero) return;

	Array options = event_res->get("opciones");
	Ref<Resource> option = options[option_index];

	if (m_event_screen) {
		RichTextLabel* story_text = Object::cast_to<RichTextLabel>(m_event_screen->get_node_or_null("TextoHistoria"));
		if (story_text) story_text->set_text(success ? option->get("texto_exito") : option->get("texto_fracaso"));

		Node* btn_box = m_event_screen->get_node_or_null("CajaBotones");
		if (btn_box) {
			TypedArray<Node> children = btn_box->get_children();
			for (int i = 0; i < children.size(); i++) {
				Control* child = Object::cast_to<Control>(children[i]); 
				if (child) child->set_visible(child->get_name() == StringName("BtnContinuar"));
			}
		}
	}

	String fn_call = success ? option->get("funcion_exito")   : option->get("funcion_fracaso");
	int param      = success ? (int)option->get("parametro_exito") : (int)option->get("parametro_fracaso");
	
	if (!success && hero) {
		int sanity = hero->has_meta("sanidad_actual") ? (int)hero->get_meta("sanidad_actual") : 100;
		hero->set_meta("sanidad_actual", UtilityFunctions::max(0, sanity - 10)); 
		ActualizarStatsUI(instance); // LLAMADA C++ DIRECTA
	}

	if (!fn_call.is_empty()) {
		Node* pm = GetPartyManager();
		if (pm && pm->has_method(fn_call)) pm->call(fn_call, param, hero);
		else UtilityFunctions::printerr("[UI] El evento pide una función que no existe en PartyManager: '", fn_call, "'");
	}
}

// Cierra la pantalla de evento al instante (al cambiar de piso o desde debug)
void CerrarEventoPorDebug(Caller* instance) {
	if (!self) return;
	anim_dados = false;
	wait_resultado = false;
	if (m_event_screen) m_event_screen->set_visible(false);
	Node* dungeon = self->get_parent()->get_node_or_null("ContenedorMapa/SubViewport/DungeonManager");
	if (dungeon) dungeon->call("BajarMapa");
}

// ¡AQUÍ ESTÁ EL Caller* instance QUE FALTABA!
void MostrarNumeroDado(Caller* instance, int roll, bool success) { 
	if (!self || !m_event_screen) return;
	
	// Justo antes de cambiar el texto de success
	Node* audio = self->get_node_or_null(NodePath("/root/AudioManager"));
	if (audio) audio->call("play_sfx", success ? "success" : "fail");
		
	RichTextLabel* story_text = Object::cast_to<RichTextLabel>(m_event_screen->get_node_or_null("TextoHistoria"));
	if (story_text) {
		
		String color = success ? "[color=green]" : "[color=red]";
		String result_txt = String("[center]Resultado:\n\n") + color + "[b][font_size=50]" + String::num_int64(roll) + "[/font_size][/b][/color][/center]";

		story_text->set_text(result_txt);
	}
}

void ResolverOpcion(Caller* instance, int option_index) {
	if (!self) return;

	Ref<Resource> event_res = self->get_meta("evento_actual");
	Node* hero = Object::cast_to<Node>(self->get_meta("heroe_en_peligro"));
	if (event_res.is_null() || !hero) return;

	Array options = event_res->get("opciones");
	Ref<Resource> option = options[option_index];

	String stat_req = option->get("stat_usada");
	float mult = option->get("multiplicador_dificultad");
	int hero_stat = hero->has_meta(stat_req) ? (int)hero->get_meta(stat_req) : 0;
	
	int sanity = hero->has_meta("sanidad_actual") ? (int)hero->get_meta("sanidad_actual") : 100;
	if (sanity < 20) {
		hero_stat = hero_stat / 2; 
	}
	
	int prob_success = (stat_req == "ninguna") ? 100 : (hero_stat * mult * 10);
	if (prob_success > 100) prob_success = 100;

	int roll = UtilityFunctions::randi_range(1, 100);
	bool success = (roll <= prob_success);
	
	if (m_event_screen) {
		Node* btn_box = m_event_screen->get_node_or_null("CajaBotones");
		if (btn_box) {
			TypedArray<Node> children = btn_box->get_children();
			for (int i = 0; i < children.size(); i++) {
				Control* child = Object::cast_to<Control>(children[i]); 
				if (child) child->set_visible(false);
			}
		}

		save_prob = prob_success;
		save_tirada = roll;
		save_exito = success;
		save_index = option_index;
		
		anim_dados = true;
		tiempo_dados = 1.5f;
		tick_view = 0.0f;

		// Dado 3D low poly en el hueco de la ilustración
		Node* dado = m_event_screen->get_node_or_null("Dado");
		Control* foto = Object::cast_to<Control>(m_event_screen->get_node_or_null("Foto"));
		if (dado) {
			if (foto) foto->set_visible(false);
			dado->call("tirar", roll, success, 1.5);
		}
	}
}

// OJO: Jenova revienta si una función llamada con call()/Callable recibe un Variant por valor
void MostrarEventoVisual(Caller* instance, Resource* event_data) {
	if (!self) return;

	// === SALVAVIDAS ANTI-PANTALLAS INVISIBLES ===
	Node* main_node = self->get_parent();
	if (main_node) {
		// Forzamos que la capa 2 esté visible (por si la ocultaste en el editor)
		CanvasLayer* capa_eventos = Object::cast_to<CanvasLayer>(main_node->get_node_or_null("CanvasLayer2"));
		if (capa_eventos) capa_eventos->set_visible(true);
		
		// Obligamos a buscar la pantalla ahora, no en el Awake
		m_event_screen = Object::cast_to<Control>(main_node->get_node_or_null("CanvasLayer2/PantallaEvento"));
	}

	if (!m_event_screen) {
		godot::UtilityFunctions::print("[ERROR GRAVE] ¡El código no encuentra CanvasLayer2/PantallaEvento! Revisa los nombres en el árbol.");
		return;
	}
	// ============================================

	Ref<Resource> res = event_data;
	if (res.is_null()) return;

	self->set_meta("evento_actual", res);
	
	String title = res->get("titulo");
	String desc  = res->get("descripcion");
	Array options = res->get("opciones");
	Ref<Texture2D> img = res->get("ilustracion"); 

	// ... [EL RESTO DE TU FUNCIÓN SIGUE IGUAL DESDE AQUÍ] ...

	Label* l_title           = Object::cast_to<Label>(m_event_screen->get_node_or_null("Titulo"));
	RichTextLabel* l_story   = Object::cast_to<RichTextLabel>(m_event_screen->get_node_or_null("TextoHistoria"));
	TextureRect* r_photo     = Object::cast_to<TextureRect>(m_event_screen->get_node_or_null("Foto"));

	if (l_title) l_title->set_text(title);
	Node* dado = m_event_screen->get_node_or_null("Dado");
	if (dado) dado->call("ocultar");
	Control* foto_visible = Object::cast_to<Control>(m_event_screen->get_node_or_null("Foto"));
	if (foto_visible) foto_visible->set_visible(true);
	if (l_story) l_story->set_text(desc);
	if (r_photo && img.is_valid()) r_photo->set_texture(img);

	Control* btn_box = Object::cast_to<Control>(m_event_screen->get_node_or_null("CajaBotones"));
	Button* btn_cont = Object::cast_to<Button>(m_event_screen->get_node_or_null("CajaBotones/BtnContinuar"));

	if (btn_box) {
		TypedArray<Node> children = btn_box->get_children();
		for (int i = 0; i < children.size(); i++) {
			Node* h = Object::cast_to<Node>(children[i]);
			if (h != btn_cont) h->queue_free();
		}

		if (options.size() > 0) {
			if (btn_cont) btn_cont->set_visible(false); 
			
			for (int i = 0; i < options.size(); i++) {
				Button* b = memnew(Button);
				
				String btn_text = "Error";
				Variant opt_var = options[i];

				if (opt_var.get_type() == Variant::OBJECT) {
					Ref<Resource> sub_res = opt_var;
					if (sub_res.is_valid()) {
						btn_text = sub_res->get("texto_boton"); 
					}
				}

				b->set_text(btn_text);
				b->set_custom_minimum_size(Vector2(0, 50));
				btn_box->add_child(b);

				Callable click_fn = Callable(self, StringName("ResolverOpcion")).bind(i);
				b->connect("pressed", click_fn);
			}
		} else {
			if (btn_cont) btn_cont->set_visible(true); 
		}
	}

	float screen_height = self->get_viewport()->get_visible_rect().size.y;
	
	m_event_screen->set_modulate(Color(1, 1, 1, 1));
	m_event_screen->set_scale(Vector2(1, 1));
	m_event_screen->set_position(Vector2(628, screen_height)); 
	m_event_screen->set_visible(true);

	// Fusión del MODO DIOS con el truco Anti-Errores de C++
	Node* root_node = self->get_node_or_null(NodePath("/root"));
	if (root_node) {
		Node* dungeon = root_node->find_child("DungeonManager", true, false);
		if (dungeon) {
			dungeon->call("SubirMapa");
		} else {
			godot::UtilityFunctions::print("[ERROR] No se encontró el DungeonManager al abrir.");
		}
	}
	
	Ref<Tween> tween = m_event_screen->create_tween();
	tween->tween_property(m_event_screen, "position", Vector2(628, 0), 0.5)
		 ->set_trans(Tween::TRANS_CUBIC)
		 ->set_ease(Tween::EASE_OUT);
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
	InicializarTodaLaUI(instance); // LLAMADA C++ DIRECTA
	ActualizarMarcadorTurno(instance); // LLAMADA C++ DIRECTA
	
	if (m_event_screen) {
		Button* btn_cont = Object::cast_to<Button>(m_event_screen->get_node_or_null("CajaBotones/BtnContinuar"));
		if (btn_cont) {
			Callable close_fn = Callable(self, StringName("CerrarEvento"));
			if (!btn_cont->is_connected("pressed", close_fn)) {
				btn_cont->connect("pressed", close_fn);
			}
		}
	}
	
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

	// TAB alterna el modo de las fichas
	bool tab = Input::get_singleton()->is_physical_key_pressed(KEY_TAB);
	if (tab && !tab_prev) AlternarModoFichas(instance);
	tab_prev = tab;

	t_fichas -= (float)delta;
	if (t_fichas <= 0.0f) {
		t_fichas = 0.1f;
		ActualizarVisibilidadFichas();
	}

	if (anim_dados) {
		tiempo_dados -= delta;
		tick_view -= delta;

		if (tick_view <= 0.0f) {
			tick_view = 0.05f; 
			
			if (m_event_screen) {
				RichTextLabel* story_text = Object::cast_to<RichTextLabel>(m_event_screen->get_node_or_null("TextoHistoria"));
				if (story_text) {
					int fake_num = UtilityFunctions::randi_range(1, 100);
					String txt = String::utf8("[center]Probabilidad de \xC3\xA9xito: [b]") + String::num_int64(save_prob) + "%[/b]\n\n";
					txt += "[shake rate=20.0 level=5 connected=1]Tirando el dado...[/shake]\n\n";
					txt += "[font_size=60][color=gray]" + String::num_int64(fake_num) + "[/color][/font_size][/center]";
					story_text->set_text(txt);
				}
			}
		}

		if (tiempo_dados <= 0.0f) {
			anim_dados = false;
			MostrarNumeroDado(instance, save_tirada, save_exito);
			
			wait_resultado = true;
			tiempo_espera = 1.5f;
		}
	}

	if (wait_resultado) {
		tiempo_espera -= delta;
		
		if (tiempo_espera <= 0.0f) {
			wait_resultado = false;
			AplicarResultadoEvento(instance, save_index, save_exito);
		}
	}
}

void OnDestroy(Caller* instance) {
	self = nullptr;
	m_event_screen = nullptr;
}

JENOVA_SCRIPT_END
