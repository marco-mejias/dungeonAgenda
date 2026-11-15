/* Jenova C++ Node Base Script (Meteora) - DungeonManager (3D) */

// El cerebro: mantiene mapa.map_data (la cuadrícula lógica), genera la mazmorra,
// procesa los clics y coordina al resto de managers. No pinta nada: eso es
// trabajo del DungeonRenderer.

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
	Mazmorra mapa; // la cuadrícula lógica del piso actual


	Node* m_party_manager = nullptr;
	Node* m_ui_manager = nullptr;
	Node* m_renderer = nullptr;
	Node* m_path_manager = nullptr;
	Node* m_event_manager = nullptr;

	bool debug_mode = false;

	// Exploración (Modo.gd, sin enemigos despiertos): andar no gasta PA y no hay turnos.
	bool Explorando() { return self && self->has_meta("modo_exploracion") && (bool)self->get_meta("modo_exploracion"); }
	// Sin coste de PA: el modo debug o la exploración
	bool SinCostes() { return debug_mode || Explorando(); }


	// ==========================================
	// HELPERS (no expuestos a Godot)
	// ==========================================

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

	// Oculto (Sigilo.gd): andar cuesta 2 PA por casilla (la ruta, el alcance y el coste lo cuentan)
	int PaPorCasilla(Node* act) {
		if (!act || SinCostes() || !act->has_meta("estados")) return 1;
		Dictionary est = act->get_meta("estados");
		return est.has("oculto") ? 2 : 1;
	}

	bool MetaBool(Node* n, const char* key) { return n && n->has_meta(key) && (bool)n->get_meta(key); }

	bool HeroeEnMapa(Node* h) { return h && !MetaBool(h, "esta_muerto") && !MetaBool(h, "ha_escapado") && h->has_meta("grid_pos"); }

	Node* ActiveChar() {
		if (!m_party_manager) return nullptr;
		return Object::cast_to<Node>(m_party_manager->call("GetActiveCharacter"));
	}

	Vector2i ActivePos() {
		if (!m_party_manager) return Vector2i(-1, -1);
		return m_party_manager->call("GetActiveGridPos");
	}

	Node3D* Token(Node* hero) {
		if (!self || !hero) return nullptr;
		return Object::cast_to<Node3D>(self->get_node_or_null(NodePath(String(hero->get_name()))));
	}

	String NombreEnemigo(Vector2i p) { return "Enemy_" + String::num_int64(p.x) + "_" + String::num_int64(p.y); }

	bool HayHeroeEn(Vector2i p, Node* excepto) {
		for (Node* h : Heroes()) {
			if (h == excepto || !HeroeEnMapa(h)) continue;
			if ((Vector2i)h->get_meta("grid_pos") == p) return true;
		}
		return false;
	}

	bool MenuAbierto() {
		if (!m_ui_manager) return false;
		Node* menu = m_ui_manager->get_node_or_null("MenuTactico_Instancia");
		return menu && !menu->is_queued_for_deletion();
	}

	bool Bloqueado() {
		return self && ((self->has_meta("bloqueo_evento") && (bool)self->get_meta("bloqueo_evento")) ||
						(self->has_meta("is_moving") && (bool)self->get_meta("is_moving")) ||
						// el mensaje de cambio de turno/día (banner_turno.gd) aún se está leyendo
						(self->has_meta("bloqueo_banner") && (bool)self->get_meta("bloqueo_banner")));
	}

	PackedInt32Array AParesInt(const std::vector<Vector2i>& celdas) {
		PackedInt32Array out;
		for (const Vector2i& c : celdas) { out.push_back(c.x); out.push_back(c.y); }
		return out;
	}

	void UpdateUI() {
		if (m_ui_manager) m_ui_manager->call("UpdateWarriorUI");
	}


	// ==========================================
	// VISIÓN Y NIEBLA DE GUERRA
	// ==========================================

	void RefrescarVision() {
		if (!self || mapa.vacia()) return;

		// Cada héroe en el mapa es un foco de luz
		std::vector<Foco> focos;
		for (Node* h : Heroes()) {
			if (!HeroeEnMapa(h)) continue;
			focos.push_back({ (Vector2i)h->get_meta("grid_pos"), RadioVision(h) });
		}
		mapa.CalcularVision(focos);
		if (m_renderer) m_renderer->call("ActualizarNiebla", mapa.EstadosNiebla(debug_mode), debug_mode ? 1 : 0);

		// Los enemigos solo se ven si su casilla está a la vista
		for (int x = 0; x < GRID_SIZE; x++)
			for (int y = 0; y < GRID_SIZE; y++) {
				if (!mapa.map_data[x][y].has_enemy) continue;
				Node3D* e = Object::cast_to<Node3D>(self->get_node_or_null(NodePath(NombreEnemigo(Vector2i(x, y)))));
				if (e) e->set_visible(debug_mode || mapa.map_data[x][y].is_visible);
			}
	}

	void LimpiarAlcance() {
		for (int x = 0; x < GRID_SIZE && !mapa.map_data.empty(); x++)
			for (int y = 0; y < GRID_SIZE; y++)
				mapa.map_data[x][y].is_highlighted = false;
		if (m_renderer) m_renderer->call("LimpiarOverlay");
	}

	const int COSTE_INTERACCION = 2; // igual que EventManager (atacar cuesta lo que diga el arma)

	// ¿Puede el héroe atacar al enemigo de 'p' desde donde está? (alcance del arma + línea de visión)
	bool PuedeAtacar(Node* act, Vector2i p) {
		if (!act || !Mazmorra::InBounds(p.x, p.y) || !mapa.map_data[p.x][p.y].has_enemy) return false;
		Vector2i origen = act->get_meta("grid_pos", Vector2i(-99, -99));
		if (!reglas::EnAlcance(act, origen, p)) return false;
		return !reglas::RequiereVision(act) || mapa.CheckLineOfSight(origen, p);
	}

	// ¿Hay algo con lo que interactuar en esta casilla? Rellena la acción y su coste en PA.
	// tipo_overlay: 1 = interactuable, 2 = enemigo
	bool DescribirInteraccion(Vector2i p, Node* act, String& accion, int& coste, int& tipo_overlay) {
		if (!Mazmorra::InBounds(p.x, p.y)) return false;
		const Cell& c = mapa.map_data[p.x][p.y];
		coste = SinCostes() ? 0 : COSTE_INTERACCION;
		tipo_overlay = 1;
		if (c.has_enemy) {
			if (act && !debug_mode) coste = reglas::CosteAtaque(act);
			// Sin cuentas: lo que hace el golpe lo enseña la tarjeta de previsión
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

	String TextoBocadillo(const String& accion, int coste) {
		if (Explorando()) return accion; // sin turnos no hay PA que contar
		return accion + String("\n") + String::num_int64(coste) + String(" PA");
	}

	void LimpiarBocadillos() {
		if (!m_renderer) return;
		m_renderer->call("FijarBocadillos", 0, PackedInt32Array(), PackedStringArray(), PackedInt32Array());
		m_renderer->call("FijarBocadillos", 1, PackedInt32Array(), PackedStringArray(), PackedInt32Array());
		m_renderer->call("MostrarCoste", -1, -1, String(""), 0);
	}

	// Bocadillos sobre los interactuables pegados al héroe activo
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
			if (tipo == 2 && !PuedeAtacar(act, n)) continue; // p.ej. con un arco no se dispara a bocajarro
			if (n == mapa.m_salida) coste = 1; // un paso
			if (SinCostes()) coste = 0;
			celdas.push_back(n.x); celdas.push_back(n.y);
			textos.push_back(TextoBocadillo(accion, coste));
			estilos.push_back(pa < coste ? 1 : 0);
		}
		m_renderer->call("FijarBocadillos", 0, celdas, textos, estilos);
	}

	// Flood fill con PA y LoS. Muros, puertas cerradas, cofres, enemigos y otros héroes
	// actúan como muro real: se marcan visitados pero NO se iluminan ni se atraviesan.
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
				// Explorando se puede ir a cualquier sitio ya explorado; en combate, solo a lo que se ve
				if (Explorando() ? !mapa.map_data[n.x][n.y].is_explored : !mapa.CheckLineOfSight(inicio, n)) continue;

				// Se puede pasar por encima de un compañero, pero no acabar en su casilla
				if (!HayHeroeEn(n, act)) {
					mapa.map_data[n.x][n.y].is_highlighted = true;
					iluminadas.push_back(n);
				}
				cola.push({ n, coste + 1 });
			}
		}

		// Ternas (x, y, tipo) para el renderer: el área de andar (con el héroe) y, alrededor,
		// los interactuables (dorado) y enemigos (rojo) que se pueden alcanzar
		PackedInt32Array ternas;
		std::vector<std::vector<bool>> marcado(GRID_SIZE, std::vector<bool>(GRID_SIZE, false));
		iluminadas.push_back(inicio);
		for (const Vector2i& c : iluminadas) {
			// Explorando no se dibuja el área de andar (se puede ir a todo lo explorado)
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

	void UpdateHighlight() {
		if (!self || mapa.map_data.empty()) return;
		RefrescarVision();

		Node* act = ActiveChar();
		if (!act || Bloqueado() || MenuAbierto() || !HeroeEnMapa(act)) {
			LimpiarAlcance();
			LimpiarBocadillos();
			return;
		}
		int pa = act->has_meta("pa_actual") ? (int)act->get_meta("pa_actual") : 0;
		if (debug_mode) pa = std::max(pa, 8);
		if (Explorando()) pa = GRID_SIZE * GRID_SIZE; // sin límite
		HighlightReachableCells(ActivePos(), pa / PaPorCasilla(act));
		ActualizarBocadillos(act, ActivePos());

		// La ruta/coste del hover se calculó desde la posición anterior: que se recalcule
		Node* input = self->get_node_or_null("InputManager");
		if (input) input->call("RefrescarHover");
	}

	// ==========================================
	// MOVIMIENTO: FIN E INTERRUPCIONES
	// ==========================================

	void RestarPA(Node* act, int cantidad) {
		if (!act || SinCostes() || cantidad <= 0) return;
		int pa = act->has_meta("pa_actual") ? (int)act->get_meta("pa_actual") : 0;
		act->set_meta("pa_actual", std::max(0, pa - cantidad));
	}

	// El cambio al siguiente héroe es lo ÚLTIMO del turno: se pide aquí y se hace cuando ya no
	// queda nada en marcha (puerta girando, botín volando, evento abierto, ataque, turno enemigo).
	const float ESPERA_FIN_TURNO = 0.85f; // lo que tardan la puerta (0.55) y el botín (0.77)

	bool HeroeSinTurno(Node* act) {
		return act && ((int)act->get_meta("pa_actual", 0) <= 0 || !HeroeEnMapa(act));
	}

	void ProgramarFinDeTurno(float espera) {
		self->set_meta("fin_turno_pendiente", true);
		Ref<Tween> tw = self->create_tween();
		tw->tween_interval(espera);
		tw->tween_callback(Callable(self, StringName("IntentarFinDeTurno")));
	}

	void ComprobarFinDeTurno() {
		Node* act = ActiveChar();
		if (!act || !m_party_manager || MetaBool(self, "fin_turno_pendiente")) return;
		if (HeroeSinTurno(act)) ProgramarFinDeTurno(ESPERA_FIN_TURNO);
	}

	// Frenada de emergencia: mata el tween, coloca la ficha en su casilla y cobra los pasos dados
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

	// Devuelve el índice del evento automático que salta en esta casilla, o -1
	int BuscarEventoAutomatico(Node* act, Vector2i pos) {
		for (size_t i = 0; i < mapa.m_eventos.size(); i++) {
			const EventInfo& e = mapa.m_eventos[i];
			if (!e.activo) continue;
			int dist = std::abs(pos.x - e.pos.x) + std::abs(pos.y - e.pos.y);

			if (e.gatillo == 1) { // PROXIMIDAD: salta al verlo dentro de su radio
				if (dist <= e.radio && mapa.CheckLineOfSight(pos, e.pos)) return (int)i;
			} else if (e.gatillo == 2 && dist == 0) { // CONDICIONAL: solo al pisarlo
				if (act && act->has_meta(e.stat) && (int)act->get_meta(e.stat) < e.critico) return (int)i;
			}
		}
		return -1;
	}

	// ¿Ve algún enemigo dormido a algún héroe? Regla única y simétrica: el enemigo tiene que estar a
	// la vista del grupo (su casilla visible) y, desde cualquier héroe en el mapa (el líder o los de
	// su fila), dentro de su radio de detección medido en círculo y con la misma línea de visión que
	// las sombras. Vale también en mitad de un combate (despierta a otro pelotón).
	bool ComprobarAggro(Node* act, Vector2i pos) {
		if (!act) return false;
		// Con Sigilo.gd: conos de visión (hacia dónde mira cada enemigo). 1 = empieza el sigilo (frena
		// para que el jugador decida), 2 = le han visto (combate)
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
			UtilityFunctions::print("[Combate] ¡", act->get_name(), " HA ENTRADO EN COMBATE!");
			Node* audio = self->get_node_or_null("/root/AudioManager");
			if (audio) audio->call("play_sfx", "fail");
		}
		return detectado;
	}

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

