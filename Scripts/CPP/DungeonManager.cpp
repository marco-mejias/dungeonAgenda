/* Jenova C++ Node Base Script (Meteora) - DungeonManager (3D) */

// The brain: keeps mapa.map_data (the logic grid), generates the dungeon, processes clicks and
// coordinates the rest of the managers. It doesn't draw anything: that's the DungeonRenderer's job.

#include <Godot/godot.hpp>
#include <Godot/classes/node3d.hpp>
#include <Godot/classes/sprite3d.hpp>
#include <Godot/classes/control.hpp>
#include <Godot/classes/canvas_layer.hpp>
#include <Godot/classes/packed_scene.hpp>
#include <Godot/classes/resource_loader.hpp>
#include <Godot/classes/resource.hpp>
#include <Godot/classes/dir_access.hpp>
#include <Godot/classes/tween.hpp>
#include <Godot/classes/property_tweener.hpp>
#include <Godot/classes/callback_tweener.hpp>
#include <Godot/classes/interval_tweener.hpp>
#include <Godot/variant/utility_functions.hpp>
#include "Scripts/CPP/Mazmorra/Mazmorra.hpp"
#include "Scripts/CPP/Combate/Reglas.hpp"
#include "Scripts/CPP/IA/Enemigos.hpp"
#include <algorithm>
#include <vector>
#include <queue>
#include <cmath>

using namespace godot;
using namespace jenova::sdk;
using namespace mazmorra;

namespace {
	Node3D* self = nullptr;
	Mazmorra mapa; // The logic grid of the current floor


	Node* m_party_manager = nullptr;
	Node* m_ui_manager = nullptr;
	Node* m_renderer = nullptr;
	Node* m_path_manager = nullptr;
	Node* m_event_manager = nullptr;

	bool debug_mode = false;

	// Exploring (Modo.gd, no enemies awake): walking costs no PA and there are no turns.
	bool Explorando() { return self && self->has_meta("modo_exploracion") && (bool)self->get_meta("modo_exploracion"); }
	// No PA cost: debug mode or exploring
	bool SinCostes() { return debug_mode || Explorando(); }


	// ==========================================
	// HELPERS (not exposed to Godot)
	// ==========================================

	// The PartyManager children that are heroes (skips the DatosEquipo node)
	std::vector<Node*> Heroes() {
		std::vector<Node*> out;
		if (!m_party_manager) return out;
		TypedArray<Node> hijos = m_party_manager->get_children();
		for (int i = 0; i < hijos.size(); i++) {
			Node* h = Object::cast_to<Node>(hijos[i]);
			if (h && h->get_name() != StringName("DatosEquipo")) out.push_back(h);
		}
		return out;
	}

	// Hidden (Sigilo.gd): walking costs 2 PA per tile (path, reach and cost all count it)
	int PaPorCasilla(Node* act) {
		if (!act || SinCostes() || !act->has_meta("estados")) return 1;
		Dictionary est = act->get_meta("estados");
		return est.has("oculto") ? 2 : 1;
	}

	bool MetaBool(Node* n, const char* key) { return n && n->has_meta(key) && (bool)n->get_meta(key); }

	bool HeroeEnMapa(Node* h) { return h && !MetaBool(h, "esta_muerto") && !MetaBool(h, "ha_escapado") && h->has_meta("grid_pos"); }

	// The hero whose turn it is
	Node* ActiveChar() {
		if (!m_party_manager) return nullptr;
		return Object::cast_to<Node>(m_party_manager->call("GetActiveCharacter"));
	}

	// Grid tile of the active hero
	Vector2i ActivePos() {
		if (!m_party_manager) return Vector2i(-1, -1);
		return m_party_manager->call("GetActiveGridPos");
	}

	// The 3D token of a hero (same name as their PartyManager node)
	Node3D* Token(Node* hero) {
		if (!self || !hero) return nullptr;
		return Object::cast_to<Node3D>(self->get_node_or_null(NodePath(String(hero->get_name()))));
	}

	String NombreEnemigo(Vector2i p) { return "Enemy_" + String::num_int64(p.x) + "_" + String::num_int64(p.y); }

	// Is there a hero (other than 'excepto') standing on 'p'?
	bool HayHeroeEn(Vector2i p, Node* excepto) {
		for (Node* h : Heroes()) {
			if (h == excepto || !HeroeEnMapa(h)) continue;
			if ((Vector2i)h->get_meta("grid_pos") == p) return true;
		}
		return false;
	}

	// The board doesn't take clicks while an event, a movement or a banner is running
	bool Bloqueado() {
		return self && ((self->has_meta("bloqueo_evento") && (bool)self->get_meta("bloqueo_evento")) ||
						(self->has_meta("is_moving") && (bool)self->get_meta("is_moving")) ||
						// The turn/day change message (banner_turno.gd) is still being read
						(self->has_meta("bloqueo_banner") && (bool)self->get_meta("bloqueo_banner")));
	}

	// Vector of tiles -> flat x,y array (the format GDScript reads fastest)
	PackedInt32Array AParesInt(const std::vector<Vector2i>& celdas) {
		PackedInt32Array out;
		for (const Vector2i& c : celdas) { out.push_back(c.x); out.push_back(c.y); }
		return out;
	}

	// Refreshes the hero cards
	void UpdateUI() {
		if (m_ui_manager) m_ui_manager->call("UpdateWarriorUI");
	}


	// ==========================================
	// VISION AND FOG OF WAR
	// ==========================================

	// Recomputes what the party sees and sends the fog to the renderer
	void RefrescarVision() {
		if (!self || mapa.vacia()) return;

		// Every hero on the map is a light source
		std::vector<Foco> focos;
		for (Node* h : Heroes()) {
			if (!HeroeEnMapa(h)) continue;
			focos.push_back({ (Vector2i)h->get_meta("grid_pos"), RadioVision(h) });
		}
		mapa.CalcularVision(focos);
		if (m_renderer) m_renderer->call("ActualizarNiebla", mapa.EstadosNiebla(debug_mode), debug_mode ? 1 : 0);

		// Enemies are only visible if their tile is in view
		for (int x = 0; x < GRID_SIZE; x++)
			for (int y = 0; y < GRID_SIZE; y++) {
				if (!mapa.map_data[x][y].has_enemy) continue;
				Node3D* e = Object::cast_to<Node3D>(self->get_node_or_null(NodePath(NombreEnemigo(Vector2i(x, y)))));
				if (e) e->set_visible(debug_mode || mapa.map_data[x][y].is_visible);
			}
	}

	// Clears the reach overlay (the logic flags and the drawing)
	void LimpiarAlcance() {
		for (int x = 0; x < GRID_SIZE && !mapa.map_data.empty(); x++)
			for (int y = 0; y < GRID_SIZE; y++)
				mapa.map_data[x][y].is_highlighted = false;
		if (m_renderer) m_renderer->call("LimpiarOverlay");
	}

	const int COSTE_INTERACCION = 2; // Same as EventManager (attacking costs whatever the weapon says)

	// Can the hero attack the enemy at 'p' from where they are? (weapon reach + line of sight)
	bool PuedeAtacar(Node* act, Vector2i p) {
		if (!act || !Mazmorra::InBounds(p.x, p.y) || !mapa.map_data[p.x][p.y].has_enemy) return false;
		Vector2i origen = act->get_meta("grid_pos", Vector2i(-99, -99));
		if (!reglas::EnAlcance(act, origen, p)) return false;
		return !reglas::RequiereVision(act) || mapa.CheckLineOfSight(origen, p);
	}

	// Is there something to interact with on this tile? Fills in the action and its PA cost.
	// tipo_overlay: 1 = interactable, 2 = enemy
	bool DescribirInteraccion(Vector2i p, Node* act, String& accion, int& coste, int& tipo_overlay) {
		if (!Mazmorra::InBounds(p.x, p.y)) return false;
		const Cell& c = mapa.map_data[p.x][p.y];
		coste = SinCostes() ? 0 : COSTE_INTERACCION;
		tipo_overlay = 1;
		if (c.has_enemy) {
			if (act && !debug_mode) coste = reglas::CosteAtaque(act);
			// No numbers: what the hit does is shown by the preview card
			accion = "Atacar";
			tipo_overlay = 2;
			return true;
		}
		if (p == mapa.m_hoguera) { accion = String::utf8("Descansar en la hoguera"); coste = 0; return true; }
		if (c.is_chest && !c.chest_opened) { accion = "Abrir cofre"; return true; }
		if (c.is_door && !c.door_open && c.is_locked) {
			accion = MetaBool(self, "llave_jefe") ? String("Abrir con la llave") : String("Cerrada con llave");
			return true;
		}
		if (c.is_door && !c.door_open) { accion = "Abrir puerta"; return true; }
		if (mapa.EventoManualActivo(c)) { accion = "Investigar"; return true; }
		if (p == mapa.m_salida) { accion = "Bajar la escalera"; coste = 0; return true; }
		return false;
	}

