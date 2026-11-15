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
#include "Scripts/CPP/Inventario/Inventario.hpp"
#include <vector>

using namespace godot;
using namespace jenova::sdk;

namespace {
	Node* self = nullptr;
	Node* act_char = nullptr;

	Node* m_ui_manager = nullptr;
	Node* m_dungeon_manager = nullptr;

	bool MetaBool(Node* n, const char* key) { return n && n->has_meta(key) && (bool)n->get_meta(key); }

	// Our children are the heroes (except the DatosEquipo node)
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

	// Alive, on the map, hasn't ended their turn and has PA left
	bool PuedeJugar(Node* h) {
		return h && !MetaBool(h, "esta_muerto") && !MetaBool(h, "ha_escapado") && !MetaBool(h, "ha_movido") && (int)h->get_meta("pa_actual", 0) > 0;
	}

	// The hero collapses sideways and disappears
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

	// A hero dies: no more PA, their card and token go away
	void Morir(Node* h, const String& causa) {
		h->set_meta("esta_muerto", true);
		h->set_meta("pa_actual", 0);
		UtilityFunctions::print("[PartyManager] ", h->get_name(), " HA MUERTO (", causa, ").");
		OcultarFicha(h);
	}

	// The first hero that can still play becomes the active one
	void SeleccionarSiguiente() {
		act_char = nullptr;
		for (Node* h : Heroes()) {
			if (PuedeJugar(h)) { act_char = h; break; }
		}
	}

	// Hero node by name (null for DatosEquipo or a missing name)
	Node* Heroe(const String& nombre) {
		if (!self || nombre.is_empty() || nombre == "DatosEquipo") return nullptr;
		return self->get_node_or_null(NodePath(nombre));
	}

	// Is the campfire screen open?
	bool EnFogata() {
		CanvasLayer* fogata = self ? Object::cast_to<CanvasLayer>(self->get_parent()->get_node_or_null("CanvasLayer3")) : nullptr;
		return fogata && fogata->is_visible();
	}

	bool EnMapa(Node* h) { return h && !MetaBool(h, "esta_muerto") && !MetaBool(h, "ha_escapado") && h->has_meta("grid_pos"); }

	// Two heroes can trade items when they're next to each other (or anywhere at the campfire)
	bool PuedenIntercambiarObjetos(Node* a, Node* b) {
		if (!a || !b || a == b || MetaBool(a, "esta_muerto") || MetaBool(b, "esta_muerto")) return false;
		if (EnFogata()) return true;
		if (!EnMapa(a) || !EnMapa(b)) return false;
		Vector2i pa = a->get_meta("grid_pos"), pb = b->get_meta("grid_pos");
		return std::abs(pa.x - pb.x) + std::abs(pa.y - pb.y) == 1;
	}

	// Changing equipment can change the light (radio_luz), PA or health: we refresh everything
	void TrasCambiarEquipo(Node* h) {
		if (m_ui_manager) {
			m_ui_manager->call("UpdateWarriorUI");
			m_ui_manager->call("ActualizarStatsUI");
		}
		if (m_dungeon_manager) {
			m_dungeon_manager->call("CalcularMapaDeLuces");
			m_dungeon_manager->call("RefrescarLuces");
		}
	}

	// NIGHTFALL: survival and PA recharge
	void Noche() {
		int turno = (int)self->get_meta("turno_actual", 1) + 1;
		self->set_meta("turno_actual", turno);

		std::vector<Node*> heroes = Heroes();
		for (Node* h : heroes) {
			if (MetaBool(h, "ha_escapado") || MetaBool(h, "esta_muerto")) continue;

			// HUNGER: -1 per day. If it's low and there's food in the bag, they eat on their own.
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

			// SANITY: losing sight of the party is scary
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

			inventario::AvanzarDia(h); // Temporary effects last X days
			h->set_meta("ha_movido", false);
			h->set_meta("pa_actual", h->get_meta("pa_max", 3));
		}

		UtilityFunctions::print("[PartyManager] New day. Current turn: ", turno);
		if (m_ui_manager) {
			m_ui_manager->call("ActualizarMarcadorTurno");
			m_ui_manager->call("ActualizarStatsUI");
		}
	}