// Crea la ficha del enemigo de la casilla. Los de un campamento o de la sala del jefe salen ya en
// su pelotón (id fijo por sala); el de la casilla del jefe se hace jefe (Zonas.gd)
static Node3D* CrearFichaEnemigo(Vector2i celda, int sala) {
	Ref<PackedScene> escena = ResourceLoader::get_singleton()->load("res://Scenes/Enemy.tscn"); // la caché del loader la guarda
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
// CONSTRUCCIÓN DEL PISO
// ==========================================

void ConstruirLogica(Caller* instance) {
	mapa.m_piso = m_party_manager ? (int)m_party_manager->get_meta("piso_actual", 1) : 1;
	mapa.GenerarPiso();
	self->set_meta("botin_cofres", Dictionary()); // botín pendiente de los cofres del piso anterior

	// CARGA ESTÁTICA DEL ASTAR: muros y puertas cerradas, UNA sola vez
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

	// Estructura del piso (la usa Zonas.gd): hoguera, jefe, llave, campamentos
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

	UtilityFunctions::print("[DungeonManager] Lógica lista. Eventos: ", (int)mapa.m_eventos.size(), " | Cofres: ", cofres.size(), " | Enemigos: ", mapa.ListaEnemigos().size());
}

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

	// Los eventos manuales y los de proximidad se ven en el mapa (su sprite); las trampas son ocultas
	for (const EventInfo& e : mapa.m_eventos) {
		if (e.gatillo != 2) m_renderer->call("CrearSpriteEvento", e.pos.x, e.pos.y, e.path);
		else if (debug_mode) m_renderer->call("CrearMarcadorEvento", e.pos.x, e.pos.y);
	}

	// ENEMIGOS: el Catálogo elige el tipo (Data/Enemigos) según el piso
	for (int x = 0; x < GRID_SIZE; x++)
		for (int y = 0; y < GRID_SIZE; y++)
			if (mapa.map_data[x][y].has_enemy) CrearFichaEnemigo(Vector2i(x, y), mapa.map_data[x][y].peloton);

	RefrescarVision();
}