	// Bubble text: the action and, with turns, its PA cost
	String TextoBocadillo(const String& accion, int coste) {
		if (Explorando()) return accion; // Without turns there's no PA to count
		return accion + String("\n") + String::num_int64(coste) + String(" PA");
	}

	// Hides every bubble and the cost label
	void LimpiarBocadillos() {
		if (!m_renderer) return;
		m_renderer->call("FijarBocadillos", 0, PackedInt32Array(), PackedStringArray(), PackedInt32Array());
		m_renderer->call("FijarBocadillos", 1, PackedInt32Array(), PackedStringArray(), PackedInt32Array());
		m_renderer->call("MostrarCoste", -1, -1, String(""), 0);
	}

	// Bubbles over the interactables next to the active hero
	void ActualizarBocadillos(Node* act, Vector2i pos) {
		if (!m_renderer) return;
		PackedInt32Array celdas, estilos;
		PackedStringArray textos;
		int pa = act ? (int)act->get_meta("pa_actual", 0) : 0;
		for (const Vector2i& d : DIRS4) {
			Vector2i n = pos + d;
			String accion; int coste = 0, tipo = 0;
			if (!Mazmorra::InBounds(n.x, n.y) || !(mapa.map_data[n.x][n.y].is_visible || debug_mode)) continue;
			if (!DescribirInteraccion(n, act, accion, coste, tipo)) continue;
			if (tipo == 2 && !PuedeAtacar(act, n)) continue; // E.g. a bow can't shoot point-blank
			if (n == mapa.m_salida) coste = 1; // One step
			if (SinCostes()) coste = 0;
			celdas.push_back(n.x); celdas.push_back(n.y);
			textos.push_back(TextoBocadillo(accion, coste));
			estilos.push_back(pa < coste ? 1 : 0);
		}
		m_renderer->call("FijarBocadillos", 0, celdas, textos, estilos);
	}

	// Flood fill with PA and LoS. Walls, closed doors, chests, enemies and other heroes act as real
	// walls: they are marked visited but NOT lit nor crossed.
	void HighlightReachableCells(Vector2i inicio, int pa) {
		LimpiarAlcance();
		if (!Mazmorra::InBounds(inicio.x, inicio.y)) return;
		Node* act = ActiveChar();

		std::vector<std::vector<bool>> visitado(GRID_SIZE, std::vector<bool>(GRID_SIZE, false));
		std::queue<std::pair<Vector2i, int>> cola;
		std::vector<Vector2i> iluminadas;

		cola.push({ inicio, 0 });
		visitado[inicio.x][inicio.y] = true;
		mapa.map_data[inicio.x][inicio.y].is_highlighted = true;

		while (!cola.empty()) {
			Vector2i pos = cola.front().first;
			int coste = cola.front().second;
			cola.pop();
			if (coste >= pa) continue;

			for (const Vector2i& d : DIRS4) {
				Vector2i n = pos + d;
				if (!Mazmorra::InBounds(n.x, n.y) || visitado[n.x][n.y]) continue;
				visitado[n.x][n.y] = true;

				if (!mapa.EsSueloLibre(n.x, n.y)) continue;
				// While exploring you can go anywhere already explored; in combat, only to what is
				// in view
				if (Explorando() ? !mapa.map_data[n.x][n.y].is_explored : !mapa.CheckLineOfSight(inicio, n)) continue;

				// You can walk through a teammate, but not end on their tile
				if (!HayHeroeEn(n, act)) {
					mapa.map_data[n.x][n.y].is_highlighted = true;
					iluminadas.push_back(n);
				}
				cola.push({ n, coste + 1 });
			}
		}

		// Triplets (x, y, type) for the renderer: the walk area (with the hero) and, around it, the
		// reachable interactables (gold) and enemies (red)
		PackedInt32Array ternas;
		std::vector<std::vector<bool>> marcado(GRID_SIZE, std::vector<bool>(GRID_SIZE, false));
		iluminadas.push_back(inicio);
		for (const Vector2i& c : iluminadas) {
			// While exploring the walk area isn't drawn (you can go anywhere explored)
			if (!Explorando()) { ternas.push_back(c.x); ternas.push_back(c.y); ternas.push_back(0); }
			marcado[c.x][c.y] = true;
		}
		for (const Vector2i& c : iluminadas) {
			for (const Vector2i& d : DIRS4) {
				Vector2i n = c + d;
				if (!Mazmorra::InBounds(n.x, n.y) || marcado[n.x][n.y] || !(mapa.map_data[n.x][n.y].is_visible || debug_mode)) continue;
				String accion; int coste = 0, tipo = 0;
				if (n == mapa.m_salida || !DescribirInteraccion(n, act, accion, coste, tipo)) continue;
				marcado[n.x][n.y] = true;
				ternas.push_back(n.x); ternas.push_back(n.y); ternas.push_back(tipo);
			}
		}
		if (m_renderer) m_renderer->call("PintarAlcance", ternas);
	}

	// Recomputes vision, the walk area and the bubbles of the active hero
	void UpdateHighlight() {
		if (!self || mapa.map_data.empty()) return;
		RefrescarVision();

		Node* act = ActiveChar();
		if (!act || Bloqueado() || !HeroeEnMapa(act)) {
			LimpiarAlcance();
			LimpiarBocadillos();
			return;
		}
		int pa = act->has_meta("pa_actual") ? (int)act->get_meta("pa_actual") : 0;
		if (debug_mode) pa = std::max(pa, 8);
		if (Explorando()) pa = GRID_SIZE * GRID_SIZE; // No limit
		HighlightReachableCells(ActivePos(), pa / PaPorCasilla(act));
		ActualizarBocadillos(act, ActivePos());

		// The hover path/cost was computed from the previous position: recompute it
		Node* input = self->get_node_or_null("InputManager");
		if (input) input->call("RefrescarHover");
	}

	// ==========================================
	// MOVEMENT: END AND INTERRUPTIONS
	// ==========================================

	// Spends PA from the hero (nothing while exploring or in debug)
	void RestarPA(Node* act, int cantidad) {
		if (!act || SinCostes() || cantidad <= 0) return;
		int pa = act->has_meta("pa_actual") ? (int)act->get_meta("pa_actual") : 0;
		act->set_meta("pa_actual", std::max(0, pa - cantidad));
	}

	// Switching to the next hero is the LAST thing of the turn: it is requested here and done once
	// nothing is still running (door turning, loot flying, event open, attack, enemy turn).
	const float ESPERA_FIN_TURNO = 0.85f; // How long the door (0.55) and the loot (0.77) take

	// Out of PA or off the map: their turn is over
	bool HeroeSinTurno(Node* act) {
		return act && ((int)act->get_meta("pa_actual", 0) <= 0 || !HeroeEnMapa(act));
	}

	// Asks for the end of turn in 'espera' seconds (IntentarFinDeTurno waits if something is running)
	void ProgramarFinDeTurno(float espera) {
		self->set_meta("fin_turno_pendiente", true);
		Ref<Tween> tw = self->create_tween();
		tw->tween_interval(espera);
		tw->tween_callback(Callable(self, StringName("IntentarFinDeTurno")));
	}

	// If the active hero can't do anything else, schedule the end of their turn
	void ComprobarFinDeTurno() {
		Node* act = ActiveChar();
		if (!act || !m_party_manager || MetaBool(self, "fin_turno_pendiente")) return;
		if (HeroeSinTurno(act)) ProgramarFinDeTurno(ESPERA_FIN_TURNO);
	}