	// Everyone left alive has escaped down the stairs: the floor is over
	void ComprobarFinDelPiso() {
		bool alguno_escapo = false;
		for (Node* h : Heroes()) {
			if (MetaBool(h, "ha_escapado")) { alguno_escapo = true; continue; }
			if (!MetaBool(h, "esta_muerto")) return; // Someone alive is still inside
		}
		if (!alguno_escapo) {
			UtilityFunctions::print(String::utf8("\n[PartyManager] *** TODO EL GRUPO HA CA\xC3\x8D" "DO ***\n"));
			return;
		}

		Node* audio = self->get_node_or_null("/root/AudioManager");
		if (audio) audio->call("play_music", "fireplace");
		UtilityFunctions::print("\n***********************************");
		UtilityFunctions::print(String::utf8(" \xC2\xA1TODOS A SALVO! ENCENDIENDO FOGATA "));
		UtilityFunctions::print("***********************************\n");

		CanvasLayer* fogata = Object::cast_to<CanvasLayer>(self->get_parent()->get_node_or_null("CanvasLayer3"));
		if (fogata) fogata->set_visible(true);
	}
}

JENOVA_SCRIPT_BEGIN

// ==========================================
// ACTIVE CHARACTER MANAGEMENT
// ==========================================

// The hero whose turn it is (everyone asks this)
Variant GetActiveCharacter(Caller* instance) {
	return act_char;
}

// Selects a hero (only if they haven't ended their turn yet)
void SetActiveCharacter(Caller* instance, Node* new_char) {
	if (!new_char) return;
	if (!MetaBool(new_char, "ha_movido")) {
		act_char = new_char;
		UtilityFunctions::print("[PartyManager] Active character: ", new_char->get_meta("nombre_display"));
	} else {
		UtilityFunctions::print("[PartyManager] ", new_char->get_meta("nombre_display"), " is exhausted this turn.");
	}
}

// Picks the first hero that can still play and refreshes the UI
void SeleccionarPrimerDisponible(Caller* instance) {
	SeleccionarSiguiente();
	if (m_ui_manager) m_ui_manager->call("UpdateWarriorUI");
}

// A meta of the active hero (null if they don't have it)
Variant GetCurrentStat(Caller* instance, String stat_name) {
	if (!act_char || !act_char->has_meta(stat_name)) return Variant();
	return act_char->get_meta(stat_name);
}

// Grid tile of the active hero
Vector2i GetActiveGridPos(Caller* instance) {
	if (!act_char || !act_char->has_meta("grid_pos")) return Vector2i(-1, -1);
	return act_char->get_meta("grid_pos");
}

// Moves the active hero in the logic (the token moves on its own)
void SetActiveGridPos(Caller* instance, int x, int y) {
	if (!act_char) return;
	act_char->set_meta("grid_pos", Vector2i(x, y));
}

// ==========================================
// TURN FLOW MANAGEMENT
// ==========================================

// The active hero is done: mark them and either pick the next one or end the day
void TerminarTurnoPersonaje(Caller* instance) {
	if (!self) return;

	if (act_char) {
		act_char->set_meta("ha_movido", true);
		UtilityFunctions::print("[PartyManager] Action completed for ", act_char->get_meta("nombre_display"));
	}

	// 1. DOES THE DAY END? (nobody alive and on the map has PA left)
	bool dia_terminado = true;
	for (Node* h : Heroes()) {
		if (PuedeJugar(h)) { dia_terminado = false; break; }
	}

	// 2. If the day ends, before nightfall the awake enemies play; when they finish, the
	// EnemyManager calls IniciarNuevoDia
	if (dia_terminado) {
		Node* em = m_dungeon_manager ? m_dungeon_manager->get_node_or_null("EnemyManager") : nullptr;
		if (em && (bool)em->call("HayEnemigosQueActuan")) {
			act_char = nullptr;
			if (m_ui_manager) m_ui_manager->call("UpdateWarriorUI");
			em->call("IniciarTurnoEnemigos");
			return;
		}
		Noche();
	}

	// 3. NEXT AVAILABLE CHARACTER
	SeleccionarSiguiente();

	if (m_ui_manager) m_ui_manager->call("UpdateWarriorUI");
	if (m_dungeon_manager) m_dungeon_manager->call("RefrescarLuces");
}

