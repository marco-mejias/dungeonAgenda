/* Jenova C++ Node Base Script (Meteora) - PartyManager */

#include <Godot/godot.hpp>
#include <Godot/classes/node.hpp>
#include <Godot/classes/node3d.hpp>
#include <Godot/classes/canvas_layer.hpp>
#include <Godot/classes/tween.hpp>
#include <Godot/classes/property_tweener.hpp>
#include <Godot/classes/callback_tweener.hpp>
#include <Godot/classes/interval_tweener.hpp>
#include <Godot/variant/utility_functions.hpp>
#include <vector>

using namespace godot;
using namespace jenova::sdk;

namespace {
	Node* self = nullptr;
	Node* act_char = nullptr;

	Node* m_ui_manager = nullptr;
	Node* m_dungeon_manager = nullptr;

	bool MetaBool(Node* n, const char* key) { return n && n->has_meta(key) && (bool)n->get_meta(key); }

	std::vector<Node*> Heroes() {
		std::vector<Node*> out;
		if (!self) return out;
		TypedArray<Node> hijos = self->get_children();
		for (int i = 0; i < hijos.size(); i++) {
			Node* h = Object::cast_to<Node>(hijos[i]);
			if (h && h->get_name() != StringName("DatosEquipo")) out.push_back(h);
		}
		return out;
	}

	bool PuedeJugar(Node* h) {
		return h && !MetaBool(h, "esta_muerto") && !MetaBool(h, "ha_escapado") && !MetaBool(h, "ha_movido") && (int)h->get_meta("pa_actual", 0) > 0;
	}

	// El héroe se desploma de lado y desaparece
	void OcultarFicha(Node* h) {
		if (!m_dungeon_manager || !h) return;
		Node3D* ficha = Object::cast_to<Node3D>(m_dungeon_manager->get_node_or_null(NodePath(String(h->get_name()))));
		if (!ficha) return;
		Node3D* cuerpo = Object::cast_to<Node3D>(ficha->get_node_or_null("Cuerpo"));
		if (!cuerpo) { ficha->set_visible(false); return; }
		Ref<Tween> tw = ficha->create_tween();
		tw->tween_property(cuerpo, "rotation:z", 1.45, 0.35)->set_trans(Tween::TRANS_BOUNCE)->set_ease(Tween::EASE_OUT);
		tw->parallel()->tween_property(cuerpo, "position:y", 0.2, 0.35);
		tw->tween_interval(0.5);
		tw->tween_callback(Callable(ficha, StringName("hide")));
		Node* feel = self ? self->get_node_or_null("/root/Feel") : nullptr;
		if (feel) feel->call("sacudir", 0.35);
	}

	void Morir(Node* h, const String& causa) {
		h->set_meta("esta_muerto", true);
		h->set_meta("pa_actual", 0);
		UtilityFunctions::print("[PartyManager] ", h->get_name(), " HA MUERTO (", causa, ").");
		OcultarFicha(h);
	}

	void SeleccionarSiguiente() {
		act_char = nullptr;
		for (Node* h : Heroes()) {
			if (PuedeJugar(h)) { act_char = h; break; }
		}
	}

	void ComprobarFinDelPiso() {
		bool alguno_escapo = false;
		for (Node* h : Heroes()) {
			if (MetaBool(h, "ha_escapado")) { alguno_escapo = true; continue; }
			if (!MetaBool(h, "esta_muerto")) return; // queda alguien vivo dentro
		}
		if (!alguno_escapo) {
			UtilityFunctions::print("\n[PartyManager] *** TODO EL GRUPO HA CAÍDO ***\n");
			return;
		}

		Node* audio = self->get_node_or_null("/root/AudioManager");
		if (audio) audio->call("play_music", "fireplace");
		UtilityFunctions::print("\n***********************************");
		UtilityFunctions::print(" ¡TODOS A SALVO! ENCENDIENDO FOGATA ");
		UtilityFunctions::print("***********************************\n");

		CanvasLayer* fogata = Object::cast_to<CanvasLayer>(self->get_parent()->get_node_or_null("CanvasLayer3"));
		if (fogata) fogata->set_visible(true);
	}
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
	if (!MetaBool(new_char, "ha_movido")) {
		act_char = new_char;
		UtilityFunctions::print("[PartyManager] Active character: ", new_char->get_meta("nombre_display"));
	} else {
		UtilityFunctions::print("[PartyManager] ", new_char->get_meta("nombre_display"), " is exhausted this turn.");
	}
}