	// Emergency brake: kills the tween, puts the token on its tile and charges the steps taken
	void DetenerMovimiento(Vector2i pos, int pasos) {
		Ref<Tween> tw = self->get_meta("current_move_tween", Variant());
		if (tw.is_valid()) tw->kill();
		self->set_meta("is_moving", false);
		if (self->has_meta("interaccion_pendiente")) self->remove_meta("interaccion_pendiente");

		Node* act = ActiveChar();
		Node3D* token = Token(act);
		if (token) {
			token->set_position(GridToWorld(pos));
			Node3D* cuerpo = Object::cast_to<Node3D>(token->get_node_or_null("Cuerpo"));
			if (cuerpo) cuerpo->set_position(Vector3(0, (float)cuerpo->get_meta("y_base", 0.44), 0));
		}
		if (m_party_manager) m_party_manager->call("SetActiveGridPos", pos.x, pos.y);
		RestarPA(act, pasos);
	}

	// Returns the index of the automatic event that triggers on this tile, or -1
	int BuscarEventoAutomatico(Node* act, Vector2i pos) {
		for (size_t i = 0; i < mapa.m_eventos.size(); i++) {
			const EventInfo& e = mapa.m_eventos[i];
			if (!e.activo) continue;
			int dist = std::abs(pos.x - e.pos.x) + std::abs(pos.y - e.pos.y);

			if (e.gatillo == 1) { // PROXIMITY: triggers when seen inside its radius
				if (dist <= e.radio && mapa.CheckLineOfSight(pos, e.pos)) return (int)i;
			} else if (e.gatillo == 2 && dist == 0) { // CONDITIONAL: only when stepped on
				if (act && act->has_meta(e.stat) && (int)act->get_meta(e.stat) < e.critico) return (int)i;
			}
		}
		return -1;
	}

	// Does any sleeping enemy see any hero? One symmetric rule: the enemy has to be in the party's
	// view (its tile visible) and, from any hero on the map (the leader or the ones in its line),
	// inside its detection radius measured as a circle and with the same line of sight as the
	// shadows. It also works mid-combat (it wakes up another squad).
	bool ComprobarAggro(Node* act, Vector2i pos) {
		if (!act) return false;
		// With Sigilo.gd: vision cones (where each enemy is looking). 1 = stealth starts (stop so
		// the player can decide), 2 = they've been seen (combat)
		Node* sigilo = self->get_node_or_null("/root/Sigilo");
		if (sigilo) {
			int r = (int)sigilo->call("comprobar", act, pos.x, pos.y);
			if (r == 2) {
				act->set_meta("in_combat", true);
				UtilityFunctions::print("[Combate] ", act->get_name(), " ha sido visto: combate");
			}
			return r != 0;
		}
		std::vector<Vector2i> ojos;
		for (Node* h : Heroes())
			if (HeroeEnMapa(h)) ojos.push_back(h == act ? pos : (Vector2i)h->get_meta("grid_pos"));

		bool detectado = false;
		for (int x = 0; x < GRID_SIZE; x++)
			for (int y = 0; y < GRID_SIZE; y++) {
				if (!mapa.map_data[x][y].has_enemy || !mapa.map_data[x][y].is_visible) continue;
				Node* enemigo = self->get_node_or_null(NodePath(NombreEnemigo(Vector2i(x, y))));
				if (!enemigo || MetaBool(enemigo, "in_combat")) continue;
				float radio = (int)enemigo->get_meta("radio_deteccion", 6) + 0.5f;
				for (const Vector2i& o : ojos) {
					if (mapa.VeCasilla(Vector2i(x, y), radio, o)) {
						enemigo->set_meta("in_combat", true);
						detectado = true;
						break;
					}
				}
			}

		if (detectado) {
			act->set_meta("in_combat", true);
			UtilityFunctions::print(String::utf8("[Combate] \xC2\xA1"), act->get_name(), " HA ENTRADO EN COMBATE!");
			Node* audio = self->get_node_or_null("/root/AudioManager");
			if (audio) audio->call("play_sfx", "fail");
		}
		return detectado;
	}

	// Places the heroes on the spawn tiles of the new floor (the dead ones stay hidden)
	void ColocarHeroesEnSpawn() {
		int idx = 0;
		for (Node* h : Heroes()) {
			Node3D* token = Token(h);
			if (MetaBool(h, "esta_muerto")) {
				if (token) token->set_visible(false);
				continue;
			}
			Vector2i celda = mapa.m_spawn_cells.empty() ? mapa.m_inicio : mapa.m_spawn_cells[std::min(idx, (int)mapa.m_spawn_cells.size() - 1)];
			h->set_meta("grid_pos", celda);
			h->set_meta("in_combat", false);
			if (token) {
				token->set_position(GridToWorld(celda));
				token->set_visible(true);
			}
			idx++;
		}
	}
}

// Creates the token of the enemy on that tile. The ones in a camp or in the boss room come out
// already in their squad (fixed id per room); the one on the boss tile becomes the boss (Zonas.gd)
static Node3D* CrearFichaEnemigo(Vector2i celda, int sala) {
	Ref<PackedScene> escena = ResourceLoader::get_singleton()->load("res://Scenes/Enemy.tscn"); // The loader cache keeps it
	if (escena.is_null()) {
		UtilityFunctions::printerr("[DungeonManager] ERROR: no se pudo cargar res://Scenes/Enemy.tscn");
		return nullptr;
	}
	int piso = (m_party_manager && m_party_manager->has_meta("piso_actual")) ? (int)m_party_manager->get_meta("piso_actual") : 1;
	Node* catalogo = inventario::Catalogo(self);
	Node3D* enemigo = Object::cast_to<Node3D>(escena->instantiate());
	if (!enemigo) return nullptr;
	enemigo->set_name(NombreEnemigo(celda));
	enemigo->set_position(GridToWorld(celda));
	enemigo->set_visible(false);
	Ref<Resource> tipo = catalogo ? Ref<Resource>(catalogo->call("elegir_enemigo", piso)) : Ref<Resource>();
	enemigos::Configurar(enemigo, tipo, piso, celda);
	if (sala >= 0) {
		enemigo->set_meta("sala", sala);
		enemigo->set_meta("peloton", 100000 + piso * 100 + sala);
	}
	self->add_child(enemigo);
	Node* zonas = self->get_node_or_null("/root/Zonas");
	if (zonas && celda == mapa.m_jefe) zonas->call("hacer_jefe", enemigo);
	return enemigo;
}

JENOVA_SCRIPT_BEGIN

// ==========================================
// FLOOR CONSTRUCTION
// ==========================================

// Generates the floor logic: map, zones, AStar, events and enemies
void ConstruirLogica(Caller* instance) {
	mapa.m_piso = m_party_manager ? (int)m_party_manager->get_meta("piso_actual", 1) : 1;
	mapa.GenerarPiso();
	self->set_meta("botin_cofres", Dictionary()); // Pending loot from the previous floor's chests

	// ASTAR STATIC LOAD: walls and closed doors, ONLY once
	std::vector<Vector2i> solidos;
	for (int x = 0; x < GRID_SIZE; x++)
		for (int y = 0; y < GRID_SIZE; y++)
			if (mapa.BloqueaVision(x, y) || mapa.map_data[x][y].is_brasero) solidos.push_back(Vector2i(x, y));

	if (m_path_manager) {
		m_path_manager->call("ConfigurarAStar", GRID_SIZE, 1.0f);
		m_path_manager->call("CargarMurosEstaticos", AParesInt(solidos));
	}

	Array eventos, cofres;
	for (const EventInfo& e : mapa.m_eventos) eventos.push_back(e.pos);
	for (int x = 0; x < GRID_SIZE; x++)
		for (int y = 0; y < GRID_SIZE; y++)
			if (mapa.map_data[x][y].is_chest) cofres.push_back(Vector2i(x, y));

	self->set_meta("casilla_inicio", mapa.m_inicio);
	self->set_meta("casilla_salida", mapa.m_salida);
	self->set_meta("lista_eventos", eventos);
	self->set_meta("lista_cofres", cofres);
	self->set_meta("lista_enemigos", mapa.ListaEnemigos());

	Array puertas, braseros;
	for (int x = 0; x < GRID_SIZE; x++)
		for (int y = 0; y < GRID_SIZE; y++)
			if (mapa.map_data[x][y].is_door && !mapa.map_data[x][y].door_open) puertas.push_back(Vector2i(x, y));
	for (const Vector2i& b : mapa.m_braseros) braseros.push_back(b);
	self->set_meta("lista_puertas", puertas);
	self->set_meta("lista_braseros", braseros);

	// Floor structure (used by Zonas.gd): campfire, boss, key, camps
	Array puertas_jefe;
	for (const Vector2i& p : mapa.m_puertas_jefe) puertas_jefe.push_back(p);
	self->set_meta("zonas", mapa.ListaZonas());
	self->set_meta("casilla_hoguera", mapa.m_hoguera);
	self->set_meta("casilla_jefe", mapa.m_jefe);
	self->set_meta("cofre_llave", mapa.m_cofre_llave);
	self->set_meta("cofre_jefe", mapa.m_cofre_jefe);
	self->set_meta("puertas_jefe", puertas_jefe);
	self->set_meta("llave_jefe", false);
	self->set_meta("semilla_piso", (int64_t)(UtilityFunctions::randi() % 1000000) + 1);
	self->set_meta("version_mapa", (int)self->get_meta("version_mapa", 0) + 1); // ObtenerMapaFijo

	UtilityFunctions::print(String::utf8("[DungeonManager] L\xC3\xB3gica lista. Eventos: "), (int)mapa.m_eventos.size(), " | Cofres: ", cofres.size(), " | Enemigos: ", mapa.ListaEnemigos().size());
}