// While exploring (Modo.gd), the day goes by with walked tiles: hunger, sanity, effects and PA like
// every night, but with no enemy turn and no change of active hero.
void PasarDiaExplorando(Caller* instance) {
	if (!self) return;
	Noche();
	if (m_ui_manager) m_ui_manager->call("UpdateWarriorUI");
	if (m_dungeon_manager) m_dungeon_manager->call("RefrescarLuces");
}

// Called by the EnemyManager when the enemy turn ends
void IniciarNuevoDia(Caller* instance) {
	if (!self) return;
	Noche();
	SeleccionarSiguiente();
	if (m_ui_manager) m_ui_manager->call("UpdateWarriorUI");
	if (m_dungeon_manager) {
		m_dungeon_manager->call("RefrescarLuces");
		m_dungeon_manager->call("UpdateHighlightForWarrior");
	}
}

// Switches the active hero (clicking another hero or their portrait)
void CambiarPersonajeActivo(Caller* instance, String new_name) {
	if (!self) return;
	Node* child = self->get_node_or_null(NodePath(new_name));
	if (!child || MetaBool(child, "esta_muerto") || MetaBool(child, "ha_escapado")) return;

	if ((int)child->get_meta("pa_actual", 0) <= 0) {
		UtilityFunctions::print("[PartyManager] ", new_name, " no tiene PA restantes. Elige otro.");
		return;
	}

	act_char = child;
	UtilityFunctions::print(String::utf8("[PartyManager] Selecci\xC3\xB3n manual: "), act_char->get_meta("nombre_display"));

	if (m_ui_manager) m_ui_manager->call("UpdateWarriorUI");
	if (m_dungeon_manager) m_dungeon_manager->call("UpdateHighlightForWarrior");
}

// ==========================================
// CUSTOM EVENTS
// ==========================================

// A hero takes damage; at 0 health they die
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

// A hero went down the stairs: out of the floor
void PersonajeEscapa(Caller* instance, Node* hero) {
	if (!self || !hero) return;
	hero->set_meta("ha_escapado", true);
	UtilityFunctions::print("[PartyManager] ", hero->get_meta("nombre_display"), " has gone down the stairs.");
	ComprobarFinDelPiso();
}

// ==========================================
// INVENTORY AND EQUIPMENT (the logic lives in Inventario/Inventario.hpp)
// ==========================================
// Equipping, using and swapping cost no PA. The functions return "" if everything went fine or the
// reason it failed (for the interface).

// Equips item 'indice' from the hero's inventory. "" or the reason it failed
Variant Equipar(Caller* instance, String heroe, int indice) {
	Node* h = Heroe(heroe);
	if (!h) return String::utf8("No existe ese h\xC3\xA9roe");
	String error = inventario::Equipar(h, indice);
	if (error.is_empty()) TrasCambiarEquipo(h);
	return error;
}

// Unequips the slot back into the backpack. "" or the reason it failed
Variant Desequipar(Caller* instance, String heroe, String ranura) {
	Node* h = Heroe(heroe);
	if (!h) return String::utf8("No existe ese h\xC3\xA9roe");
	String error = inventario::Desequipar(h, ranura);
	if (error.is_empty()) TrasCambiarEquipo(h);
	return error;
}

// Returns how many units got in
Variant AnadirObjeto(Caller* instance, String heroe, String id, int cantidad) {
	Node* h = Heroe(heroe);
	if (!h) return 0;
	int n = inventario::Anadir(h, id, cantidad);
	if (m_ui_manager) m_ui_manager->call("UpdateWarriorUI");
	return n;
}