// Un enemigo nuevo en una casilla libre (refuerzos de los campamentos). 'sala' = la del pelotón
// al que se une (-1 = suelto). Devuelve su ficha o null.
Variant CrearEnemigoEn(Caller* instance, int x, int y, int sala) {
	if (!Mazmorra::InBounds(x, y) || !mapa.EsSueloLibre(x, y) || Vector2i(x, y) == mapa.m_salida || HayHeroeEn(Vector2i(x, y), nullptr)) return Variant();
	mapa.map_data[x][y].has_enemy = true;
	Node3D* e = CrearFichaEnemigo(Vector2i(x, y), sala);
	if (!e) { mapa.map_data[x][y].has_enemy = false; return Variant(); }
	self->set_meta("lista_enemigos", mapa.ListaEnemigos());
	RefrescarVision();
	return e;
}

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
// VISIÓN Y ALCANCE (API pública)
// ==========================================

void CalcularMapaDeLuces(Caller* instance) { RefrescarVision(); }

void ActualizarVisibilidadEntidades(Caller* instance) { RefrescarVision(); }

Variant IsCasillaIluminada(Caller* instance, int x, int y) {
	return Mazmorra::InBounds(x, y) && !mapa.map_data.empty() && mapa.map_data[x][y].is_lit;
}