void SeleccionarPrimerDisponible(Caller* instance) {
	SeleccionarSiguiente();
	if (m_ui_manager) m_ui_manager->call("UpdateWarriorUI");
}

Variant GetCurrentStat(Caller* instance, String stat_name) {
	if (!act_char || !act_char->has_meta(stat_name)) return Variant();
	return act_char->get_meta(stat_name);
}

Vector2i GetActiveGridPos(Caller* instance) {
	if (!act_char || !act_char->has_meta("grid_pos")) return Vector2i(-1, -1);
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
	if (!self) return;

	if (act_char) {
		act_char->set_meta("ha_movido", true);
		UtilityFunctions::print("[PartyManager] Action completed for ", act_char->get_meta("nombre_display"));
	}

	// 1. ¿TERMINA EL DÍA? (nadie vivo y dentro del mapa conserva PA)
	bool dia_terminado = true;
	for (Node* h : Heroes()) {
		if (PuedeJugar(h)) { dia_terminado = false; break; }
	}

	// 2. CAE LA NOCHE: supervivencia y recarga
	if (dia_terminado) {
		int turno = (int)self->get_meta("turno_actual", 1) + 1;
		self->set_meta("turno_actual", turno);

		std::vector<Node*> heroes = Heroes();
		for (Node* h : heroes) {
			if (MetaBool(h, "ha_escapado") || MetaBool(h, "esta_muerto")) continue;

			// HAMBRE: -1 por día. Si queda poca y hay comida en la bolsa, se come sola.
			int hambre = (int)h->get_meta("hambre_actual", 10) - 1;
			int comida = h->get_meta("comida_bolsa", 0);
			if (hambre <= 3 && comida > 0) {
				hambre = UtilityFunctions::mini((int)h->get_meta("hambre_max", 10), hambre + 5);
				h->set_meta("comida_bolsa", comida - 1);
				UtilityFunctions::print("[Survival] ", h->get_name(), " come algo de la bolsa.");
			}
			h->set_meta("hambre_actual", UtilityFunctions::maxi(0, hambre));
			if (hambre <= 0) {
				Morir(h, "hambre");
				continue;
			}

			// CORDURA: perder al grupo de vista asusta
			Vector2i mi_pos = h->get_meta("grid_pos", Vector2i(0, 0));
			int min_dist = 999;
			for (Node* c : heroes) {
				if (c == h || MetaBool(c, "ha_escapado") || MetaBool(c, "esta_muerto")) continue;
				Vector2i p = c->get_meta("grid_pos", Vector2i(0, 0));
				min_dist = UtilityFunctions::mini(min_dist, abs(mi_pos.x - p.x) + abs(mi_pos.y - p.y));
			}
			if (min_dist > 3 && min_dist != 999) {
				int cordura = h->get_meta("sanidad_actual", 100);
				h->set_meta("sanidad_actual", UtilityFunctions::maxi(0, cordura - 15));
				UtilityFunctions::print("[Survival] ", h->get_name(), " loses 15 sanity due to isolation.");
			}

			h->set_meta("ha_movido", false);
			h->set_meta("pa_actual", h->get_meta("pa_max", 3));
		}

		UtilityFunctions::print("[PartyManager] New day. Current turn: ", turno);
		if (m_ui_manager) {
			m_ui_manager->call("ActualizarMarcadorTurno");
			m_ui_manager->call("ActualizarStatsUI");
		}
	}

	// 3. SIGUIENTE PERSONAJE DISPONIBLE
	SeleccionarSiguiente();

	if (m_ui_manager) m_ui_manager->call("UpdateWarriorUI");
	if (m_dungeon_manager) m_dungeon_manager->call("RefrescarLuces");
}

void CambiarPersonajeActivo(Caller* instance, String new_name) {
	if (!self) return;
	Node* child = self->get_node_or_null(NodePath(new_name));
	if (!child || MetaBool(child, "esta_muerto") || MetaBool(child, "ha_escapado")) return;

	if ((int)child->get_meta("pa_actual", 0) <= 0) {
		UtilityFunctions::print("[PartyManager] ", new_name, " no tiene PA restantes. Elige otro.");
		return;
	}

	act_char = child;
	UtilityFunctions::print("[PartyManager] Selección manual: ", act_char->get_meta("nombre_display"));

	if (m_ui_manager) m_ui_manager->call("UpdateWarriorUI");
	if (m_dungeon_manager) m_dungeon_manager->call("UpdateHighlightForWarrior");
}