// Uses a consumable (special ones go to Especiales.gd). "" or the reason it failed
Variant UsarObjeto(Caller* instance, String heroe, int indice) {
	Node* h = Heroe(heroe);
	if (!h || MetaBool(h, "esta_muerto")) return String("No puede usarlo");
	String resultado;
	// Consumables with weird effects ("what could possibly go wrong?" potion, panic button):
	// Especiales.gd
	Array inv_especial = inventario::Inventario(h);
	if (indice >= 0 && indice < inv_especial.size()) {
		String id = Dictionary(inv_especial[indice])["id"];
		Node* esp = h->get_node_or_null("/root/Especiales");
		if (esp && (bool)esp->call("es_especial", id)) {
			String error_esp = esp->call("usar", h, id);
			if (!error_esp.is_empty()) return error_esp;
			inventario::Quitar(h, indice, 1);
			UtilityFunctions::print("[PartyManager] ", heroe, " usa ", id);
			TrasCambiarEquipo(h);
			return String("");
		}
	}
	String error = inventario::Usar(h, indice, resultado);
	if (error.is_empty()) {
		UtilityFunctions::print("[PartyManager] ", heroe, " usa un objeto: ", resultado);
		TrasCambiarEquipo(h);
	}
	return error;
}

// At the campfire you can give anything; in the dungeon, only between adjacent heroes
Variant PuedenIntercambiar(Caller* instance, String a, String b) {
	return PuedenIntercambiarObjetos(Heroe(a), Heroe(b));
}

// Gives 'cantidad' units of an item from one hero to another
Variant DarObjeto(Caller* instance, String origen, int indice, String destino, int cantidad) {
	Node* de = Heroe(origen);
	Node* para = Heroe(destino);
	if (!de || !para || de == para) return String::utf8("No existe ese h\xC3\xA9roe");
	if (!PuedenIntercambiarObjetos(de, para)) return String("Tienen que estar juntos (o en la fogata)");
	if (inventario::Lleva(de, "casco_cubo_basura") || inventario::Lleva(para, "casco_cubo_basura"))
		return String::utf8("\xC2\xA1" "Qu\xC3\xA9 peste! Nadie se acerca al del cubo de basura");
	Array inv = inventario::Inventario(de);
	if (indice < 0 || indice >= inv.size()) return String("No existe ese objeto");
	Dictionary e = inv[indice];
	int n = std::min(cantidad, (int)e["cantidad"]);
	int entran = inventario::Anadir(para, e["id"], n);
	if (entran <= 0) return String("No le cabe en la mochila");
	inventario::Quitar(de, indice, entran);
	UtilityFunctions::print("[PartyManager] ", origen, " da ", entran, "x ", e["id"], " a ", destino);
	if (m_ui_manager) m_ui_manager->call("UpdateWarriorUI");
	return entran < n ? String("Solo le caben ") + String::num_int64(entran) : String("");
}

// Applies a list of Efecto.tres to a hero (events): returns the summary ("+20 PV, -10 oro")
Variant AplicarEfectosA(Caller* instance, String heroe, Array efectos) {
	Node* h = Heroe(heroe);
	if (!h) return String("");
	String texto = inventario::AplicarEfectos(efectos, h);
	TrasCambiarEquipo(h);
	return texto;
}

// The inventory as a list of dictionaries, for the backpack screen
Variant DescribirInventario(Caller* instance, String heroe) {
	Node* h = Heroe(heroe);
	return h ? inventario::DescribirInventario(h) : Array();
}

Variant PesoInventario(Caller* instance, String heroe) {
	Node* h = Heroe(heroe);
	return h ? inventario::PesoInventario(h) : 0.0f;
}

Variant CapacidadInventario(Caller* instance, String heroe) {
	Node* h = Heroe(heroe);
	return h ? inventario::Capacidad(h) : 0.0f;
}