// Builds the 3D world from the logic grid (floors, walls, props, fog)
void ConstruirVisual(Caller* instance) {
	if (!m_renderer) {
		UtilityFunctions::printerr("[DungeonManager] ERROR: no encuentro el DungeonRenderer");
		return;
	}

	PackedByteArray tipos;
	tipos.resize(GRID_SIZE * GRID_SIZE);
	for (int x = 0; x < GRID_SIZE; x++)
		for (int y = 0; y < GRID_SIZE; y++) {
			const Cell& c = mapa.map_data[x][y];
			tipos[x * GRID_SIZE + y] = c.is_pillar ? 4 : (c.is_wall ? 0 : (c.is_door ? (c.door_open ? 3 : 2) : 1));
		}
	m_renderer->call("ConstruirMundo", tipos, GRID_SIZE);

	for (int x = 0; x < GRID_SIZE; x++)
		for (int y = 0; y < GRID_SIZE; y++)
			if (mapa.map_data[x][y].is_chest) m_renderer->call("CrearCofre", x, y);

	m_renderer->call("CrearSalida", mapa.m_salida.x, mapa.m_salida.y);
	for (const Vector2i& b : mapa.m_braseros) m_renderer->call("CrearBrasero", b.x, b.y);
	m_renderer->call("CrearDecoracion", mapa.GenerarDecoracion());

	// Manual and proximity events are visible on the map (their sprite); traps are hidden
	for (const EventInfo& e : mapa.m_eventos) {
		if (e.gatillo != 2) m_renderer->call("CrearSpriteEvento", e.pos.x, e.pos.y, e.path);
		else if (debug_mode) m_renderer->call("CrearMarcadorEvento", e.pos.x, e.pos.y);
	}

	// ENEMIES: the Catalogo picks the type (Data/Enemigos) depending on the floor
	for (int x = 0; x < GRID_SIZE; x++)
		for (int y = 0; y < GRID_SIZE; y++)
			if (mapa.map_data[x][y].has_enemy) CrearFichaEnemigo(Vector2i(x, y), mapa.map_data[x][y].peloton);

	RefrescarVision();
}

// A new enemy on a free tile (camp reinforcements). 'sala' = the room of the squad it joins (-1 =
// loose). Returns its token or null.
Variant CrearEnemigoEn(Caller* instance, int x, int y, int sala) {
	if (!Mazmorra::InBounds(x, y) || !mapa.EsSueloLibre(x, y) || Vector2i(x, y) == mapa.m_salida || HayHeroeEn(Vector2i(x, y), nullptr)) return Variant();
	mapa.map_data[x][y].has_enemy = true;
	Node3D* e = CrearFichaEnemigo(Vector2i(x, y), sala);
	if (!e) { mapa.map_data[x][y].has_enemy = false; return Variant(); }
	self->set_meta("lista_enemigos", mapa.ListaEnemigos());
	RefrescarVision();
	return e;
}

// Creates the 3D token of a hero from their PersonajeData and puts it on a spawn tile
void SpawnVisualCharacter(Caller* instance, Resource* recurso_ficha, int index_posicion) {
	Ref<Resource> res = recurso_ficha;
	if (res.is_null() || !self) return;

	String nombre = res->get("nombre_display");
	String prefab_path = res->get("ruta_cuerpo");
	Ref<PackedScene> prefab = ResourceLoader::get_singleton()->load(prefab_path);
	if (prefab.is_null()) {
		UtilityFunctions::printerr("[DungeonManager] Escena visual no encontrada: ", prefab_path);
		return;
	}

	Node3D* token = Object::cast_to<Node3D>(prefab->instantiate());
	if (!token) return;
	token->set_name(nombre);

	Vector2i celda = mapa.m_spawn_cells.empty() ? mapa.m_inicio : mapa.m_spawn_cells[std::min(index_posicion, (int)mapa.m_spawn_cells.size() - 1)];
	token->set_position(GridToWorld(celda));

	Node3D* cuerpo = Object::cast_to<Node3D>(token->get_node_or_null("Cuerpo"));
	if (cuerpo) cuerpo->set_meta("y_base", cuerpo->get_position().y);

	self->add_child(token);

	if (m_party_manager) {
		Node* alma = m_party_manager->get_node_or_null(NodePath(nombre));
		if (alma) alma->set_meta("grid_pos", celda);
	}
	UtilityFunctions::print("[DungeonManager] ", nombre, " aparece en ", celda.x, ", ", celda.y);
}

// ==========================================
// VISION AND REACH (public API)
// ==========================================

void CalcularMapaDeLuces(Caller* instance) { RefrescarVision(); }

void ActualizarVisibilidadEntidades(Caller* instance) { RefrescarVision(); }

// Is the tile lit right now? (for scripts)
Variant IsCasillaIluminada(Caller* instance, int x, int y) {
	return Mazmorra::InBounds(x, y) && !mapa.map_data.empty() && mapa.map_data[x][y].is_lit;
}

void UpdateHighlightForWarrior(Caller* instance) { UpdateHighlight(); }

// HUD wheel: hovering ATTACK (type 0) shows in red how far the weapon reaches (with line of sight
// if ranged) and the enemies that can be hit; hovering ABILITY (type 1), in blue, where the chicken
// can appear. On exit, OcultarRangoAccion.
void MostrarRangoAccion(Caller* instance, int tipo) {
	Node* act = ActiveChar();
	if (!act || !HeroeEnMapa(act) || mapa.map_data.empty() || !m_renderer || Bloqueado()) return;
	Vector2i pos = ActivePos();
	PackedInt32Array ternas;
	auto meter = [&](int x, int y, int t) { ternas.push_back(x); ternas.push_back(y); ternas.push_back(t); };
	if (tipo == 0) {
		bool vision = reglas::RequiereVision(act);
		for (int x = 0; x < GRID_SIZE; x++) {
			for (int y = 0; y < GRID_SIZE; y++) {
				const Cell& c = mapa.map_data[x][y];
				Vector2i p(x, y);
				if (c.is_wall || p == pos || !(c.is_explored || debug_mode)) continue;
				if (!reglas::EnAlcance(act, pos, p)) continue;
				if (vision && !mapa.CheckLineOfSight(pos, p)) continue;
				bool objetivo = c.has_enemy && (c.is_visible || debug_mode);
				meter(x, y, objetivo ? 2 : 3);
			}
		}
	} else {
		const Vector2i vecinas[] = { {1, 0}, {0, 1}, {-1, 0}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1} };
		for (const Vector2i& d : vecinas) {
			Vector2i n = pos + d;
			if (Mazmorra::InBounds(n.x, n.y) && mapa.EsSueloLibre(n.x, n.y) && !HayHeroeEn(n, act)) meter(n.x, n.y, 4);
		}
	}
	m_renderer->call("PintarAlcance", ternas);
}

void OcultarRangoAccion(Caller* instance) { UpdateHighlight(); }

void RefrescarLuces(Caller* instance) { UpdateHighlight(); }