void UpdateHighlightForWarrior(Caller* instance) { UpdateHighlight(); }

// Rueda del HUD: al pasar el ratón por ATACAR (tipo 0) se ve en rojo hasta dónde llega el arma
// (con línea de visión si es a distancia) y los enemigos que se pueden golpear; por HABILIDAD
// (tipo 1), en azul, dónde puede aparecer la gallina. Al salir, OcultarRangoAccion.
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

// Lo usa el PathManager para inyectar entidades dinámicas en el AStar (pares x,y)
// Tipo de cada casilla (0 muro, 1 suelo, 2/3 puerta cerrada/abierta, 4 columna): para ver mapas generados
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

PackedInt32Array ObtenerObstaculosDinamicos(Caller* instance) {
	std::vector<Vector2i> obst;
	// Los héroes solo trazan rutas por donde ya han explorado: no saben el camino por lo oscuro
	for (int x = 0; x < GRID_SIZE && !mapa.map_data.empty(); x++)
		for (int y = 0; y < GRID_SIZE; y++) {
			const Cell& c = mapa.map_data[x][y];
			if (c.has_enemy || c.is_chest || (!c.is_explored && !c.is_wall && !debug_mode)) obst.push_back(Vector2i(x, y));
		}
	// Los héroes no tapan: se puede cruzar a un compañero (PathManager evita acabar encima)
	return AParesInt(obst);
}

