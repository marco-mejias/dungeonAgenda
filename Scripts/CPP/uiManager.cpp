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

using namespace godot;
using namespace jenova::sdk;

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
	const int MAX_FICHAS_ENEMIGO = 4;
	Ref<Texture2D> m_hoja_personajes;

	const Color BORDE_NORMAL = Color(0.55, 0.6, 0.85);
	const Color BORDE_ACTIVO = Color(1.0, 0.8, 0.25);
	const Color BORDE_ENEMIGO = Color(0.6, 0.18, 0.16);
	const Color BORDE_ENEMIGO_ALERTA = Color(1.0, 0.3, 0.2);
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

String GenerarTextoStats(Node* character) {
	if (!character) return "";
	
	int spd = character->has_meta("velocidad") ? (int)character->get_meta("velocidad") : 0;
	int str = character->has_meta("fuerza")    ? (int)character->get_meta("fuerza")    : 0;
	int def = character->has_meta("defensa")   ? (int)character->get_meta("defensa")   : 0;
	int mag = character->has_meta("magia")     ? (int)character->get_meta("magia")     : 0;
	int lck = character->has_meta("suerte")    ? (int)character->get_meta("suerte")    : 0;

	return "Speed: "   + String::num_int64(spd) + "\n" +
		   "Strength: " + String::num_int64(str) + "\n" +
		   "Defense: "  + String::num_int64(def) + "\n" +
		   "Magic: "    + String::num_int64(mag) + "\n" +
		   "Luck: "     + String::num_int64(lck);
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

bool FichaFijada(const String& nombre) {
	for (const String& n : fichas_fijadas) if (n == nombre) return true;
	return false;
}

Ref<StyleBoxFlat> EstiloFicha(Color borde) {
	Ref<StyleBoxFlat> s;
	s.instantiate();
	s->set_bg_color(Color(0.04, 0.05, 0.16, 0.9));
	s->set_border_width_all(2);
	s->set_border_color(borde);
	s->set_corner_radius_all(3);
	s->set_corner_detail(1);
	s->set_shadow_color(Color(0, 0, 0, 0.5));
	s->set_shadow_size(2);
	s->set_shadow_offset(Vector2(2, 2));
	s->set_content_margin(SIDE_LEFT, 5);
	s->set_content_margin(SIDE_RIGHT, 5);
	s->set_content_margin(SIDE_TOP, 4);
	s->set_content_margin(SIDE_BOTTOM, 4);
	return s;
}

void PintarBorde(Control* ficha, Color borde) {
	if (ficha->has_meta("borde") && (Color)ficha->get_meta("borde") == borde) return;
	ficha->set_meta("borde", borde);
	ficha->add_theme_stylebox_override("panel", EstiloFicha(borde));
}

void FijarTexto(Control* ficha, const char* ruta, const String& texto) {
	Label* l = Object::cast_to<Label>(ficha->get_node_or_null(ruta));
	if (l) l->set_text(texto);
}

void FijarBarra(Control* ficha, const char* fila, int valor, int maximo) {
	String base = String("V/Cabecera/Info/") + fila;
	ProgressBar* barra = Object::cast_to<ProgressBar>(ficha->get_node_or_null(NodePath(base + "/Barra")));
	if (barra) { barra->set_max(UtilityFunctions::maxi(1, maximo)); barra->set_value(valor); }
	Label* l = Object::cast_to<Label>(ficha->get_node_or_null(NodePath(base + "/Valor")));
	if (l) l->set_text(String::num_int64(valor));
}

// PA como rombos (llenos = disponibles)
void PintarPA(Control* ficha, int pa, int pa_max) {
	HBoxContainer* caja = Object::cast_to<HBoxContainer>(ficha->get_node_or_null("V/Cabecera/Info/Fila/PA"));
	if (!caja) return;
	int clave = pa * 100 + pa_max;
	if (caja->has_meta("pintado") && (int)caja->get_meta("pintado") == clave) return;
	caja->set_meta("pintado", clave);

	TypedArray<Node> viejos = caja->get_children();
	for (int i = 0; i < viejos.size(); i++) {
		Node* n = Object::cast_to<Node>(viejos[i]);
		caja->remove_child(n);
		n->queue_free();
	}
	for (int i = 0; i < pa_max; i++) {
		ColorRect* r = memnew(ColorRect);
		r->set_custom_minimum_size(Vector2(5, 5));
		r->set_pivot_offset(Vector2(2.5, 2.5));
		r->set_rotation((float)Math_PI * 0.25f);
		r->set_color(i < pa ? Color(0.4, 0.8, 1.0) : Color(0.16, 0.18, 0.32));
		r->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
		caja->add_child(r);
	}
}

void RellenarFichaHeroe(Control* f, Node* p, Node* activo) {
	FijarTexto(f, "V/Cabecera/Info/Fila/Nombre", p->get_meta("nombre_display", p->get_name()));
	FijarBarra(f, "Vida", p->get_meta("hp_actual", 0), p->get_meta("hp_max", 100));
	FijarBarra(f, "Hambre", p->get_meta("hambre_actual", 0), p->get_meta("hambre_max", 10));
	FijarBarra(f, "Cordura", p->get_meta("sanidad_actual", 0), p->get_meta("sanidad_max", 100));
	PintarPA(f, p->get_meta("pa_actual", 0), p->get_meta("pa_max", 0));

	FijarTexto(f, "V/Detalle/Stats", GenerarTextoStats(p));
	FijarTexto(f, "V/Detalle/Recursos/Oro", String::num_int64((int)p->get_meta("oro", 0)));
	FijarTexto(f, "V/Detalle/Recursos/Comida", "Comida: " + String::num_int64((int)p->get_meta("comida_bolsa", 0)));

	int max_slots = p->get_meta("max_slots", 3);
	TypedArray<Resource> inv = p->get_meta("items_lista", TypedArray<Resource>());
	for (int s = 1; s <= 4; s++) {
		TextureRect* slot = Object::cast_to<TextureRect>(f->get_node_or_null(NodePath("V/Detalle/Slots/Slot" + String::num_int64(s))));
		if (!slot) continue;
		slot->set_visible(s <= max_slots);
		Ref<Texture2D> icono;
		if (s - 1 < inv.size()) {
			Ref<Resource> item = inv[s - 1];
			if (item.is_valid()) icono = item->get("icon");
		}
		slot->set_texture(icono);
	}

	bool muerto = p->get_meta("esta_muerto", false);
	bool escapado = p->get_meta("ha_escapado", false);
	bool agotado = p->get_meta("ha_movido", false) || (int)p->get_meta("pa_actual", 0) <= 0;
	if (muerto) f->set_modulate(Color(0.7, 0.3, 0.3, 0.8));
	else if (escapado) f->set_modulate(Color(0.6, 0.6, 0.7, 0.55));
	else if (agotado && p != activo) f->set_modulate(Color(0.6, 0.6, 0.65, 1.0));
	else f->set_modulate(Color(1, 1, 1, 1));
	PintarBorde(f, p == activo ? BORDE_ACTIVO : BORDE_NORMAL);
}

void RellenarFichaEnemigo(Control* f, Node3D* e) {
	FijarTexto(f, "V/Cabecera/Info/Fila/Nombre", e->get_meta("nombre_display", "Enemigo"));
	FijarBarra(f, "Vida", e->get_meta("hp_actual", 1), e->get_meta("hp_max", 1));
	FijarTexto(f, "V/Detalle/Stats", "Fuerza: " + String::num_int64((int)e->get_meta("fuerza", 0)) +
										 "\nDefensa: " + String::num_int64((int)e->get_meta("defensa", 0)));
	f->set_meta("enemigo", e->get_name());

	int frame = e->get_meta("frame", 11);
	if (!f->has_meta("frame_pintado") || (int)f->get_meta("frame_pintado") != frame) {
		f->set_meta("frame_pintado", frame);
		if (m_hoja_personajes.is_null())
			m_hoja_personajes = ResourceLoader::get_singleton()->load("res://Assets/2D Pixel Dungeon Asset Pack/character and tileset/Dungeon_Character_2.png");
		Ref<AtlasTexture> retrato;
		retrato.instantiate();
		retrato->set_atlas(m_hoja_personajes);
		retrato->set_region(Rect2((frame % 7) * 16, (frame / 7) * 16, 16, 16));
		TextureRect* r = Object::cast_to<TextureRect>(f->get_node_or_null("V/Cabecera/Retrato"));
		if (r) r->set_texture(retrato);
	}
	bool alerta = e->get_meta("in_combat", false);
	PintarBorde(f, alerta ? BORDE_ENEMIGO_ALERTA : BORDE_ENEMIGO);
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

// Qué fichas se ven: fijadas, el activo y lo que hay en pantalla (en AUTO) y el enemigo bajo el cursor
void ActualizarVisibilidadFichas() {
	Control* panel = PanelFichas();
	Node* pm = GetPartyManager();
	Node* dm = GetDungeon();
	if (!panel || !pm || !dm) return;
	Camera3D* cam = Object::cast_to<Camera3D>(dm->get_node_or_null("Camera3D"));
	Node* activo = Object::cast_to<Node>(pm->call("GetActiveCharacter"));

	auto en_pantalla = [&](Node3D* n) {
		return n && n->is_visible() && cam && cam->is_position_in_frustum(n->get_global_position() + Vector3(0, 0.5, 0));
	};

	TypedArray<Node> heroes = GetPersonajesValidos();
	for (int i = 0; i < heroes.size(); i++) {
		Node* p = Object::cast_to<Node>(heroes[i]);
		String nombre = p->get_name();
		Control* f = Object::cast_to<Control>(panel->get_node_or_null(NodePath(nombre)));
		if (!f) continue;
		Node3D* token = Object::cast_to<Node3D>(dm->get_node_or_null(NodePath(nombre)));
		f->set_visible(FichaFijada(nombre) || (fichas_auto && (p == activo || en_pantalla(token))));
	}

	Vector2i hover = dm->get_meta("hover_celda", Vector2i(-1, -1));
	String nombre_hover = "Enemy_" + String::num_int64(hover.x) + "_" + String::num_int64(hover.y);
	std::vector<Node3D*> enemigos;
	TypedArray<Node> hijos = dm->get_children();
	for (int i = 0; i < hijos.size(); i++) {
		Node3D* e = Object::cast_to<Node3D>(hijos[i]);
		if (!e || !e->is_visible()) continue;
		String n = e->get_name();
		if (!n.begins_with("Enemy_") || n.ends_with("_muerto")) continue;
		if (n == nombre_hover) enemigos.insert(enemigos.begin(), e);
		else if (fichas_auto && en_pantalla(e)) enemigos.push_back(e);
	}
	for (int k = 0; k < MAX_FICHAS_ENEMIGO; k++) {
		Control* f = Object::cast_to<Control>(panel->get_node_or_null(NodePath("Enemigo" + String::num_int64(k))));
		if (!f) continue;
		bool usada = k < (int)enemigos.size();
		f->set_visible(usada);
		if (usada) RellenarFichaEnemigo(f, enemigos[k]);
	}
}

void ActualizarMarcadorTurno(Caller* instance) {
	if (!self) return;
	
	Node* pm = GetPartyManager();
	if (!pm) return;
	
	int floor = pm->has_meta("piso_actual")  ? (int)pm->get_meta("piso_actual")  : 1;
	int turn  = pm->has_meta("turno_actual") ? (int)pm->get_meta("turno_actual") : 1;
	
	Label* turn_marker = Object::cast_to<Label>(self->get_node_or_null("MarcadorTurnos"));
	if (turn_marker) {
		turn_marker->set_text("FLOOR " + String::num_int64(floor) + "   -   TURN " + String::num_int64(turn));
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
	if (!FichaFijada(nombre)) fichas_fijadas.push_back(nombre);
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
		String result_txt = "[center]Roll Result:\n\n" + color + "[b][font_size=50]" + String::num_int64(roll) + "[/font_size][/b][/color][/center]";

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
					String txt = "[center]Success Probability: [b]" + String::num_int64(save_prob) + "%[/b]\n\n";
					txt += "[shake rate=20.0 level=5 connected=1]Rolling dice...[/shake]\n\n";
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