void ApagarLucesMapa(Caller* instance) { LimpiarAlcance(); }

void ApagarLucesRuta(Caller* instance) { LimpiarAlcance(); }

// Used by the PathManager to inject dynamic entities into the AStar (x,y pairs). Type of every tile
// (0 wall, 1 floor, 2/3 closed/open door, 4 column): to look at generated maps
PackedByteArray ObtenerTiposMapa(Caller* instance) {
	PackedByteArray tipos;
	tipos.resize(GRID_SIZE * GRID_SIZE);
	for (int x = 0; x < GRID_SIZE && !mapa.map_data.empty(); x++)
		for (int y = 0; y < GRID_SIZE; y++) {
			const Cell& c = mapa.map_data[x][y];
			uint8_t v = c.is_pillar ? 4 : (c.is_wall ? 0 : (c.is_door ? (c.door_open ? 3 : 2) : 1));
			if (v == 1 && c.nivel == alturas::ALTO) v = 5;
			if (v == 1 && c.nivel == alturas::ESCALON) v = 6;
			tipos[x * GRID_SIZE + y] = v;
		}
	return tipos;
}

// Dynamic obstacles for the AStar (x,y pairs): unexplored tiles, enemies, chests, braziers...
PackedInt32Array ObtenerObstaculosDinamicos(Caller* instance) {
	std::vector<Vector2i> obst;
	// Heroes only path through what they have explored: they don't know the way through the dark
	for (int x = 0; x < GRID_SIZE && !mapa.map_data.empty(); x++)
		for (int y = 0; y < GRID_SIZE; y++) {
			const Cell& c = mapa.map_data[x][y];
			if (c.has_enemy || c.is_chest || (!c.is_explored && !c.is_wall && !debug_mode)) obst.push_back(Vector2i(x, y));
		}
	// Heroes don't block: you can walk through a teammate (the PathManager avoids ending on top)
	return AParesInt(obst);
}

// Puts a hero (any of them, not just the active one) on a tile: pushes, jumps, swaps, the party
// following the leader... (Tactica/Modo). It doesn't check whether it's free: the caller decides
// that. refrescar = 0 doesn't recompute vision (it's expensive): for the middle steps of a walk.
void ColocarHeroe(Caller* instance, String nombre, int x, int y, int refrescar) {
	if (!m_party_manager || !Mazmorra::InBounds(x, y)) return;
	Node* h = m_party_manager->get_node_or_null(NodePath(nombre));
	if (!h) return;
	h->set_meta("grid_pos", Vector2i(x, y));
	if (refrescar == 0) return;
	RefrescarVision();
	if (h == ActiveChar()) UpdateHighlight();
}

// Braziers in view right now (x, y pairs): the shadow shader uses them as lights, same as the game
// vision (Mazmorra::CalcularVision)
PackedInt32Array BraserosVisibles(Caller* instance) {
	std::vector<Vector2i> out;
	if (!mapa.map_data.empty())
		for (const Vector2i& b : mapa.m_braseros)
			if (mapa.map_data[b.x][b.y].is_visible) out.push_back(b);
	return AParesInt(out);
}

// For tests: is there an event investigated by clicking (manual and active) on that tile?
Variant EsEventoManual(Caller* instance, int x, int y) {
	return Mazmorra::InBounds(x, y) && !mapa.map_data.empty() && mapa.EventoManualActivo(mapa.map_data[x][y]);
}

// Is there another hero (not the active one) on that tile? The PathManager doesn't let a path end
// there
Variant HayAliadoEn(Caller* instance, int x, int y) {
	return HayHeroeEn(Vector2i(x, y), ActiveChar());
}

// ==========================================
// FOR THE ENEMY AI (EnemyManager)
// ==========================================

// 1 = can be stepped on right now (floor with no walls, closed doors, chests, braziers, enemies or
// heroes)
PackedByteArray ObtenerTransitables(Caller* instance) {
	PackedByteArray out;
	out.resize(GRID_SIZE * GRID_SIZE);
	out.fill(0);
	if (mapa.map_data.empty()) return out;
	for (int x = 0; x < GRID_SIZE; x++)
		for (int y = 0; y < GRID_SIZE; y++) {
			const Cell& c = mapa.map_data[x][y];
			bool libre = !c.is_wall && !(c.is_door && !c.door_open) && !c.is_chest && !c.is_brasero && !c.has_enemy && Vector2i(x, y) != mapa.m_salida;
			out[x * GRID_SIZE + y] = libre ? 1 : 0;
		}
	for (Node* h : Heroes())
		if (HeroeEnMapa(h)) {
			Vector2i p = h->get_meta("grid_pos");
			if (Mazmorra::InBounds(p.x, p.y)) out[p.x * GRID_SIZE + p.y] = 0;
		}
	return out;
}

// FIXED map for the scripts (Tactica.gd stores it and uses it without calling C++ again: every
// GDScript -> C++ call costs ~0.8 ms). Per tile: bit 1 = can't be crossed (wall, column, closed
// door, chest, brazier, stairs); bit 2 = blocks sight (wall, column, closed door). Units don't
// count. It rarely changes: the meta "version_mapa" goes up every time (new floor, door opened).
PackedByteArray ObtenerMapaFijo(Caller* instance) {
	PackedByteArray out;
	out.resize(GRID_SIZE * GRID_SIZE);
	out.fill(0);
	if (mapa.map_data.empty()) return out;
	for (int x = 0; x < GRID_SIZE; x++)
		for (int y = 0; y < GRID_SIZE; y++) {
			const Cell& c = mapa.map_data[x][y];
			bool bloquea_paso = c.is_wall || (c.is_door && !c.door_open) || c.is_chest || c.is_brasero || Vector2i(x, y) == mapa.m_salida;
			out[x * GRID_SIZE + y] = (bloquea_paso ? 1 : 0) | (mapa.BloqueaVision(x, y) ? 2 : 0);
		}
	return out;
}

// Line of sight between two tiles, using the real map
Variant HayLineaDeVision(Caller* instance, int x0, int y0, int x1, int y1) {
	if (mapa.map_data.empty() || !Mazmorra::InBounds(x0, y0) || !Mazmorra::InBounds(x1, y1)) return false;
	return mapa.CheckLineOfSight(Vector2i(x0, y0), Vector2i(x1, y1));
}

// The enemy changes tile: grid, node name (it goes by position) and visibility
void MoverEnemigo(Caller* instance, int x0, int y0, int x1, int y1) {
	if (!Mazmorra::InBounds(x0, y0) || !Mazmorra::InBounds(x1, y1)) return;
	Cell& origen = mapa.map_data[x0][y0];
	Cell& destino = mapa.map_data[x1][y1];
	if (!origen.has_enemy || destino.has_enemy) return;
	Node3D* e = Object::cast_to<Node3D>(self->get_node_or_null(NodePath(NombreEnemigo(Vector2i(x0, y0)))));
	if (!e) return;
	origen.has_enemy = false;
	destino.has_enemy = true;
	e->set_name(NombreEnemigo(Vector2i(x1, y1)));
	e->set_meta("grid_pos", Vector2i(x1, y1));
	self->set_meta("lista_enemigos", mapa.ListaEnemigos());
	RefrescarVision();
}

// For the HUD minimap: one code per tile (see minimapa.gd)
PackedByteArray ObtenerMinimapa(Caller* instance) {
	PackedByteArray out;
	out.resize(GRID_SIZE * GRID_SIZE);
	if (mapa.map_data.empty()) return out;
	for (int x = 0; x < GRID_SIZE; x++)
		for (int y = 0; y < GRID_SIZE; y++) {
			const Cell& c = mapa.map_data[x][y];
			bool visto = c.is_explored || debug_mode;
			bool a_la_vista = c.is_visible || debug_mode;
			uint8_t v = 0;
			if (visto) {
				if (c.is_wall) v = c.is_explored ? 3 : 0;
				else if (c.is_door && !c.door_open) v = 4;
				else if (c.is_brasero) v = 10;
				else if (c.is_chest && !c.chest_opened) v = 5;
				else if (Vector2i(x, y) == mapa.m_salida) v = 6;
				else if (c.has_enemy && a_la_vista) v = 7;
				else if (mapa.EventoManualActivo(c) && a_la_vista) v = 11;
				else v = a_la_vista ? 2 : 1;
			}
			out[x * GRID_SIZE + y] = v;
		}
	Node* act = ActiveChar();
	for (Node* h : Heroes()) {
		if (!HeroeEnMapa(h)) continue;
		Vector2i p = h->get_meta("grid_pos");
		out[p.x * GRID_SIZE + p.y] = (h == act) ? 9 : 8;
	}
	return out;
}