// Pone a un héroe (cualquiera, no solo el activo) en una casilla: empujones, saltos, intercambios,
// el grupo que sigue al líder... (Tactica/Modo). No mira si está libre: eso lo decide quien llama.
// refrescar = 0 no recalcula la visión (es cara): para los pasos intermedios de un recorrido.
void ColocarHeroe(Caller* instance, String nombre, int x, int y, int refrescar) {
	if (!m_party_manager || !Mazmorra::InBounds(x, y)) return;
	Node* h = m_party_manager->get_node_or_null(NodePath(nombre));
	if (!h) return;
	h->set_meta("grid_pos", Vector2i(x, y));
	if (refrescar == 0) return;
	RefrescarVision();
	if (h == ActiveChar()) UpdateHighlight();
}

// Braseros que se ven ahora (pares x, y): el shader de las sombras los usa como luces, igual que la
// visión del juego (Mazmorra::CalcularVision)
PackedInt32Array BraserosVisibles(Caller* instance) {
	std::vector<Vector2i> out;
	if (!mapa.map_data.empty())
		for (const Vector2i& b : mapa.m_braseros)
			if (mapa.map_data[b.x][b.y].is_visible) out.push_back(b);
	return AParesInt(out);
}

// Para los tests: ¿hay un evento que se investiga con clic (manual y activo) en esa casilla?
Variant EsEventoManual(Caller* instance, int x, int y) {
	return Mazmorra::InBounds(x, y) && !mapa.map_data.empty() && mapa.EventoManualActivo(mapa.map_data[x][y]);
}

// ¿Hay otro héroe (no el activo) en esa casilla? El PathManager no deja que una ruta acabe ahí
Variant HayAliadoEn(Caller* instance, int x, int y) {
	return HayHeroeEn(Vector2i(x, y), ActiveChar());
}

// ==========================================
// PARA LA IA DE LOS ENEMIGOS (EnemyManager)
// ==========================================

// 1 = se puede pisar ahora mismo (suelo sin muros, puertas cerradas, cofres, braseros, enemigos ni héroes)
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

// Mapa FIJO para los scripts (Tactica.gd lo guarda y lo usa sin volver a llamar al C++: cada llamada
// de GDScript al C++ cuesta ~0.8 ms). Por casilla: bit 1 = no se puede pasar (muro, columna, puerta
// cerrada, cofre, brasero, escalera); bit 2 = tapa la vista (muro, columna, puerta cerrada). Las
// unidades no cuentan. Cambia poco: la meta "version_mapa" sube cada vez (piso nuevo, puerta abierta).
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

Variant HayLineaDeVision(Caller* instance, int x0, int y0, int x1, int y1) {
	if (mapa.map_data.empty() || !Mazmorra::InBounds(x0, y0) || !Mazmorra::InBounds(x1, y1)) return false;
	return mapa.CheckLineOfSight(Vector2i(x0, y0), Vector2i(x1, y1));
}

// El enemigo cambia de casilla: cuadrícula, nombre del nodo (va por posición) y visibilidad
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

// Para el minimapa del HUD: un código por casilla (ver minimapa.gd)
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

// Para el cursor pixel art: 0 = normal, 1 = interactuable, 2 = enemigo, 3 = no se puede ir
// OJO: devolver int por call() en Jenova llega con basura en los bits altos: devolvemos Variant
Variant TipoCursor(Caller* instance, int x, int y) {
	if (!Mazmorra::InBounds(x, y) || mapa.map_data.empty() || Bloqueado() || MenuAbierto()) return 0;
	const Cell& c = mapa.map_data[x][y];
	if (!(c.is_explored || debug_mode)) return 0;
	Node* act = ActiveChar();
	if (!act || !HeroeEnMapa(act)) return 0;
	if (Vector2i(x, y) == ActivePos()) return 0;
	if (HayHeroeEn(Vector2i(x, y), act)) return 1; // clic = seleccionarlo
	String accion; int coste = 0, tipo = 0;
	if ((c.is_visible || debug_mode) && DescribirInteraccion(Vector2i(x, y), act, accion, coste, tipo)) {
		if (Vector2i(x, y) == mapa.m_salida) return c.is_highlighted ? 1 : 3;
		return tipo == 2 ? 2 : 1;
	}
	if (c.is_highlighted) return 0;
	return c.is_wall ? 0 : 3;
}

// Para tests/depuración: casillas exploradas de verdad (pares x,y), sin contar lo que revela el modo debug
PackedInt32Array DebugExploradas(Caller* instance) {
	std::vector<Vector2i> out;
	for (int x = 0; x < GRID_SIZE && !mapa.map_data.empty(); x++)
		for (int y = 0; y < GRID_SIZE; y++)
			if (mapa.map_data[x][y].is_explored) out.push_back(Vector2i(x, y));
	return AParesInt(out);
}