// ==========================================
// CUSTOM EVENTS
// ==========================================

void RecibirDano(Caller* instance, int damage_amount, Node* target_hero) {
	if (!self || !target_hero) return;

	int hp = UtilityFunctions::maxi(0, (int)target_hero->get_meta("hp_actual", 0) - damage_amount);
	target_hero->set_meta("hp_actual", hp);
	UtilityFunctions::print("[PartyManager] Effect: ", target_hero->get_meta("nombre_display"), " takes ", damage_amount, " damage. Remaining HP: ", hp);

	if (hp <= 0 && !MetaBool(target_hero, "esta_muerto")) {
		Morir(target_hero, "heridas");
		ComprobarFinDelPiso();
	}

	if (m_ui_manager) m_ui_manager->call("ActualizarStatsUI");
}

void PersonajeEscapa(Caller* instance, Node* hero) {
	if (!self || !hero) return;
	hero->set_meta("ha_escapado", true);
	UtilityFunctions::print("[PartyManager] ", hero->get_meta("nombre_display"), " has gone down the stairs.");
	ComprobarFinDelPiso();
}

// ==========================================
// INITIALIZATION
// ==========================================

void OnAwake(Caller* instance) {
	self = GetSelf<Node>(instance);
	act_char = nullptr;
	Node* parent = self ? self->get_parent() : nullptr;
	if (parent) {
		m_ui_manager = parent->get_node_or_null("CanvasLayer");
		m_dungeon_manager = parent->get_node_or_null("ContenedorMapa/SubViewport/DungeonManager");
	}
}

void OnReady(Caller* instance) {
	if (!self) return;

	Node* team_data = self->get_node_or_null("DatosEquipo");
	if (!team_data) return;

	self->set_meta("piso_actual", 1);
	self->set_meta("turno_actual", 1);

	TypedArray<Resource> hero_list = team_data->get("lista_heroes");
	for (int i = 0; i < hero_list.size(); i++) {
		Ref<Resource> res = hero_list[i];
		if (res.is_null()) continue;

		String name = res->get("nombre_display");
		int hp = res->get("vida_max");
		int spd = res->get("velocidad");

		Node* soul = memnew(Node);
		soul->set_name(name);
		soul->set_meta("nombre_display", name);
		soul->set_meta("hp_max", hp);
		soul->set_meta("hp_actual", hp);
		soul->set_meta("velocidad", spd);
		soul->set_meta("retrato_path", res->get("retrato_path"));
		soul->set_meta("ha_movido", false);
		soul->set_meta("defensa", res->get("defensa"));
		soul->set_meta("fuerza", res->get("fuerza"));
		soul->set_meta("magia", res->get("magia"));
		soul->set_meta("suerte", res->get("suerte"));
		soul->set_meta("hambre_max", 10);
		soul->set_meta("hambre_actual", 10);
		soul->set_meta("sanidad_max", 100);
		soul->set_meta("sanidad_actual", 100);
		soul->set_meta("esta_muerto", false);
		soul->set_meta("ha_escapado", false);
		soul->set_meta("in_combat", false);
		soul->set_meta("pa_max", spd);
		soul->set_meta("pa_actual", spd);

		// Radio de visión propio (antes se usaban los PA, y con 0 PA no veías nada)
		Variant radio = res->get("radio_luz");
		soul->set_meta("radio_luz", radio.get_type() == Variant::NIL ? 4 : (int)radio);

		soul->set_meta("oro", 0);
		soul->set_meta("comida_bolsa", 0);

		// El tanque es una mula de carga
		soul->set_meta("max_slots", name == "Doom Snailer" ? 4 : 3);
		soul->set_meta("items_lista", TypedArray<Resource>());

		self->add_child(soul);

		if (m_dungeon_manager) m_dungeon_manager->call("SpawnVisualCharacter", res, i);
	}

	SeleccionarSiguiente();
	if (m_ui_manager) m_ui_manager->call("UpdateWarriorUI");
	if (m_dungeon_manager) m_dungeon_manager->call("UpdateHighlightForWarrior");
}

void OnDestroy(Caller* instance) {
	self = nullptr;
	act_char = nullptr;
	m_ui_manager = nullptr;
	m_dungeon_manager = nullptr;
}

JENOVA_SCRIPT_END