// For the pixel art cursor: 0 = normal, 1 = interactable, 2 = enemy, 3 = can't go. CAREFUL:
// returning int through call() in Jenova comes back with garbage in the high bits: we return a
// Variant
Variant TipoCursor(Caller* instance, int x, int y) {
	if (!Mazmorra::InBounds(x, y) || mapa.map_data.empty() || Bloqueado()) return 0;
	const Cell& c = mapa.map_data[x][y];
	if (!(c.is_explored || debug_mode)) return 0;
	Node* act = ActiveChar();
	if (!act || !HeroeEnMapa(act)) return 0;
	if (Vector2i(x, y) == ActivePos()) return 0;
	if (HayHeroeEn(Vector2i(x, y), act)) return 1; // Click = select it
	String accion; int coste = 0, tipo = 0;
	if ((c.is_visible || debug_mode) && DescribirInteraccion(Vector2i(x, y), act, accion, coste, tipo)) {
		if (Vector2i(x, y) == mapa.m_salida) return c.is_highlighted ? 1 : 3;
		return tipo == 2 ? 2 : 1;
	}
	if (c.is_highlighted) return 0;
	return c.is_wall ? 0 : 3;
}

// For tests/debugging: really explored tiles (x,y pairs), not counting what debug mode reveals
PackedInt32Array DebugExploradas(Caller* instance) {
	std::vector<Vector2i> out;
	for (int x = 0; x < GRID_SIZE && !mapa.map_data.empty(); x++)
		for (int y = 0; y < GRID_SIZE; y++)
			if (mapa.map_data[x][y].is_explored) out.push_back(Vector2i(x, y));
	return AParesInt(out);
}

// For tests/debugging: tiles lit right now (x,y pairs)
PackedInt32Array DebugAlcance(Caller* instance) {
	std::vector<Vector2i> out;
	for (int x = 0; x < GRID_SIZE && !mapa.map_data.empty(); x++)
		for (int y = 0; y < GRID_SIZE; y++)
			if (mapa.map_data[x][y].is_highlighted && Vector2i(x, y) != ActivePos()) out.push_back(Vector2i(x, y));
	return AParesInt(out);
}

// ==========================================
// INPUT: HOVER AND CLICK
// ==========================================

// Mouse over a tile: path preview, cost, bubble and the hovered enemy
void ActualizarHover(Caller* instance, int hover_x, int hover_y) {
	if (!m_renderer || mapa.map_data.empty()) return;
	self->set_meta("hover_celda", Vector2i(hover_x, hover_y)); // The HUD reads it (card of the enemy under the cursor)

	bool dentro = Mazmorra::InBounds(hover_x, hover_y) && (mapa.map_data[hover_x][hover_y].is_explored || debug_mode);
	m_renderer->call("PintarCursor", dentro ? hover_x : -1, dentro ? hover_y : -1);
	m_renderer->call("PintarRuta", PackedInt32Array(), 0);
	m_renderer->call("MostrarCoste", -1, -1, String(""), 0);
	m_renderer->call("FijarBocadillos", 1, PackedInt32Array(), PackedStringArray(), PackedInt32Array());

	if (!dentro || Bloqueado() || !m_path_manager) return;
	Node* act = ActiveChar();
	if (!act || !HeroeEnMapa(act)) return;

	Vector2i destino(hover_x, hover_y);
	Vector2i origen = ActivePos();
	if (destino == origen) return;

	const Cell& c = mapa.map_data[hover_x][hover_y];
	String accion; int coste_accion = 0, tipo = 0;
	bool interactuable = (c.is_visible || debug_mode) && DescribirInteraccion(destino, act, accion, coste_accion, tipo);
	if (!c.is_highlighted && !interactuable) return;

	// Enemy within reach of a ranged weapon: attack from here, without walking
	int pa_ahora = SinCostes() ? 999 : (int)act->get_meta("pa_actual", 0);
	if (tipo == 2 && interactuable && reglas::Distancia(origen, destino) > 1 && PuedeAtacar(act, destino)) {
		PackedInt32Array celdas; celdas.push_back(hover_x); celdas.push_back(hover_y);
		PackedStringArray textos; textos.push_back(TextoBocadillo(accion, coste_accion));
		PackedInt32Array estilos; estilos.push_back(pa_ahora < coste_accion ? 1 : 2);
		m_renderer->call("FijarBocadillos", 1, celdas, textos, estilos);
		return;
	}

	Array ruta = m_path_manager->call("CalcularRuta", origen.x, origen.y, hover_x, hover_y);
	int pa = act->get_meta("pa_actual", 0);
	if (SinCostes()) pa = 999;

	std::vector<Vector2i> puntos;
	for (int i = 1; i < ruta.size(); i++) puntos.push_back(ruta[i]);
	int pasos = (int)puntos.size();
	int por_casilla = PaPorCasilla(act);
	m_renderer->call("PintarRuta", AParesInt(puntos), std::min(pa / por_casilla, pasos));

	int coste_paso = SinCostes() ? 0 : pasos * por_casilla;
	bool adyacente = std::abs(origen.x - hover_x) + std::abs(origen.y - hover_y) == 1;

	if (interactuable && destino != mapa.m_salida) {
		// The ones next to the hero already have their fixed bubble (channel 0)
		if (adyacente) return;
		int total = coste_paso + coste_accion;
		PackedInt32Array celdas; celdas.push_back(hover_x); celdas.push_back(hover_y);
		PackedStringArray textos; textos.push_back(TextoBocadillo(accion, total));
		PackedInt32Array estilos; estilos.push_back(pa < total ? 1 : 2);
		m_renderer->call("FijarBocadillos", 1, celdas, textos, estilos);
		return;
	}

	if (destino == mapa.m_salida && !adyacente) {
		PackedInt32Array celdas; celdas.push_back(hover_x); celdas.push_back(hover_y);
		PackedStringArray textos; textos.push_back(TextoBocadillo("Bajar la escalera", coste_paso));
		PackedInt32Array estilos; estilos.push_back(pa < coste_paso ? 1 : 2);
		m_renderer->call("FijarBocadillos", 1, celdas, textos, estilos);
		return;
	}

	if (c.is_highlighted && !Explorando()) {
		m_renderer->call("MostrarCoste", hover_x, hover_y, String::num_int64(coste_paso) + String(" PA"), 0);
	}
}