// Para tests/depuración: casillas iluminadas ahora mismo (pares x,y)
PackedInt32Array DebugAlcance(Caller* instance) {
	std::vector<Vector2i> out;
	for (int x = 0; x < GRID_SIZE && !mapa.map_data.empty(); x++)
		for (int y = 0; y < GRID_SIZE; y++)
			if (mapa.map_data[x][y].is_highlighted && Vector2i(x, y) != ActivePos()) out.push_back(Vector2i(x, y));
	return AParesInt(out);
}

// ==========================================
// INPUT: HOVER Y CLIC
// ==========================================

void ActualizarHover(Caller* instance, int hover_x, int hover_y) {
	if (!m_renderer || mapa.map_data.empty()) return;
	self->set_meta("hover_celda", Vector2i(hover_x, hover_y)); // lo lee el HUD (ficha del enemigo bajo el cursor)

	bool dentro = Mazmorra::InBounds(hover_x, hover_y) && (mapa.map_data[hover_x][hover_y].is_explored || debug_mode);
	m_renderer->call("PintarCursor", dentro ? hover_x : -1, dentro ? hover_y : -1);
	m_renderer->call("PintarRuta", PackedInt32Array(), 0);
	m_renderer->call("MostrarCoste", -1, -1, String(""), 0);
	m_renderer->call("FijarBocadillos", 1, PackedInt32Array(), PackedStringArray(), PackedInt32Array());

	if (!dentro || Bloqueado() || MenuAbierto() || !m_path_manager) return;
	Node* act = ActiveChar();
	if (!act || !HeroeEnMapa(act)) return;

	Vector2i destino(hover_x, hover_y);
	Vector2i origen = ActivePos();
	if (destino == origen) return;

	const Cell& c = mapa.map_data[hover_x][hover_y];
	String accion; int coste_accion = 0, tipo = 0;
	bool interactuable = (c.is_visible || debug_mode) && DescribirInteraccion(destino, act, accion, coste_accion, tipo);
	if (!c.is_highlighted && !interactuable) return;

	// Enemigo al alcance de un arma a distancia: se ataca desde aquí, sin caminar
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
		// Los pegados al héroe ya tienen su bocadillo fijo (canal 0)
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

void ToggleMenuTactico(Caller* instance) {
	if (!m_ui_manager) return;
	// Con el HUD nuevo las acciones están en la rueda de abajo a la derecha: no hay menú flotante
	if (m_ui_manager->get_node_or_null("HUD")) return;

	Node* existente = m_ui_manager->get_node_or_null("MenuTactico_Instancia");
	if (existente) {
		existente->set_name("MenuTactico_Cerrando");
		existente->queue_free();
		UpdateHighlight();
		return;
	}

	Ref<PackedScene> escena = ResourceLoader::get_singleton()->load("res://Scenes/MenuTactico.tscn");
	if (escena.is_valid()) {
		Node* menu = escena->instantiate();
		menu->set_name("MenuTactico_Instancia");
		m_ui_manager->add_child(menu);
		LimpiarAlcance();
		Node* audio = self->get_node_or_null("/root/AudioManager");
		if (audio) audio->call("play_ui", "open");
	}
}

// El cerebro de las interacciones
void ProcesarClicEnCasilla(Caller* instance, int x, int y) {
	if (!m_party_manager || mapa.map_data.empty()) return;

	// 🛡️ ESCUDO ANTI-INPUT BLEEDING: con el menú abierto, cualquier clic en el tablero lo cierra
	if (MenuAbierto()) {
		Node* menu = m_ui_manager->get_node_or_null("MenuTactico_Instancia");
		menu->set_name("MenuTactico_Cerrando");
		menu->queue_free();
		UpdateHighlight();
		return;
	}

	if (!Mazmorra::InBounds(x, y) || Bloqueado()) return;
	if (self->has_meta("interaccion_pendiente")) self->remove_meta("interaccion_pendiente");

	Node* act = ActiveChar();
	if (!act || !HeroeEnMapa(act)) return;
	Vector2i actual = ActivePos();

	// Clic en tu propio personaje: menú táctico
	if (Vector2i(x, y) == actual) {
		ToggleMenuTactico(instance);
		return;
	}

	// Clic en otro héroe: pasa a ser el activo y su ficha se queda fija en el HUD
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
		// Enemigo al alcance del arma (pegado o, con arma a distancia, más lejos): atacar
		if (enemigo && PuedeAtacar(act, Vector2i(x, y))) {
			Node* combat = self->get_node_or_null("CombatManager");
			if (combat) combat->call("AtacarEnemigo", x, y);
			return;
		}
		int dist = std::abs(actual.x - x) + std::abs(actual.y - y);
		if (dist == 1 && enemigo) {
			// Pegado pero el arma no llega (un arco a bocajarro): toca apartarse
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
		// MODO ENJAMBRE: el PathManager nos lleva a la casilla adyacente alcanzable
		if (m_path_manager) {
			if (evento_manual) {
				// El evento está en una casilla de suelo: la marcamos sólida un momento para que la
				// ruta acabe al lado (como con los cofres) y al llegar se investiga solo
				self->set_meta("interaccion_pendiente", Vector2i(x, y));
				m_path_manager->call("SetCasillaSolida", x, y, 1);
				m_path_manager->call("MoverPersonaje", x, y);
				m_path_manager->call("SetCasillaSolida", x, y, 0);
				if (!(self->has_meta("is_moving") && (bool)self->get_meta("is_moving"))) self->remove_meta("interaccion_pendiente");
			} else {
				// La hoguera ya es sólida: al llegar a su lado se descansa solo
				if (hoguera) self->set_meta("interaccion_pendiente", Vector2i(x, y));
				m_path_manager->call("MoverPersonaje", x, y);
				if (hoguera && !(self->has_meta("is_moving") && (bool)self->get_meta("is_moving"))) self->remove_meta("interaccion_pendiente");
			}
		}
		return;
	}

	// Caminar
	if (c.is_highlighted && m_path_manager) m_path_manager->call("MoverPersonaje", x, y);
}

// Cada paso del tween
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
	// Posicionamiento (Tactica.gd): ataques de oportunidad, sangrado, peligros del suelo.
	// Si cae o queda aturdido, se para aquí.
	Node* tac = self->get_node_or_null("/root/Tactica");
	if (tac && previa != pos && (bool)tac->call("al_mover", act, previa, pos)) {
		DetenerMovimiento(pos, pasos);
		UpdateUI();
		ComprobarFinDeTurno();
		UpdateHighlight();
		return;
	}
	// Cruzando a un compañero: aquí no puede quedarse, así que nada lo frena en esta casilla
	if (HayHeroeEn(pos, act)) return;

	int idx = BuscarEventoAutomatico(act, pos);
	bool aggro = ComprobarAggro(act, pos);
	if (idx < 0 && !aggro) return;

	// ¡Frenada! Cobramos SOLO los pasos dados hasta aquí
	DetenerMovimiento(pos, pasos);

	if (idx >= 0) {
		UtilityFunctions::print("[DungeonManager] ¡Trampa detonada en ", mapa.m_eventos[idx].pos.x, ", ", mapa.m_eventos[idx].pos.y, "!");
		if (m_event_manager) m_event_manager->call("Interactuar", mapa.m_eventos[idx].pos.x, mapa.m_eventos[idx].pos.y, String("TRAMPA"), mapa.m_eventos[idx].path);
	}

	UpdateUI();
	ComprobarFinDeTurno();
	UpdateHighlight();
}

