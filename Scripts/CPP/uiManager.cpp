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
}

JENOVA_SCRIPT_BEGIN

// DECLARACIONES PREVIAS (Para poder llamarlas directamente en C++)
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

void ActualizarStatsUI(Caller* instance) {
	TypedArray<Node> characters = GetPersonajesValidos();

	for (int i = 0; i < characters.size(); i++) {
		Node* p = Object::cast_to<Node>(characters[i]);
		if (!p->has_meta("hp_actual")) continue;

		String base_path = "GridContainer/" + String(p->get_name()) + "/MarginContainer/VBoxContainer/";

		ProgressBar* hp_bar     = Object::cast_to<ProgressBar>(self->get_node_or_null(NodePath(base_path + "Life_Bar")));
		ProgressBar* hunger_bar = Object::cast_to<ProgressBar>(self->get_node_or_null(NodePath(base_path + "Hunger_Bar")));
		ProgressBar* sanity_bar = Object::cast_to<ProgressBar>(self->get_node_or_null(NodePath(base_path + "Sanity_Bar")));

		if (hp_bar)     hp_bar->set_value((int)p->get_meta("hp_actual"));
		if (hunger_bar) hunger_bar->set_value((int)p->get_meta("hambre_actual"));
		if (sanity_bar) sanity_bar->set_value((int)p->get_meta("sanidad_actual"));

		Label* stats_label = Object::cast_to<Label>(self->get_node_or_null(NodePath(base_path + "Stats_Box/Label")));
		if (stats_label) stats_label->set_text(GenerarTextoStats(p));
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

void DescansarYContinuar(Caller* instance) {
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
				
				int current_hp = child->has_meta("hp_actual") ? (int)child->get_meta("hp_actual") : 0;
				int max_hp     = child->has_meta("hp_max")    ? (int)child->get_meta("hp_max")    : 100;
				child->set_meta("hp_actual", UtilityFunctions::min(max_hp, current_hp + 10));
				
				if (!(bool)child->get_meta("esta_muerto")) {
					int current_hunger = child->has_meta("hambre_actual")  ? (int)child->get_meta("hambre_actual")  : 7;
					int current_sanity = child->has_meta("sanidad_actual") ? (int)child->get_meta("sanidad_actual") : 100;
					
					child->set_meta("hambre_actual", UtilityFunctions::min(7, current_hunger + 3));
					child->set_meta("sanidad_actual", UtilityFunctions::min(100, current_sanity + 50));
				}
			}
		}
	}

	// LLAMADAS DIRECTAS EN C++ (Adiós bugs)
	ActualizarMarcadorTurno(instance);
	ActualizarStatsUI(instance);
	UpdateWarriorUI(instance);

	Node* dungeon = self->get_parent()->get_node_or_null("DungeonManager");
	if (dungeon) {
		dungeon->call("AvanzarAlSiguientePiso");
	}
}