// The brain of the interactions
void ProcesarClicEnCasilla(Caller* instance, int x, int y) {
	if (!m_party_manager || mapa.map_data.empty()) return;

	if (!Mazmorra::InBounds(x, y) || Bloqueado()) return;
	if (self->has_meta("interaccion_pendiente")) self->remove_meta("interaccion_pendiente");

	Node* act = ActiveChar();
	if (!act || !HeroeEnMapa(act)) return;
	Vector2i actual = ActivePos();

	// Click on your own character: nothing (the actions are on the HUD bar)
	if (Vector2i(x, y) == actual) return;

	// Click on another hero: they become the active one and their card stays pinned on the HUD
	for (Node* h : Heroes()) {
		if (h == act || !HeroeEnMapa(h) || (Vector2i)h->get_meta("grid_pos") != Vector2i(x, y)) continue;
		String nombre = h->get_name();
		m_party_manager->call("CambiarPersonajeActivo", nombre);
		if (m_ui_manager) m_ui_manager->call("FijarFicha", nombre);
		return;
	}

	const Cell& c = mapa.map_data[x][y];
	bool cofre = c.is_chest && !c.chest_opened;
	bool puerta_cerrada = c.is_door && !c.door_open;
	bool enemigo = c.has_enemy;
	bool evento_manual = mapa.EventoManualActivo(c);
	bool hoguera = Vector2i(x, y) == mapa.m_hoguera;
	bool visible = c.is_visible || debug_mode;

	if ((cofre || puerta_cerrada || enemigo || evento_manual || hoguera) && visible) {
		// Enemy within weapon reach (adjacent or, with a ranged weapon, further away): attack
		if (enemigo && PuedeAtacar(act, Vector2i(x, y))) {
			Node* combat = self->get_node_or_null("CombatManager");
			if (combat) combat->call("AtacarEnemigo", x, y);
			return;
		}
		int dist = std::abs(actual.x - x) + std::abs(actual.y - y);
		if (dist == 1 && enemigo) {
			// Adjacent but the weapon doesn't reach (a bow point-blank): you need to step away
			Node* audio = self->get_node_or_null("/root/AudioManager");
			if (audio) audio->call("play_ui", "no");
			return;
		}
		if (dist == 1) {
			if (m_event_manager) {
				String tipo = hoguera ? "HOGUERA" : (cofre ? "COFRE" : (puerta_cerrada ? "PUERTA" : "EVENTO"));
				String path = evento_manual ? mapa.m_eventos[c.event_idx].path : String("");
				m_event_manager->call("Interactuar", x, y, tipo, path);
			}
			return;
		}
		// SWARM MODE: the PathManager takes us to the reachable adjacent tile
		if (m_path_manager) {
			if (evento_manual) {
				// The event is on a floor tile: we mark it solid for a moment so the path ends next
				// to it (like with chests) and on arrival it's investigated on its own
				self->set_meta("interaccion_pendiente", Vector2i(x, y));
				m_path_manager->call("SetCasillaSolida", x, y, 1);
				m_path_manager->call("MoverPersonaje", x, y);
				m_path_manager->call("SetCasillaSolida", x, y, 0);
				if (!(self->has_meta("is_moving") && (bool)self->get_meta("is_moving"))) self->remove_meta("interaccion_pendiente");
			} else {
				// The campfire is already solid: when you get next to it you rest on your own
				if (hoguera) self->set_meta("interaccion_pendiente", Vector2i(x, y));
				m_path_manager->call("MoverPersonaje", x, y);
				if (hoguera && !(self->has_meta("is_moving") && (bool)self->get_meta("is_moving"))) self->remove_meta("interaccion_pendiente");
			}
		}
		return;
	}

	// Walk
	if (c.is_highlighted && m_path_manager) m_path_manager->call("MoverPersonaje", x, y);
}

// Every step of the tween
void PasoDado(Caller* instance, int x, int y, int pasos) {
	if (!self || !m_party_manager) return;
	Vector2i pos(x, y);
	Node* antes = ActiveChar();
	Vector2i previa = antes ? (Vector2i)antes->get_meta("grid_pos", pos) : pos;
	m_party_manager->call("SetActiveGridPos", x, y);

	Node* audio = self->get_node_or_null("/root/AudioManager");
	if (audio) audio->call("play_sfx", "walk");

	RefrescarVision();

	Node* act = ActiveChar();
	if (!act) return;
	// Positioning (Tactica.gd): attacks of opportunity, bleeding, floor hazards. If it falls or
	// gets stunned, it stops here.
	Node* tac = self->get_node_or_null("/root/Tactica");
	if (tac && previa != pos && (bool)tac->call("al_mover", act, previa, pos)) {
		DetenerMovimiento(pos, pasos);
		UpdateUI();
		ComprobarFinDeTurno();
		UpdateHighlight();
		return;
	}
	// Walking through a teammate: it can't stay here, so nothing stops it on this tile
	if (HayHeroeEn(pos, act)) return;

	int idx = BuscarEventoAutomatico(act, pos);
	bool aggro = ComprobarAggro(act, pos);
	if (idx < 0 && !aggro) return;

	// Brake! We charge ONLY the steps taken up to here
	DetenerMovimiento(pos, pasos);

	if (idx >= 0) {
		UtilityFunctions::print(String::utf8("[DungeonManager] \xC2\xA1Trampa detonada en "), mapa.m_eventos[idx].pos.x, ", ", mapa.m_eventos[idx].pos.y, "!");
		if (m_event_manager) m_event_manager->call("Interactuar", mapa.m_eventos[idx].pos.x, mapa.m_eventos[idx].pos.y, String("TRAMPA"), mapa.m_eventos[idx].path);
	}

	UpdateUI();
	ComprobarFinDeTurno();
	UpdateHighlight();
}

// Normal end of the movement
void AtLast(Caller* instance, int x, int y, int steps_taken) {
	if (!self) return;
	self->set_meta("is_moving", false);
	if (!m_party_manager) return;

	m_party_manager->call("SetActiveGridPos", x, y);
	Node* act = ActiveChar();
	RestarPA(act, steps_taken);

	// Has it reached the stairs?
	if (act && Vector2i(x, y) == mapa.m_salida) {
		act->set_meta("pa_actual", 0);
		Node3D* token = Token(act);
		if (token) token->set_visible(false);
		m_party_manager->call("PersonajeEscapa", act);
	}

	// We were going to investigate an event: if we got next to it (and it's still there), it opens
	if (self->has_meta("interaccion_pendiente")) {
		Vector2i obj = self->get_meta("interaccion_pendiente");
		self->remove_meta("interaccion_pendiente");
		if (act && Mazmorra::InBounds(obj.x, obj.y) && std::abs(obj.x - x) + std::abs(obj.y - y) == 1) {
			const Cell& c = mapa.map_data[obj.x][obj.y];
			if (obj == mapa.m_hoguera && m_event_manager) {
				UpdateUI();
				m_event_manager->call("Interactuar", obj.x, obj.y, String("HOGUERA"), String(""));
				return;
			}
			if (mapa.EventoManualActivo(c) && m_event_manager) {
				UpdateUI();
				m_event_manager->call("Interactuar", obj.x, obj.y, String("EVENTO"), mapa.m_eventos[c.event_idx].path);
				return;
			}
		}
	}

	UpdateUI();
	ComprobarFinDeTurno();
	UpdateHighlight();
}

// ==========================================
// SERVANT FUNCTIONS (EventManager / CombatManager)
// ==========================================

// Marks a chest as opened (logic + lid animation)
void AplicarEstadoCofre(Caller* instance, int x, int y) {
	if (!Mazmorra::InBounds(x, y)) return;
	mapa.map_data[x][y].chest_opened = true;
	if (m_renderer) m_renderer->call("AbrirCofre", x, y);
}

// Opens a door: logic, AStar, the fixed map version and the animation
void AplicarEstadoPuerta(Caller* instance, int x, int y) {
	if (!Mazmorra::InBounds(x, y)) return;
	mapa.map_data[x][y].door_open = true;
	self->set_meta("version_mapa", (int)self->get_meta("version_mapa", 0) + 1); // ObtenerMapaFijo
	mapa.map_data[x][y].is_locked = false;
	if (m_path_manager) m_path_manager->call("SetCasillaSolida", x, y, 0);
	if (m_renderer) m_renderer->call("AbrirPuerta", x, y);
	RefrescarVision();
}

// The event is used up: it disappears from the map
void AplicarEstadoEvento(Caller* instance, int x, int y) {
	if (!Mazmorra::InBounds(x, y)) return;
	Cell& c = mapa.map_data[x][y];
	if (c.event_idx >= 0) mapa.m_eventos[c.event_idx].activo = false;
	c.has_event = false;

	Array lista = self->get_meta("lista_eventos", Array());
	lista.erase(Vector2i(x, y));
	self->set_meta("lista_eventos", lista);
	if (m_renderer) m_renderer->call("QuitarMarcadorEvento", x, y);
}

// The closest enemy the active hero's weapon reaches ("Attack" button)
Vector2i BuscarEnemigoAdyacente(Caller* instance) {
	Node* act = ActiveChar();
	Vector2i p = ActivePos();
	Vector2i mejor(-1, -1);
	int mejor_dist = 999;
	Array enemigos = mapa.ListaEnemigos();
	for (int i = 0; i < enemigos.size(); i++) {
		Vector2i e = enemigos[i];
		if (!(mapa.map_data[e.x][e.y].is_visible || debug_mode) || !PuedeAtacar(act, e)) continue;
		int d = reglas::Distancia(p, e);
		if (d < mejor_dist) { mejor_dist = d; mejor = e; }
	}
	return mejor;
}

// Can the active hero attack the enemy on (x, y) from where they are?
Variant PuedeAtacarA(Caller* instance, int x, int y) {
	return PuedeAtacar(ActiveChar(), Vector2i(x, y));
}