// Fin normal del movimiento
void AtLast(Caller* instance, int x, int y, int steps_taken) {
	if (!self) return;
	self->set_meta("is_moving", false);
	if (!m_party_manager) return;

	m_party_manager->call("SetActiveGridPos", x, y);
	Node* act = ActiveChar();
	RestarPA(act, steps_taken);

	// ¿Ha llegado a la escalera?
	if (act && Vector2i(x, y) == mapa.m_salida) {
		act->set_meta("pa_actual", 0);
		Node3D* token = Token(act);
		if (token) token->set_visible(false);
		m_party_manager->call("PersonajeEscapa", act);
	}

	// Íbamos a investigar un evento: si hemos llegado a su lado (y sigue ahí), se abre
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
// FUNCIONES SIRVIENTES (EventManager / CombatManager)
// ==========================================

void AplicarEstadoCofre(Caller* instance, int x, int y) {
	if (!Mazmorra::InBounds(x, y)) return;
	mapa.map_data[x][y].chest_opened = true;
	if (m_renderer) m_renderer->call("AbrirCofre", x, y);
}

void AplicarEstadoPuerta(Caller* instance, int x, int y) {
	if (!Mazmorra::InBounds(x, y)) return;
	mapa.map_data[x][y].door_open = true;
	self->set_meta("version_mapa", (int)self->get_meta("version_mapa", 0) + 1); // ObtenerMapaFijo
	mapa.map_data[x][y].is_locked = false;
	if (m_path_manager) m_path_manager->call("SetCasillaSolida", x, y, 0);
	if (m_renderer) m_renderer->call("AbrirPuerta", x, y);
	RefrescarVision();
}

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

// El enemigo más cercano al que llega el arma del héroe activo (botón "Atacar" del menú)
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

Variant PuedeAtacarA(Caller* instance, int x, int y) {
	return PuedeAtacar(ActiveChar(), Vector2i(x, y));
}

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

	// Si ya no queda ningún enemigo en alerta, los héroes salen del combate
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

// Lo piden el EventManager y el CombatManager cuando el héroe se queda sin PA
void PedirFinDeTurno(Caller* instance) {
	if (self) ComprobarFinDeTurno();
}

void IntentarFinDeTurno(Caller* instance) {
	if (!self || !m_party_manager) return;
	// ¿Algo sigue en marcha? Esperamos un poco más
	if (MetaBool(self, "is_moving") || MetaBool(self, "bloqueo_evento") || MetaBool(self, "turno_enemigo")) {
		ProgramarFinDeTurno(0.2f);
		return;
	}
	self->set_meta("fin_turno_pendiente", false);
	// Si mientras tanto el jugador eligió a otro héroe con PA, no hay nada que cambiar
	if (HeroeSinTurno(ActiveChar())) m_party_manager->call("TerminarTurnoPersonaje");
}

void FinalizarEvento(Caller* instance) {
	if (!self) return;
	self->set_meta("bloqueo_evento", false);
	self->set_meta("is_moving", false);
	UtilityFunctions::print("[DungeonManager] ¡Evento finalizado, juego descongelado!");
	ComprobarFinDeTurno();
	UpdateHighlight();
}

// Con la pantalla de evento abierta el mapa se oscurece (ya no se desplaza: ocupa toda la pantalla)
void SubirMapa(Caller* instance) {
	if (!self) return;
	Control* contenedor = Object::cast_to<Control>(self->get_node_or_null("/root/Main/ContenedorMapa"));
	if (!contenedor) return;
	Ref<Tween> tw = contenedor->create_tween();
	tw->tween_property(contenedor, "modulate", Color(0.35, 0.33, 0.42), 0.4)->set_trans(Tween::TRANS_CUBIC);
}

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

void ToggleModoDebug(Caller* instance) {
	debug_mode = !debug_mode;
	self->set_meta("debug_mode", debug_mode);
	UtilityFunctions::print("[DEBUG] Modo debug ", debug_mode ? "ACTIVADO (mapa revelado, sin coste de PA, clic = teletransporte)" : "DESACTIVADO");

	// Las trampas ocultas solo se marcan en debug
	if (m_renderer)
		for (const EventInfo& e : mapa.m_eventos) {
			if (!e.activo || e.gatillo != 2) continue; // solo las trampas se ocultan fuera de debug
			if (debug_mode) m_renderer->call("CrearMarcadorEvento", e.pos.x, e.pos.y);
			else m_renderer->call("QuitarMarcadorEvento", e.pos.x, e.pos.y);
		}
	UpdateHighlight();
}

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
// CAMBIO DE PISO
// ==========================================

void AvanzarAlSiguientePiso(Caller* instance) {
	if (!self) return;
	if (self->has_meta("bloqueo_evento") && (bool)self->get_meta("bloqueo_evento")) {
		UtilityFunctions::print("[DEBUG] ¡No puedes bajar! Resuelve antes el evento activo.");
		return;
	}

	Node* audio = self->get_node_or_null("/root/AudioManager");
	if (audio) audio->call("play_sfx", "stairs");
	UtilityFunctions::print("[DungeonManager] Reconstruyendo la mazmorra...");

	if (m_ui_manager) m_ui_manager->call("CerrarEventoPorDebug");
	self->set_meta("bloqueo_evento", false);
	self->set_meta("is_moving", false);

	// Fuera los enemigos del piso anterior
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
	// Antes de ConstruirVisual (que calcula la visión): si no, los héroes "verían" el piso nuevo
	// desde donde estaban en el anterior y revelarían salas que nadie ha visitado
	ColocarHeroesEnSpawn();
	ConstruirVisual(instance);

	if (m_party_manager) m_party_manager->call("SeleccionarPrimerDisponible");
	UpdateUI();
	UpdateHighlight();
	UtilityFunctions::print("[DungeonManager] ¡NUEVO PISO GENERADO!");
}

// ==========================================
// CICLO DE VIDA
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