void DebugForzarSalida(Caller* instance) {
	if (!self) return;
	
	Node* pm = GetPartyManager();
	Node* dungeon = self->get_parent()->get_node_or_null("DungeonManager");

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
	Node* pm = GetPartyManager();
	if (!pm) return;
	
	Node* active_char = Object::cast_to<Node>(pm->call("GetActiveCharacter"));
	TypedArray<Node> characters = GetPersonajesValidos();

	for (int i = 0; i < characters.size(); i++) {
		Node* p = Object::cast_to<Node>(characters[i]);
		
		String base_path = "GridContainer/" + String(p->get_name());
		String portrait_path = base_path + "/MarginContainer/VBoxContainer/Cabecera/Retrato";

		Control* portrait = Object::cast_to<Control>(self->get_node_or_null(NodePath(portrait_path)));

		bool has_moved   = p->has_meta("ha_movido")   ? (bool)p->get_meta("ha_movido")   : false;
		bool has_escaped = p->has_meta("ha_escapado") ? (bool)p->get_meta("ha_escapado") : false;
		bool is_dead     = p->has_meta("esta_muerto") ? (bool)p->get_meta("esta_muerto") : false; 
		
		if (portrait) {
			if (is_dead) {
				portrait->set_modulate(Color(0.3, 0.0, 0.0, 0.8)); 
			} else if (has_escaped) {
				portrait->set_modulate(Color(0.2, 0.2, 0.2, 0.3)); 
			} else if (has_moved) {
				portrait->set_modulate(Color(0.3, 0.3, 0.3, 1.0)); 
			} else if (p == active_char) {
				portrait->set_modulate(Color(0.984, 0.6, 0.0, 1.0)); 
			} else {
				portrait->set_modulate(Color(1.0, 1.0, 1.0, 1.0)); 
			}
		}
		
		int gold = p->has_meta("oro") ? (int)p->get_meta("oro") : 0;
		int food = p->has_meta("comida_bolsa") ? (int)p->get_meta("comida_bolsa") : 0;
		int max_slots = p->has_meta("max_slots") ? (int)p->get_meta("max_slots") : 3;
		TypedArray<Resource> inventory = p->has_meta("items_lista") ? (TypedArray<Resource>)p->get_meta("items_lista") : TypedArray<Resource>();

		String gold_path = base_path + "/MarginContainer/VBoxContainer/Inventario/Recursos/Oro";
		String food_path = base_path + "/MarginContainer/VBoxContainer/Inventario/Recursos/Comida";
		
		Label* lbl_gold = Object::cast_to<Label>(self->get_node_or_null(NodePath(gold_path)));
		if (lbl_gold) lbl_gold->set_text("Gold: " + String::num(gold));
		
		Label* lbl_food = Object::cast_to<Label>(self->get_node_or_null(NodePath(food_path)));
		if (lbl_food) lbl_food->set_text("Food: " + String::num(food));

		for (int s = 1; s <= 4; s++) {
			String slot_path = base_path + "/MarginContainer/VBoxContainer/Inventario/SlotsItems/Slot" + String::num_int64(s);
			TextureRect* slot = Object::cast_to<TextureRect>(self->get_node_or_null(NodePath(slot_path)));
			
			if (!slot) continue;

			if (s > max_slots) {
				slot->set_visible(false);
			} else {
				slot->set_visible(true);
				
				if (s - 1 < inventory.size()) {
					Variant item_var = inventory[s - 1];
					Ref<Resource> item = item_var;
					
					if (item.is_valid()) {
						Variant icon_var = item->get("icon"); 
						Ref<Texture2D> icon_tex = icon_var;
						if (icon_tex.is_valid()) slot->set_texture(icon_tex);
					} 
				} else {
					Ref<Texture2D> empty_tex;
					slot->set_texture(empty_tex);
				}
			}
		}
	}
}

void InicializarTodaLaUI(Caller* instance) {
	TypedArray<Node> characters = GetPersonajesValidos();

	for (int i = 0; i < characters.size(); i++) {
		Node* p = Object::cast_to<Node>(characters[i]);
		if (!p->has_meta("hp_actual")) continue; 

		String name = p->get_name();
		String base_path = "GridContainer/" + name + "/MarginContainer/VBoxContainer/";
		
		Label* l_name           = Object::cast_to<Label>(self->get_node_or_null(NodePath(base_path + "Cabecera/Nombre")));
		ProgressBar* hp_bar     = Object::cast_to<ProgressBar>(self->get_node_or_null(NodePath(base_path + "Life_Bar")));
		ProgressBar* hunger_bar = Object::cast_to<ProgressBar>(self->get_node_or_null(NodePath(base_path + "Hunger_Bar")));
		ProgressBar* sanity_bar = Object::cast_to<ProgressBar>(self->get_node_or_null(NodePath(base_path + "Sanity_Bar")));
		TextureRect* portrait   = Object::cast_to<TextureRect>(self->get_node_or_null(NodePath(base_path + "Cabecera/Retrato")));
		
		if (portrait && p->has_meta("retrato_path")) {
			String path = p->get_meta("retrato_path");
			if (!path.is_empty()) {
				Ref<Texture2D> tex = ResourceLoader::get_singleton()->load(path);
				if (tex.is_valid()) portrait->set_texture(tex); 
			}
		}
		
		Control* stats_box = Object::cast_to<Control>(self->get_node_or_null(NodePath(base_path + "Stats_Box")));
		Label* stats_label = Object::cast_to<Label>(self->get_node_or_null(NodePath(base_path + "Stats_Box/Label")));

		if (stats_box) {
			stats_box->set("custom_minimum_size", Vector2(0, 0));
			stats_box->set_visible(false);
		}
		
		if (l_name) l_name->set_text(p->get_meta("nombre_display"));
		
		if (hp_bar) {
			hp_bar->set_max((int)p->get_meta("hp_max"));
			hp_bar->set_value((int)p->get_meta("hp_actual"));
		}
		if (hunger_bar) { hunger_bar->set_max(7); hunger_bar->set_value(7); }
		if (sanity_bar) { sanity_bar->set_max(100); sanity_bar->set_value(100); }
		
		if (stats_label) stats_label->set_text(GenerarTextoStats(p));
		
		Button* btn_card = Object::cast_to<Button>(self->get_node_or_null(NodePath("GridContainer/" + name + "/BtnFicha")));
		if (btn_card) {
			Callable click_fn = Callable(self, StringName("SeleccionarPersonajeDesdeUI")).bind(name);
			Callable show_fn  = Callable(self, StringName("MostrarStats")).bind(name);
			Callable hide_fn  = Callable(self, StringName("OcultarStats")).bind(name);
			
			if (!btn_card->is_connected("pressed", click_fn))      btn_card->connect("pressed", click_fn);
			if (!btn_card->is_connected("mouse_entered", show_fn)) btn_card->connect("mouse_entered", show_fn);
			if (!btn_card->is_connected("mouse_exited", hide_fn))  btn_card->connect("mouse_exited", hide_fn);
		}
	}
}