// Removes a dead enemy from the logic grid and the enemy list
void EliminarEnemigo(Caller* instance, int x, int y) {
	if (!Mazmorra::InBounds(x, y)) return;
	mapa.map_data[x][y].has_enemy = false;
	self->set_meta("lista_enemigos", mapa.ListaEnemigos());

	Node3D* enemigo = Object::cast_to<Node3D>(self->get_node_or_null(NodePath(NombreEnemigo(Vector2i(x, y)))));
	if (enemigo) {
		enemigo->set_name(NombreEnemigo(Vector2i(x, y)) + "_muerto");
		Ref<Tween> tw = enemigo->create_tween();
		tw->tween_property(enemigo, "scale", Vector3(1.4, 0.05, 1.4), 0.25)->set_trans(Tween::TRANS_BACK)->set_ease(Tween::EASE_IN);
		tw->tween_callback(Callable(enemigo, StringName("queue_free")));
	}

	// If no enemy is left on alert, the heroes leave combat
	bool quedan = false;
	for (int ex = 0; ex < GRID_SIZE; ex++)
		for (int ey = 0; ey < GRID_SIZE; ey++)
			if (mapa.map_data[ex][ey].has_enemy) {
				Node* e = self->get_node_or_null(NodePath(NombreEnemigo(Vector2i(ex, ey))));
				if (MetaBool(e, "in_combat")) quedan = true;
			}
	if (!quedan)
		for (Node* h : Heroes()) h->set_meta("in_combat", false);
}

// Requested by the EventManager and the CombatManager when the hero runs out of PA
void PedirFinDeTurno(Caller* instance) {
	if (self) ComprobarFinDeTurno();
}

// Switches to the next hero, but only once nothing else is running
void IntentarFinDeTurno(Caller* instance) {
	if (!self || !m_party_manager) return;
	// Is something still running? We wait a bit more
	if (MetaBool(self, "is_moving") || MetaBool(self, "bloqueo_evento") || MetaBool(self, "turno_enemigo")) {
		ProgramarFinDeTurno(0.2f);
		return;
	}
	self->set_meta("fin_turno_pendiente", false);
	// If meanwhile the player picked another hero with PA, there's nothing to switch
	if (HeroeSinTurno(ActiveChar())) m_party_manager->call("TerminarTurnoPersonaje");
}

// The event screen closed: the board unlocks and the turn can end
void FinalizarEvento(Caller* instance) {
	if (!self) return;
	self->set_meta("bloqueo_evento", false);
	self->set_meta("is_moving", false);
	UtilityFunctions::print(String::utf8("[DungeonManager] \xC2\xA1" "Evento finalizado, juego descongelado!"));
	ComprobarFinDeTurno();
	UpdateHighlight();
}

// With the event screen open the map gets darker (it doesn't slide any more: it takes the whole
// screen)
void SubirMapa(Caller* instance) {
	if (!self) return;
	Control* contenedor = Object::cast_to<Control>(self->get_node_or_null("/root/Main/ContenedorMapa"));
	if (!contenedor) return;
	Ref<Tween> tw = contenedor->create_tween();
	tw->tween_property(contenedor, "modulate", Color(0.35, 0.33, 0.42), 0.4)->set_trans(Tween::TRANS_CUBIC);
}

// The map goes back to full brightness when the event screen closes
void BajarMapa(Caller* instance) {
	if (!self) return;
	Control* contenedor = Object::cast_to<Control>(self->get_node_or_null("/root/Main/ContenedorMapa"));
	if (!contenedor) return;
	Ref<Tween> tw = contenedor->create_tween();
	tw->tween_property(contenedor, "modulate", Color(1, 1, 1), 0.4)->set_trans(Tween::TRANS_CUBIC);
}

// ==========================================
// DEBUG (F9)
// ==========================================

// F9: reveals the map, no PA cost and click = teleport
void ToggleModoDebug(Caller* instance) {
	debug_mode = !debug_mode;
	self->set_meta("debug_mode", debug_mode);
	UtilityFunctions::print("[DEBUG] Modo debug ", debug_mode ? "ACTIVADO (mapa revelado, sin coste de PA, clic = teletransporte)" : "DESACTIVADO");

	// Hidden traps are only marked in debug
	if (m_renderer)
		for (const EventInfo& e : mapa.m_eventos) {
			if (!e.activo || e.gatillo != 2) continue; // Only traps are hidden outside debug
			if (debug_mode) m_renderer->call("CrearMarcadorEvento", e.pos.x, e.pos.y);
			else m_renderer->call("QuitarMarcadorEvento", e.pos.x, e.pos.y);
		}
	UpdateHighlight();
}

// Debug: puts the active hero on a free tile instantly
void TeletransporteDebug(Caller* instance, int x, int y) {
	if (!Mazmorra::InBounds(x, y) || Bloqueado() || !m_party_manager) return;
	Node* act = ActiveChar();
	if (!act || !mapa.EsSueloLibre(x, y) || HayHeroeEn(Vector2i(x, y), act)) return;

	m_party_manager->call("SetActiveGridPos", x, y);
	Node3D* token = Token(act);
	if (token) token->set_position(GridToWorld(Vector2i(x, y)));
	UtilityFunctions::print("[DEBUG] Teletransporte a ", x, ", ", y);

	if (Vector2i(x, y) == mapa.m_salida) {
		AtLast(instance, x, y, 0);
		return;
	}
	UpdateHighlight();
}

// ==========================================
// FLOOR CHANGE
// ==========================================

// Builds the next floor: closes any event, removes the old enemies and generates a new one
void AvanzarAlSiguientePiso(Caller* instance) {
	if (!self) return;
	if (self->has_meta("bloqueo_evento") && (bool)self->get_meta("bloqueo_evento")) {
		UtilityFunctions::print(String::utf8("[DEBUG] \xC2\xA1No puedes bajar! Resuelve antes el evento activo."));
		return;
	}

	Node* audio = self->get_node_or_null("/root/AudioManager");
	if (audio) audio->call("play_sfx", "stairs");
	UtilityFunctions::print("[DungeonManager] Reconstruyendo la mazmorra...");

	if (m_ui_manager) m_ui_manager->call("CerrarEventoPorDebug");
	self->set_meta("bloqueo_evento", false);
	self->set_meta("is_moving", false);

	// Out with the previous floor's enemies
	TypedArray<Node> hijos = self->get_children();
	for (int i = 0; i < hijos.size(); i++) {
		Node* hijo = Object::cast_to<Node>(hijos[i]);
		if (hijo && String(hijo->get_name()).begins_with("Enemy_")) {
			self->remove_child(hijo);
			hijo->queue_free();
		}
	}

	self->set_meta("fin_turno_pendiente", false);
	ConstruirLogica(instance);
	// Before ConstruirVisual (which computes vision): otherwise the heroes would "see" the new
	// floor from where they stood on the old one and reveal rooms nobody has visited
	ColocarHeroesEnSpawn();
	ConstruirVisual(instance);

	if (m_party_manager) m_party_manager->call("SeleccionarPrimerDisponible");
	UpdateUI();
	UpdateHighlight();
	UtilityFunctions::print(String::utf8("[DungeonManager] \xC2\xA1NUEVO PISO GENERADO!"));
}

// ==========================================
// LIFE CYCLE
// ==========================================

void OnAwake(Caller* instance) {
	self = GetSelf<Node3D>(instance);
}

void OnReady(Caller* instance) {
	self = GetSelf<Node3D>(instance);
	m_party_manager = self->get_node_or_null("/root/Main/PartyManager");
	m_ui_manager = self->get_node_or_null("/root/Main/CanvasLayer");
	m_renderer = self->get_node_or_null("DungeonRenderer");
	m_path_manager = self->get_node_or_null("PathManager");
	m_event_manager = self->get_node_or_null("EventManager");
	self->set_meta("debug_mode", debug_mode);
	self->set_meta("is_moving", false);
	self->set_meta("bloqueo_evento", false);

	ConstruirLogica(instance);
	ConstruirVisual(instance);
}

void OnDestroy(Caller* instance) {
	self = nullptr;
	mapa = Mazmorra();
	m_party_manager = nullptr;
	m_ui_manager = nullptr;
	m_renderer = nullptr;
	m_path_manager = nullptr;
	m_event_manager = nullptr;
	debug_mode = false;
}

JENOVA_SCRIPT_END
