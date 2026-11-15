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

	Node* Heroe(const String& nombre) {
		if (!self || nombre.is_empty() || nombre == "DatosEquipo") return nullptr;
		return self->get_node_or_null(NodePath(nombre));
	}

	bool EnFogata() {
		CanvasLayer* fogata = self ? Object::cast_to<CanvasLayer>(self->get_parent()->get_node_or_null("CanvasLayer3")) : nullptr;
		return fogata && fogata->is_visible();
	}

	bool EnMapa(Node* h) { return h && !MetaBool(h, "esta_muerto") && !MetaBool(h, "ha_escapado") && h->has_meta("grid_pos"); }

	bool PuedenIntercambiarObjetos(Node* a, Node* b) {
		if (!a || !b || a == b || MetaBool(a, "esta_muerto") || MetaBool(b, "esta_muerto")) return false;
		if (EnFogata()) return true;
		if (!EnMapa(a) || !EnMapa(b)) return false;
		Vector2i pa = a->get_meta("grid_pos"), pb = b->get_meta("grid_pos");
		return std::abs(pa.x - pb.x) + std::abs(pa.y - pb.y) == 1;
	}

	// Cambiar el equipo puede cambiar la luz (radio_luz), los PA o la vida: refrescamos todo
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

	// CAE LA NOCHE: supervivencia y recarga de PA
	void Noche() {
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

			inventario::AvanzarDia(h); // los efectos temporales duran X días
			h->set_meta("ha_movido", false);
			h->set_meta("pa_actual", h->get_meta("pa_max", 3));
		}

		UtilityFunctions::print("[PartyManager] New day. Current turn: ", turno);
		if (m_ui_manager) {
			m_ui_manager->call("ActualizarMarcadorTurno");
			m_ui_manager->call("ActualizarStatsUI");
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

	// 2. Si se acaba el día, antes de la noche juegan los enemigos despiertos;
	//    al terminar, el EnemyManager llama a IniciarNuevoDia
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

	// 3. SIGUIENTE PERSONAJE DISPONIBLE
	SeleccionarSiguiente();

	if (m_ui_manager) m_ui_manager->call("UpdateWarriorUI");
	if (m_dungeon_manager) m_dungeon_manager->call("RefrescarLuces");
}

// Explorando (Modo.gd), el día pasa por casillas andadas: hambre, cordura, efectos y PA como cada
// noche, pero sin turno enemigo y sin cambiar de héroe activo.
void PasarDiaExplorando(Caller* instance) {
	if (!self) return;
	Noche();
	if (m_ui_manager) m_ui_manager->call("UpdateWarriorUI");
	if (m_dungeon_manager) m_dungeon_manager->call("RefrescarLuces");
}

// Lo llama el EnemyManager al terminar el turno enemigo
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
// INVENTARIO Y EQUIPO (la lógica está en Inventario/Inventario.hpp)
// ==========================================
// Equipar, usar e intercambiar no cuestan PA. Las funciones devuelven "" si todo fue bien
// o el motivo del fallo (para la interfaz).

Variant Equipar(Caller* instance, String heroe, int indice) {
	Node* h = Heroe(heroe);
	if (!h) return String::utf8("No existe ese h\xC3\xA9roe");
	String error = inventario::Equipar(h, indice);
	if (error.is_empty()) TrasCambiarEquipo(h);
	return error;
}

Variant Desequipar(Caller* instance, String heroe, String ranura) {
	Node* h = Heroe(heroe);
	if (!h) return String::utf8("No existe ese h\xC3\xA9roe");
	String error = inventario::Desequipar(h, ranura);
	if (error.is_empty()) TrasCambiarEquipo(h);
	return error;
}

// Devuelve cuántas unidades han entrado
Variant AnadirObjeto(Caller* instance, String heroe, String id, int cantidad) {
	Node* h = Heroe(heroe);
	if (!h) return 0;
	int n = inventario::Anadir(h, id, cantidad);
	if (m_ui_manager) m_ui_manager->call("UpdateWarriorUI");
	return n;
}

Variant UsarObjeto(Caller* instance, String heroe, int indice) {
	Node* h = Heroe(heroe);
	if (!h || MetaBool(h, "esta_muerto")) return String("No puede usarlo");
	String resultado;
	// Consumibles con efectos raros (poción "¿qué podría malir sal?", botón de pánico): Especiales.gd
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

// En la fogata se puede dar cualquier cosa; en la mazmorra, solo entre héroes pegados
Variant PuedenIntercambiar(Caller* instance, String a, String b) {
	return PuedenIntercambiarObjetos(Heroe(a), Heroe(b));
}

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

// Aplica una lista de Efecto.tres a un héroe (eventos): devuelve el resumen ("+20 PV, -10 oro")
Variant AplicarEfectosA(Caller* instance, String heroe, Array efectos) {
	Node* h = Heroe(heroe);
	if (!h) return String("");
	String texto = inventario::AplicarEfectos(efectos, h);
	TrasCambiarEquipo(h);
	return texto;
}

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

// Al bajar de piso se acaban los efectos "hasta el final del piso"
void FinDePiso(Caller* instance) {
	for (Node* h : Heroes()) inventario::FinDePiso(h);
}

// ==========================================
// DEPURACIÓN (panel F10)
// ==========================================

Variant QuitarObjeto(Caller* instance, String heroe, int indice, int cantidad) {
	Node* h = Heroe(heroe);
	bool ok = h && inventario::Quitar(h, indice, cantidad);
	if (ok && m_ui_manager) m_ui_manager->call("UpdateWarriorUI");
	return ok;
}

// Tira una tabla de botín y se la da al héroe. Devuelve lo que salió (y lo que no cupo).
Variant DarBotinDebug(Caller* instance, String heroe, String tabla) {
	Node* h = Heroe(heroe);
	Node* cat = inventario::Catalogo(self);
	if (!h || !cat) return String("No existe ese héroe");
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

// Tras editar un .tres con el juego en marcha (Catalogo.recargar): stats al día
void RecalcularTodo(Caller* instance) {
	for (Node* h : Heroes()) inventario::RecalcularStats(h);
	TrasCambiarEquipo(nullptr);
}

// Pasa un día solo para los efectos temporales (para probarlos sin jugar el turno)
void AvanzarDiaEfectos(Caller* instance, String heroe) {
	Node* h = Heroe(heroe);
	if (!h) return;
	inventario::AvanzarDia(h);
	TrasCambiarEquipo(h);
}

// Aplica los efectos al usar de un objeto sin gastarlo (probar consumibles y eventos)
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
		self->add_child(soul); // antes de equiparlo: el inventario busca el Catalogo desde el árbol

		// Stats base + equipo inicial -> stats finales (hp_max, fuerza, radio_luz...)
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