// ==========================================
// MOUSE FUNCTIONS
// ==========================================

void SeleccionarPersonajeDesdeUI(Caller* instance, String nombre) {
	if (!self) return;

	Node* dungeon = self->get_parent()->get_node_or_null("DungeonManager");
	if (dungeon && dungeon->has_meta("is_moving") && (bool)dungeon->get_meta("is_moving")) {
		return;
	}

	Node* pm = GetPartyManager();
	if (!pm) return;
	
	pm->call("CambiarPersonajeActivo", nombre);
}

void MostrarStats(Caller* instance, String nombre) {
	if (!self) return;
	Control* stats_box = Object::cast_to<Control>(self->get_node_or_null("GridContainer/" + nombre + "/MarginContainer/VBoxContainer/Stats_Box"));
	if (stats_box) {
		stats_box->set_visible(true);
		Ref<Tween> tween = stats_box->create_tween();
		tween->tween_property(stats_box, "custom_minimum_size", Vector2(0, 140), 0.2)->set_trans(Tween::TRANS_CUBIC)->set_ease(Tween::EASE_OUT);
	}
}

void OcultarStats(Caller* instance, String nombre) {
	if (!self) return;
	Control* stats_box = Object::cast_to<Control>(self->get_node_or_null("GridContainer/" + nombre + "/MarginContainer/VBoxContainer/Stats_Box"));
	if (stats_box) {
		Ref<Tween> tween = stats_box->create_tween();
		tween->tween_property(stats_box, "custom_minimum_size", Vector2(0, 0), 0.15)->set_trans(Tween::TRANS_CUBIC)->set_ease(Tween::EASE_OUT);
		tween->connect("finished", Callable(stats_box, StringName("hide")));
	}
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
	
	Node* dungeon = self->get_parent()->get_node_or_null("DungeonManager");
	if (dungeon) {
		dungeon->call("BajarMapa");
		dungeon->call("FinalizarEvento");
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
		if (pm) pm->call(fn_call, param, hero);
	}
}

// ¡AQUÍ ESTÁ EL Caller* instance QUE FALTABA!
void MostrarNumeroDado(Caller* instance, int roll, bool success) { 
	if (!self || !m_event_screen) return;

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

void MostrarEventoVisual(Caller* instance, Variant event_data) {
	if (!self || !m_event_screen) return;

	Ref<Resource> res = event_data;
	if (res.is_null()) return;

	self->set_meta("evento_actual", res);

	Node* pm = GetPartyManager();
	if (pm) {
		Node* hero = Object::cast_to<Node>(pm->call("GetActiveCharacter")); 
		if (hero) self->set_meta("heroe_en_peligro", hero);
	}

	String title = res->get("titulo");
	String desc  = res->get("descripcion");
	Array options = res->get("opciones");
	Ref<Texture2D> img = res->get("ilustracion"); 

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

	Node* dungeon = self->get_parent()->get_node_or_null("DungeonManager");
	if (dungeon) dungeon->call("SubirMapa");

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