// When going down a floor the "until the end of the floor" effects end
void FinDePiso(Caller* instance) {
	for (Node* h : Heroes()) inventario::FinDePiso(h);
}

// ==========================================
// DEBUGGING (F10 panel)
// ==========================================

// Drops units of an item (they're lost)
Variant QuitarObjeto(Caller* instance, String heroe, int indice, int cantidad) {
	Node* h = Heroe(heroe);
	bool ok = h && inventario::Quitar(h, indice, cantidad);
	if (ok && m_ui_manager) m_ui_manager->call("UpdateWarriorUI");
	return ok;
}

// Rolls a loot table and gives it to the hero. Returns what came out (and what didn't fit).
Variant DarBotinDebug(Caller* instance, String heroe, String tabla) {
	Node* h = Heroe(heroe);
	Node* cat = inventario::Catalogo(self);
	if (!h || !cat) return String(String::utf8("No existe ese h\xC3\xA9roe"));
	Array botin = cat->call("tirar_botin", tabla, (int)self->get_meta("piso_actual", 1));
	Vector3 pos(0, 0, 0);
	if (m_dungeon_manager && h->has_meta("grid_pos")) {
		Vector2i g = h->get_meta("grid_pos");
		pos = Vector3(g.x + 0.5f, 0.6f, g.y + 0.5f);
	}
	Array restos = inventario::EntregarBotin(h, botin, pos);
	String texto;
	for (int i = 0; i < botin.size(); i++) {
		Dictionary b = botin[i];
		int r = b["recurso"];
		String que = r == inventario::B_ORO ? String("oro") : (r == inventario::B_COMIDA ? String("comida") : String(b["id"]));
		texto += (texto.is_empty() ? String("") : String(", ")) + String::num_int64((int)b["cantidad"]) + " " + que;
	}
	if (texto.is_empty()) texto = "nada";
	if (!restos.is_empty()) texto += String(" (no cupo todo)");
	TrasCambiarEquipo(h);
	return texto;
}

// After editing a .tres with the game running (Catalogo.recargar): stats up to date
void RecalcularTodo(Caller* instance) {
	for (Node* h : Heroes()) inventario::RecalcularStats(h);
	TrasCambiarEquipo(nullptr);
}

// A day goes by only for the temporary effects (to test them without playing the turn)
void AvanzarDiaEfectos(Caller* instance, String heroe) {
	Node* h = Heroe(heroe);
	if (!h) return;
	inventario::AvanzarDia(h);
	TrasCambiarEquipo(h);
}

// Applies an item's use effects without spending it (to test consumables and events)
Variant ProbarEfectos(Caller* instance, String heroe, String id) {
	Node* h = Heroe(heroe);
	Ref<Resource> def = inventario::Definicion(self, id);
	if (!h || def.is_null()) return String("No existe");
	String texto = inventario::AplicarEfectos(def->get("efectos_al_usar"), h);
	TrasCambiarEquipo(h);
	return texto.is_empty() ? String("sin efecto") : texto;
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

		Node* soul = memnew(Node);
		soul->set_name(name);
		soul->set_meta("nombre_display", name);
		soul->set_meta("retrato_path", res->get("retrato_path"));
		soul->set_meta("ha_movido", false);
		soul->set_meta("esta_muerto", false);
		soul->set_meta("ha_escapado", false);
		soul->set_meta("in_combat", false);
		soul->set_meta("oro", 0);
		soul->set_meta("comida_bolsa", 0);
		self->add_child(soul); // Before equipping it: the inventory looks for the Catalogo from the tree

		// Base stats + starting equipment -> final stats (hp_max, fuerza, radio_luz...)
		inventario::Inicializar(soul, res);
		soul->set_meta("hp_actual", soul->get_meta("hp_max"));
		soul->set_meta("sanidad_actual", soul->get_meta("sanidad_max"));
		soul->set_meta("hambre_actual", soul->get_meta("hambre_max"));
		soul->set_meta("pa_actual", soul->get_meta("pa_max"));

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
