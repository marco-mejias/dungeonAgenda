/* Jenova C++ Node Base Script (Meteora) - DungeonManager (3D) */

// El cerebro: mantiene map_data (la cuadrícula lógica), genera la mazmorra,
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
#include <Godot/variant/utility_functions.hpp>
#include <algorithm>
#include <vector>
#include <queue>
#include <cmath>

using namespace godot;
using namespace jenova::sdk;

struct Cell {
	bool is_wall = true;
	bool is_highlighted = false;
	bool is_explored = false;
	bool is_visible = false;
	bool is_lit = false;
	bool is_door = false;
	bool door_open = false;
	bool is_chest = false;
	bool chest_opened = false;
	bool has_event = false;
	bool has_enemy = false;
	bool is_brasero = false; // obstáculo estático que ilumina su sala
	int event_idx = -1;
};

// Estructura para definir y manipular habitaciones
struct Room {
	int x, y, w, h;

	Vector2i get_center() const { return Vector2i(x + w / 2, y + h / 2); }

	bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }

	bool intersects(const Room& other) const {
		return (x <= other.x + other.w + 1 && x + w + 1 >= other.x &&
				y <= other.y + other.h + 1 && y + h + 1 >= other.y);
	}
};

// Datos de un evento, leídos del .tres UNA vez al generar el piso
struct EventInfo {
	Vector2i pos;
	String path;
	int gatillo = 0; // 0 = MANUAL, 1 = PROXIMIDAD, 2 = CONDICIONAL
	int radio = 3;
	String stat = "ninguno";
	int critico = 30;
	bool activo = true;
};

namespace {
	Node3D* self = nullptr;
	const int GRID_SIZE = 35;

	std::vector<std::vector<Cell>> map_data;
	std::vector<Room> m_rooms;
	std::vector<EventInfo> m_eventos;
	std::vector<Vector2i> m_spawn_cells;
	std::vector<Vector2i> m_braseros;
	const int RADIO_BRASERO = 3;
	Vector2i m_inicio(1, 1);
	Vector2i m_salida(2, 2);

	Node* m_party_manager = nullptr;
	Node* m_ui_manager = nullptr;
	Node* m_renderer = nullptr;
	Node* m_path_manager = nullptr;
	Node* m_event_manager = nullptr;

	bool debug_mode = false;

	const Vector2i DIRS4[4] = { Vector2i(0, -1), Vector2i(0, 1), Vector2i(-1, 0), Vector2i(1, 0) };

	// ==========================================
	// HELPERS (no expuestos a Godot)
	// ==========================================

	bool InBounds(int x, int y) { return x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE; }

	bool BloqueaVision(int x, int y) {
		const Cell& c = map_data[x][y];
		return c.is_wall || (c.is_door && !c.door_open);
	}

	Vector3 GridToWorld(Vector2i c) { return Vector3(c.x + 0.5f, 0.0f, c.y + 0.5f); }

	bool CheckLineOfSight(Vector2i start, Vector2i end) {
		int x0 = start.x, y0 = start.y, x1 = end.x, y1 = end.y;
		int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
		int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
		int err = dx + dy;

		while (true) {
			if (!InBounds(x0, y0)) return false;

			// Si golpeamos un obstáculo, podemos verlo, pero la visión se detiene ahí
			if (BloqueaVision(x0, y0) && !(x0 == start.x && y0 == start.y)) {
				return (x0 == x1 && y0 == y1);
			}
			if (x0 == x1 && y0 == y1) break;

			int e2 = 2 * err;
			int prev_x = x0, prev_y = y0;
			if (e2 >= dy) { err += dy; x0 += sx; }
			if (e2 <= dx) { err += dx; y0 += sy; }

			// Control estricto de esquinas (paso diagonal)
			if (x0 != prev_x && y0 != prev_y) {
				bool wall1 = InBounds(x0, prev_y) && BloqueaVision(x0, prev_y);
				bool wall2 = InBounds(prev_x, y0) && BloqueaVision(prev_x, y0);
				if (wall1 || wall2) return false;
			}
		}
		return true;
	}

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

	// ¿Puede alguien estar de pie aquí? (sin mirar a los héroes)
	bool EsSueloLibre(int x, int y) {
		if (!InBounds(x, y)) return false;
		const Cell& c = map_data[x][y];
		return !c.is_wall && !(c.is_door && !c.door_open) && !c.is_chest && !c.has_enemy && !c.is_brasero;
	}

	bool MenuAbierto() {
		if (!m_ui_manager) return false;
		Node* menu = m_ui_manager->get_node_or_null("MenuTactico_Instancia");
		return menu && !menu->is_queued_for_deletion();
	}

	bool Bloqueado() {
		return self && ((self->has_meta("bloqueo_evento") && (bool)self->get_meta("bloqueo_evento")) ||
						(self->has_meta("is_moving") && (bool)self->get_meta("is_moving")));
	}

	PackedInt32Array AParesInt(const std::vector<Vector2i>& celdas) {
		PackedInt32Array out;
		for (const Vector2i& c : celdas) { out.push_back(c.x); out.push_back(c.y); }
		return out;
	}

	Array ListaEnemigos() {
		Array out;
		for (int x = 0; x < GRID_SIZE; x++)
			for (int y = 0; y < GRID_SIZE; y++)
				if (map_data[x][y].has_enemy) out.push_back(Vector2i(x, y));
		return out;
	}

	void UpdateUI() {
		if (m_ui_manager) m_ui_manager->call("UpdateWarriorUI");
	}

	// Pide a la cámara que vuelva a centrarse en el héroe activo
	void RecentrarCamara() {
		Node* cam = self ? self->get_node_or_null("Camera3D") : nullptr;
		if (cam) cam->set_meta("recentrar", true);
	}

	// ==========================================
	// GENERACIÓN PROCEDURAL
	// ==========================================

	void GenerarMapa() {
		// Lienzo en negro: reiniciamos TODA la matriz (antes quedaban puertas y enemigos del piso anterior)
		map_data.assign(GRID_SIZE, std::vector<Cell>(GRID_SIZE));
		m_rooms.clear();

		const int MAX_ROOMS = 12, MIN_SIZE = 4, MAX_SIZE = 7, MAX_ATTEMPTS = 150;

		for (int i = 0; i < MAX_ATTEMPTS && (int)m_rooms.size() < MAX_ROOMS; i++) {
			int w = UtilityFunctions::randi_range(MIN_SIZE, MAX_SIZE);
			int h = UtilityFunctions::randi_range(MIN_SIZE, MAX_SIZE);
			int x = UtilityFunctions::randi_range(1, GRID_SIZE - w - 2);
			int y = UtilityFunctions::randi_range(1, GRID_SIZE - h - 2);
			Room nueva = { x, y, w, h };

			bool colision = false;
			for (const Room& otra : m_rooms) {
				if (nueva.intersects(otra)) { colision = true; break; }
			}
			if (colision) continue;

			for (int rx = x; rx < x + w; rx++)
				for (int ry = y; ry < y + h; ry++)
					map_data[rx][ry].is_wall = false;
			m_rooms.push_back(nueva);
		}

		// Pasillos en L entre salas consecutivas
		for (size_t i = 1; i < m_rooms.size(); i++) {
			Vector2i a = m_rooms[i - 1].get_center();
			Vector2i b = m_rooms[i].get_center();
			bool horizontal_primero = UtilityFunctions::randi_range(0, 1) == 0;
			if (horizontal_primero) {
				for (int x = std::min(a.x, b.x); x <= std::max(a.x, b.x); x++) map_data[x][a.y].is_wall = false;
				for (int y = std::min(a.y, b.y); y <= std::max(a.y, b.y); y++) map_data[b.x][y].is_wall = false;
			} else {
				for (int y = std::min(a.y, b.y); y <= std::max(a.y, b.y); y++) map_data[a.x][y].is_wall = false;
				for (int x = std::min(a.x, b.x); x <= std::max(a.x, b.x); x++) map_data[x][b.y].is_wall = false;
			}
		}
		UtilityFunctions::print("[DungeonManager] Mapa generado: ", (int)m_rooms.size(), " salas.");
	}

	bool EnAlgunaSala(int x, int y) {
		for (const Room& r : m_rooms) if (r.contains(x, y)) return true;
		return false;
	}

	void GenerarPuertas() {
		const int DISTANCIA_MINIMA = 6;
		const int PROBABILIDAD_PUERTA = 40;

		for (int x = 1; x < GRID_SIZE - 1; x++) {
			for (int y = 1; y < GRID_SIZE - 1; y++) {
				if (map_data[x][y].is_wall || EnAlgunaSala(x, y)) continue;

				bool pasillo_vertical = map_data[x - 1][y].is_wall && map_data[x + 1][y].is_wall && !map_data[x][y - 1].is_wall && !map_data[x][y + 1].is_wall;
				bool pasillo_horizontal = map_data[x][y - 1].is_wall && map_data[x][y + 1].is_wall && !map_data[x - 1][y].is_wall && !map_data[x + 1][y].is_wall;
				if (!pasillo_vertical && !pasillo_horizontal) continue;

				bool hay_puerta_cerca = false;
				for (int dx = -DISTANCIA_MINIMA; dx <= DISTANCIA_MINIMA && !hay_puerta_cerca; dx++)
					for (int dy = -DISTANCIA_MINIMA; dy <= DISTANCIA_MINIMA; dy++)
						if (InBounds(x + dx, y + dy) && map_data[x + dx][y + dy].is_door) { hay_puerta_cerca = true; break; }

				if (!hay_puerta_cerca && (int)(UtilityFunctions::randi() % 100) < PROBABILIDAD_PUERTA) {
					map_data[x][y].is_door = true;
					map_data[x][y].door_open = false;
				}
			}
		}
	}

	void DefinirInicioYSalida() {
		m_inicio = Vector2i(1, 1);
		m_salida = Vector2i(2, 2);
		if (m_rooms.empty()) return;

		m_inicio = m_rooms[0].get_center();

		if (m_rooms.size() > 1) {
			int max_dist = 0;
			std::vector<int> dist(m_rooms.size(), 0);
			for (size_t i = 1; i < m_rooms.size(); i++) {
				Vector2i c = m_rooms[i].get_center();
				dist[i] = std::abs(c.x - m_inicio.x) + std::abs(c.y - m_inicio.y);
				max_dist = std::max(max_dist, dist[i]);
			}
			std::vector<int> lejanas;
			for (size_t i = 1; i < m_rooms.size(); i++)
				if (dist[i] >= max_dist * 0.65) lejanas.push_back((int)i);
			m_salida = lejanas.empty() ? m_rooms.back().get_center()
									   : m_rooms[lejanas[UtilityFunctions::randi_range(0, (int)lejanas.size() - 1)]].get_center();
		} else {
			m_salida = m_inicio + Vector2i(1, 0);
		}

		// Sin puertas pegadas al spawn ni en la salida
		for (int dx = -2; dx <= 2; dx++)
			for (int dy = -2; dy <= 2; dy++)
				if (InBounds(m_inicio.x + dx, m_inicio.y + dy)) map_data[m_inicio.x + dx][m_inicio.y + dy].is_door = false;
		map_data[m_salida.x][m_salida.y].is_door = false;

		// Casillas de aparición: las 4 más cercanas al centro de la sala inicial
		m_spawn_cells.clear();
		std::vector<std::vector<bool>> visto(GRID_SIZE, std::vector<bool>(GRID_SIZE, false));
		std::queue<Vector2i> cola;
		cola.push(m_inicio);
		visto[m_inicio.x][m_inicio.y] = true;
		while (!cola.empty() && m_spawn_cells.size() < 4) {
			Vector2i p = cola.front(); cola.pop();
			m_spawn_cells.push_back(p);
			for (const Vector2i& d : DIRS4) {
				Vector2i n = p + d;
				if (InBounds(n.x, n.y) && !visto[n.x][n.y] && !map_data[n.x][n.y].is_wall && !map_data[n.x][n.y].is_door) {
					visto[n.x][n.y] = true;
					cola.push(n);
				}
			}
		}
	}

	bool CercaDelInicio(Vector2i p, int radio) {
		return std::abs(p.x - m_inicio.x) + std::abs(p.y - m_inicio.y) <= radio;
	}

	bool PegadoAPuerta(int x, int y) {
		for (const Vector2i& d : DIRS4)
			if (InBounds(x + d.x, y + d.y) && map_data[x + d.x][y + d.y].is_door) return true;
		return false;
	}

	// Casilla vacía, lejos del spawn y de la salida, sin taponar puertas
	bool CasillaDeSalaValida(int x, int y) {
		const Cell& c = map_data[x][y];
		if (c.is_wall || c.is_door || c.is_chest || c.has_event || c.has_enemy || c.is_brasero) return false;
		if (Vector2i(x, y) == m_salida || CercaDelInicio(Vector2i(x, y), 3) || PegadoAPuerta(x, y)) return false;
		return true;
	}

	void GenerarEventos() {
		m_eventos.clear();

		std::vector<String> disponibles;
		String carpeta = "res://Data/Events/";
		Ref<DirAccess> dir = DirAccess::open(carpeta);
		if (dir.is_valid()) {
			dir->list_dir_begin();
			for (String f = dir->get_next(); !f.is_empty(); f = dir->get_next()) {
				if (dir->current_is_dir()) continue;
				// En exportaciones los .tres pueden aparecer como .tres.remap
				if (f.ends_with(".tres.remap")) f = f.trim_suffix(".remap");
				if (f.ends_with(".tres")) disponibles.push_back(carpeta + f);
			}
		}
		if (disponibles.empty()) return;

		std::vector<Vector2i> libres;
		for (int x = 0; x < GRID_SIZE; x++)
			for (int y = 0; y < GRID_SIZE; y++)
				if (CasillaDeSalaValida(x, y)) libres.push_back(Vector2i(x, y));

		int a_crear = std::clamp((int)m_rooms.size() / 2, 2, 6);
		while ((int)m_eventos.size() < a_crear && !libres.empty()) {
			int idx = UtilityFunctions::randi_range(0, (int)libres.size() - 1);
			Vector2i pos = libres[idx];
			libres.erase(libres.begin() + idx);

			String path = disponibles[UtilityFunctions::randi_range(0, (int)disponibles.size() - 1)];
			Ref<Resource> evt = ResourceLoader::get_singleton()->load(path);
			if (evt.is_null()) continue;

			EventInfo info;
			info.pos = pos;
			info.path = path;
			info.gatillo = (int)evt->get("gatillo");
			info.radio = (int)evt->get("radio_activacion");
			info.stat = evt->get("stat_objetivo");
			info.critico = (int)evt->get("valor_critico");

			map_data[pos.x][pos.y].has_event = true;
			map_data[pos.x][pos.y].event_idx = (int)m_eventos.size();
			m_eventos.push_back(info);
		}
	}

	void GenerarCofres() {
		std::vector<Vector2i> candidatas;
		for (const Room& room : m_rooms)
			for (int x = room.x; x < room.x + room.w; x++)
				for (int y = room.y; y < room.y + room.h; y++)
					if (CasillaDeSalaValida(x, y)) candidatas.push_back(Vector2i(x, y));

		int creados = 0;
		while (creados < 4 && !candidatas.empty()) {
			int idx = UtilityFunctions::randi_range(0, (int)candidatas.size() - 1);
			Vector2i p = candidatas[idx];
			candidatas.erase(candidatas.begin() + idx);

			bool junto_a_cofre = false;
			for (const Vector2i& d : DIRS4)
				if (map_data[p.x + d.x][p.y + d.y].is_chest) junto_a_cofre = true;
			if (junto_a_cofre || !CasillaDeSalaValida(p.x, p.y)) continue;

			map_data[p.x][p.y].is_chest = true;
			creados++;
		}
	}

	// Un brasero en algunas salas (no la inicial). Nunca en la fila/columna central de la sala,
	// que es por donde entran los pasillos, para no taponar el paso.
	void GenerarBraseros() {
		m_braseros.clear();
		// 2ª pasada solo si la 1ª no colocó ninguno: así cada piso tiene al menos uno
		for (int pasada = 0; pasada < 2 && m_braseros.empty(); pasada++)
		for (size_t i = 1; i < m_rooms.size(); i++) {
			const Room& r = m_rooms[i];
			if (pasada == 1 && !m_braseros.empty()) break;
			if (r.w < 5 || r.h < 5 || (pasada == 0 && UtilityFunctions::randi_range(0, 99) >= 45)) continue;
			Vector2i centro = r.get_center();
			std::vector<Vector2i> validas;
			for (int x = r.x + 1; x < r.x + r.w - 1; x++)
				for (int y = r.y + 1; y < r.y + r.h - 1; y++) {
					if (x == centro.x || y == centro.y || !CasillaDeSalaValida(x, y)) continue;
					bool junto_a_cofre = false;
					for (const Vector2i& d : DIRS4)
						if (map_data[x + d.x][y + d.y].is_chest) junto_a_cofre = true;
					if (!junto_a_cofre) validas.push_back(Vector2i(x, y));
				}
			if (validas.empty()) continue;
			Vector2i p = validas[UtilityFunctions::randi_range(0, (int)validas.size() - 1)];
			map_data[p.x][p.y].is_brasero = true;
			m_braseros.push_back(p);
		}
	}

	void GenerarEnemigos() {
		// Saltamos la sala 0 (spawn del jugador). Solo en el interior para no taponar entradas.
		for (size_t i = 1; i < m_rooms.size(); i++) {
			const Room& r = m_rooms[i];
			std::vector<Vector2i> validas;
			for (int x = r.x + 1; x < r.x + r.w - 1; x++)
				for (int y = r.y + 1; y < r.y + r.h - 1; y++)
					if (CasillaDeSalaValida(x, y)) validas.push_back(Vector2i(x, y));

			int num = std::min((int)UtilityFunctions::randi_range(1, 2), (int)validas.size());
			for (int e = 0; e < num; e++) {
				int idx = UtilityFunctions::randi_range(0, (int)validas.size() - 1);
				Vector2i p = validas[idx];
				map_data[p.x][p.y].has_enemy = true;
				validas.erase(validas.begin() + idx);
			}
		}
	}

	// ==========================================
	// VISIÓN Y NIEBLA DE GUERRA
	// ==========================================

	void RefrescarVision() {
		if (!self || map_data.empty()) return;

		for (int x = 0; x < GRID_SIZE; x++)
			for (int y = 0; y < GRID_SIZE; y++) {
				map_data[x][y].is_visible = false;
				map_data[x][y].is_lit = false;
			}

		// Cada héroe en el mapa ilumina su radio de luz (con línea de visión)
		for (Node* h : Heroes()) {
			if (!HeroeEnMapa(h)) continue;
			Vector2i pos = h->get_meta("grid_pos");
			int r = h->has_meta("radio_luz") ? (int)h->get_meta("radio_luz") : 4;
			for (int x = std::max(0, pos.x - r); x <= std::min(GRID_SIZE - 1, pos.x + r); x++) {
				for (int y = std::max(0, pos.y - r); y <= std::min(GRID_SIZE - 1, pos.y + r); y++) {
					int dx = x - pos.x, dy = y - pos.y;
					if (dx * dx + dy * dy > r * r + r) continue;
					if (!CheckLineOfSight(pos, Vector2i(x, y))) continue;
					map_data[x][y].is_visible = true;
					map_data[x][y].is_lit = true;
					map_data[x][y].is_explored = true;
				}
			}
		}

		// Si se ve un brasero, se ve también lo que ilumina
		for (const Vector2i& b : m_braseros) {
			if (!map_data[b.x][b.y].is_visible) continue;
			for (int x = std::max(0, b.x - RADIO_BRASERO); x <= std::min(GRID_SIZE - 1, b.x + RADIO_BRASERO); x++)
				for (int y = std::max(0, b.y - RADIO_BRASERO); y <= std::min(GRID_SIZE - 1, b.y + RADIO_BRASERO); y++) {
					int dx = x - b.x, dy = y - b.y;
					if (dx * dx + dy * dy > RADIO_BRASERO * RADIO_BRASERO + RADIO_BRASERO) continue;
					if (!CheckLineOfSight(b, Vector2i(x, y))) continue;
					map_data[x][y].is_visible = true;
					map_data[x][y].is_lit = true;
					map_data[x][y].is_explored = true;
				}
		}

		// Los muros que rodean al suelo visible también se ven
		std::vector<Vector2i> muros;
		for (int x = 0; x < GRID_SIZE; x++)
			for (int y = 0; y < GRID_SIZE; y++) {
				if (!map_data[x][y].is_visible || map_data[x][y].is_wall) continue;
				for (int dx = -1; dx <= 1; dx++)
					for (int dy = -1; dy <= 1; dy++)
						if (InBounds(x + dx, y + dy) && map_data[x + dx][y + dy].is_wall) muros.push_back(Vector2i(x + dx, y + dy));
			}
		for (const Vector2i& m : muros) {
			map_data[m.x][m.y].is_visible = true;
			map_data[m.x][m.y].is_explored = true;
		}

		PackedByteArray estados;
		estados.resize(GRID_SIZE * GRID_SIZE);
		for (int x = 0; x < GRID_SIZE; x++)
			for (int y = 0; y < GRID_SIZE; y++) {
				const Cell& c = map_data[x][y];
				estados[x * GRID_SIZE + y] = (debug_mode || c.is_visible) ? 2 : (c.is_explored ? 1 : 0);
			}
		if (m_renderer) m_renderer->call("ActualizarNiebla", estados, debug_mode ? 1 : 0);

		// Los enemigos solo se ven si su casilla está a la vista
		for (int x = 0; x < GRID_SIZE; x++)
			for (int y = 0; y < GRID_SIZE; y++) {
				if (!map_data[x][y].has_enemy) continue;
				Node3D* e = Object::cast_to<Node3D>(self->get_node_or_null(NodePath(NombreEnemigo(Vector2i(x, y)))));
				if (e) e->set_visible(debug_mode || map_data[x][y].is_visible);
			}
	}

	void LimpiarAlcance() {
		for (int x = 0; x < GRID_SIZE && !map_data.empty(); x++)
			for (int y = 0; y < GRID_SIZE; y++)
				map_data[x][y].is_highlighted = false;
		if (m_renderer) m_renderer->call("LimpiarOverlay");
	}

	const int COSTE_INTERACCION = 2; // igual que EventManager y CombatManager

	bool EventoManualActivo(const Cell& c) {
		return c.has_event && c.event_idx >= 0 && m_eventos[c.event_idx].activo && m_eventos[c.event_idx].gatillo == 0;
	}

	// ¿Hay algo con lo que interactuar en esta casilla? Rellena la acción y su coste en PA.
	// tipo_overlay: 1 = interactuable, 2 = enemigo
	bool DescribirInteraccion(Vector2i p, Node* act, String& accion, int& coste, int& tipo_overlay) {
		if (!InBounds(p.x, p.y)) return false;
		const Cell& c = map_data[p.x][p.y];
		coste = debug_mode ? 0 : COSTE_INTERACCION;
		tipo_overlay = 1;
		if (c.has_enemy) {
			Node* e = self->get_node_or_null(NodePath(NombreEnemigo(p)));
			int dano = 1;
			if (e && act) dano = std::max(1, (int)act->get_meta("fuerza", 1) - (int)e->get_meta("defensa", 0));
			// Jenova recodifica el .cpp al preprocesarlo: los caracteres no ASCII van como bytes UTF-8 escapados
			accion = String("Atacar: ") + String::num_int64(dano) + String::utf8(" de da\xC3\xB1o");
			tipo_overlay = 2;
			return true;
		}
		if (c.is_chest && !c.chest_opened) { accion = "Abrir cofre"; return true; }
		if (c.is_door && !c.door_open) { accion = "Abrir puerta"; return true; }
		if (EventoManualActivo(c)) { accion = "Investigar"; return true; }
		if (p == m_salida) { accion = "Bajar la escalera"; coste = 0; return true; }
		return false;
	}

	String TextoBocadillo(const String& accion, int coste) {
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
			if (!InBounds(n.x, n.y) || !(map_data[n.x][n.y].is_visible || debug_mode)) continue;
			if (!DescribirInteraccion(n, act, accion, coste, tipo)) continue;
			if (n == m_salida) coste = 1; // un paso
			if (debug_mode) coste = 0;
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
		if (!InBounds(inicio.x, inicio.y)) return;
		Node* act = ActiveChar();

		std::vector<std::vector<bool>> visitado(GRID_SIZE, std::vector<bool>(GRID_SIZE, false));
		std::queue<std::pair<Vector2i, int>> cola;
		std::vector<Vector2i> iluminadas;

		cola.push({ inicio, 0 });
		visitado[inicio.x][inicio.y] = true;
		map_data[inicio.x][inicio.y].is_highlighted = true;

		while (!cola.empty()) {
			Vector2i pos = cola.front().first;
			int coste = cola.front().second;
			cola.pop();
			if (coste >= pa) continue;

			for (const Vector2i& d : DIRS4) {
				Vector2i n = pos + d;
				if (!InBounds(n.x, n.y) || visitado[n.x][n.y]) continue;
				visitado[n.x][n.y] = true;

				if (!EsSueloLibre(n.x, n.y) || HayHeroeEn(n, act)) continue;
				if (!CheckLineOfSight(inicio, n)) continue;

				map_data[n.x][n.y].is_highlighted = true;
				iluminadas.push_back(n);
				cola.push({ n, coste + 1 });
			}
		}

		// Ternas (x, y, tipo) para el renderer: el área de andar (con el héroe) y, alrededor,
		// los interactuables (dorado) y enemigos (rojo) que se pueden alcanzar
		PackedInt32Array ternas;
		std::vector<std::vector<bool>> marcado(GRID_SIZE, std::vector<bool>(GRID_SIZE, false));
		iluminadas.push_back(inicio);
		for (const Vector2i& c : iluminadas) {
			ternas.push_back(c.x); ternas.push_back(c.y); ternas.push_back(0);
			marcado[c.x][c.y] = true;
		}
		for (const Vector2i& c : iluminadas) {
			for (const Vector2i& d : DIRS4) {
				Vector2i n = c + d;
				if (!InBounds(n.x, n.y) || marcado[n.x][n.y] || !(map_data[n.x][n.y].is_visible || debug_mode)) continue;
				String accion; int coste = 0, tipo = 0;
				if (n == m_salida || !DescribirInteraccion(n, act, accion, coste, tipo)) continue;
				marcado[n.x][n.y] = true;
				ternas.push_back(n.x); ternas.push_back(n.y); ternas.push_back(tipo);
			}
		}
		if (m_renderer) m_renderer->call("PintarAlcance", ternas);
	}

	void UpdateHighlight() {
		if (!self || map_data.empty()) return;
		RefrescarVision();

		Node* act = ActiveChar();
		if (!act || Bloqueado() || MenuAbierto() || !HeroeEnMapa(act)) {
			LimpiarAlcance();
			LimpiarBocadillos();
			return;
		}
		int pa = act->has_meta("pa_actual") ? (int)act->get_meta("pa_actual") : 0;
		if (debug_mode) pa = std::max(pa, 8);
		HighlightReachableCells(ActivePos(), pa);
		ActualizarBocadillos(act, ActivePos());

		// La ruta/coste del hover se calculó desde la posición anterior: que se recalcule
		Node* input = self->get_node_or_null("InputManager");
		if (input) input->call("RefrescarHover");
	}

	// ==========================================
	// MOVIMIENTO: FIN E INTERRUPCIONES
	// ==========================================

	void RestarPA(Node* act, int cantidad) {
		if (!act || debug_mode || cantidad <= 0) return;
		int pa = act->has_meta("pa_actual") ? (int)act->get_meta("pa_actual") : 0;
		act->set_meta("pa_actual", std::max(0, pa - cantidad));
	}

	void ComprobarFinDeTurno() {
		Node* act = ActiveChar();
		if (!act || !m_party_manager) return;
		int pa = act->has_meta("pa_actual") ? (int)act->get_meta("pa_actual") : 0;
		if (pa <= 0 || !HeroeEnMapa(act)) m_party_manager->call("TerminarTurnoPersonaje");
	}

	// Frenada de emergencia: mata el tween, coloca la ficha en su casilla y cobra los pasos dados
	void DetenerMovimiento(Vector2i pos, int pasos) {
		Ref<Tween> tw = self->get_meta("current_move_tween", Variant());
		if (tw.is_valid()) tw->kill();
		self->set_meta("is_moving", false);

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
		for (size_t i = 0; i < m_eventos.size(); i++) {
			const EventInfo& e = m_eventos[i];
			if (!e.activo) continue;
			int dist = std::abs(pos.x - e.pos.x) + std::abs(pos.y - e.pos.y);

			if (e.gatillo == 1) { // PROXIMIDAD: salta al verlo dentro de su radio
				if (dist <= e.radio && CheckLineOfSight(pos, e.pos)) return (int)i;
			} else if (e.gatillo == 2 && dist == 0) { // CONDICIONAL: solo al pisarlo
				if (act && act->has_meta(e.stat) && (int)act->get_meta(e.stat) < e.critico) return (int)i;
			}
		}
		return -1;
	}

	bool ComprobarAggro(Node* act, Vector2i pos) {
		if (!act || MetaBool(act, "in_combat")) return false;

		const int RADIO_VISION = 6;
		bool detectado = false;
		for (int x = 0; x < GRID_SIZE; x++)
			for (int y = 0; y < GRID_SIZE; y++) {
				if (!map_data[x][y].has_enemy) continue;
				Node* enemigo = self->get_node_or_null(NodePath(NombreEnemigo(Vector2i(x, y))));
				if (!enemigo || MetaBool(enemigo, "in_combat")) continue;
				int dist = std::abs(pos.x - x) + std::abs(pos.y - y);
				if (dist <= RADIO_VISION && CheckLineOfSight(pos, Vector2i(x, y))) {
					enemigo->set_meta("in_combat", true);
					detectado = true;
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

	// Ternas (x, y, tipo): 0 = telaraña (esquina), 1 = huesos, 2 = calavera, 3 = rocas, 4 = piedras
	PackedInt32Array GenerarDecoracion() {
		PackedInt32Array ternas;
		for (const Room& r : m_rooms)
			for (int x = r.x; x < r.x + r.w; x++)
				for (int y = r.y; y < r.y + r.h; y++) {
					if (!CasillaDeSalaValida(x, y)) continue;
					bool spawn = false;
					for (const Vector2i& c : m_spawn_cells) if (c == Vector2i(x, y)) spawn = true;
					if (spawn) continue;

					bool muro_x = map_data[x - 1][y].is_wall || map_data[x + 1][y].is_wall;
					bool muro_y = map_data[x][y - 1].is_wall || map_data[x][y + 1].is_wall;
					if (!muro_x && !muro_y) continue; // solo pegado a la pared

					int tirada = UtilityFunctions::randi_range(0, 99);
					int tipo = -1;
					if (muro_x && muro_y) { if (tirada < 55) tipo = 0; }
					else if (tirada < 12) tipo = UtilityFunctions::randi_range(1, 4);
					if (tipo < 0) continue;
					ternas.push_back(x); ternas.push_back(y); ternas.push_back(tipo);
				}
		return ternas;
	}

	void ColocarHeroesEnSpawn() {
		int idx = 0;
		for (Node* h : Heroes()) {
			Node3D* token = Token(h);
			if (MetaBool(h, "esta_muerto")) {
				if (token) token->set_visible(false);
				continue;
			}
			Vector2i celda = m_spawn_cells.empty() ? m_inicio : m_spawn_cells[std::min(idx, (int)m_spawn_cells.size() - 1)];
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

JENOVA_SCRIPT_BEGIN

// ==========================================
// CONSTRUCCIÓN DEL PISO
// ==========================================

void ConstruirLogica(Caller* instance) {
	GenerarMapa();
	GenerarPuertas();
	DefinirInicioYSalida();
	GenerarEventos();
	GenerarCofres();
	GenerarBraseros();
	GenerarEnemigos();

	// CARGA ESTÁTICA DEL ASTAR: muros y puertas cerradas, UNA sola vez
	std::vector<Vector2i> solidos;
	for (int x = 0; x < GRID_SIZE; x++)
		for (int y = 0; y < GRID_SIZE; y++)
			if (BloqueaVision(x, y) || map_data[x][y].is_brasero) solidos.push_back(Vector2i(x, y));

	if (m_path_manager) {
		m_path_manager->call("ConfigurarAStar", GRID_SIZE, 1.0f);
		m_path_manager->call("CargarMurosEstaticos", AParesInt(solidos));
	}

	Array eventos, cofres;
	for (const EventInfo& e : m_eventos) eventos.push_back(e.pos);
	for (int x = 0; x < GRID_SIZE; x++)
		for (int y = 0; y < GRID_SIZE; y++)
			if (map_data[x][y].is_chest) cofres.push_back(Vector2i(x, y));

	self->set_meta("casilla_inicio", m_inicio);
	self->set_meta("casilla_salida", m_salida);
	self->set_meta("lista_eventos", eventos);
	self->set_meta("lista_cofres", cofres);
	self->set_meta("lista_enemigos", ListaEnemigos());

	Array puertas, braseros;
	for (int x = 0; x < GRID_SIZE; x++)
		for (int y = 0; y < GRID_SIZE; y++)
			if (map_data[x][y].is_door && !map_data[x][y].door_open) puertas.push_back(Vector2i(x, y));
	for (const Vector2i& b : m_braseros) braseros.push_back(b);
	self->set_meta("lista_puertas", puertas);
	self->set_meta("lista_braseros", braseros);

	UtilityFunctions::print("[DungeonManager] Lógica lista. Eventos: ", (int)m_eventos.size(), " | Cofres: ", cofres.size(), " | Enemigos: ", ListaEnemigos().size());
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
			const Cell& c = map_data[x][y];
			tipos[x * GRID_SIZE + y] = c.is_wall ? 0 : (c.is_door ? (c.door_open ? 3 : 2) : 1);
		}
	m_renderer->call("ConstruirMundo", tipos, GRID_SIZE);

	for (int x = 0; x < GRID_SIZE; x++)
		for (int y = 0; y < GRID_SIZE; y++)
			if (map_data[x][y].is_chest) m_renderer->call("CrearCofre", x, y);

	m_renderer->call("CrearSalida", m_salida.x, m_salida.y);
	for (const Vector2i& b : m_braseros) m_renderer->call("CrearBrasero", b.x, b.y);
	m_renderer->call("CrearDecoracion", GenerarDecoracion());

	// Solo los eventos MANUALES tienen marcador: las trampas son ocultas
	for (const EventInfo& e : m_eventos)
		if (e.gatillo == 0 || debug_mode) m_renderer->call("CrearMarcadorEvento", e.pos.x, e.pos.y);

	// ENEMIGOS
	Ref<PackedScene> escena = ResourceLoader::get_singleton()->load("res://Scenes/Enemy.tscn");
	if (escena.is_valid()) {
		int piso = (m_party_manager && m_party_manager->has_meta("piso_actual")) ? (int)m_party_manager->get_meta("piso_actual") : 1;
		const int frames[] = { 8, 9, 11, 12, 13, 11 };
		for (int x = 0; x < GRID_SIZE; x++)
			for (int y = 0; y < GRID_SIZE; y++) {
				if (!map_data[x][y].has_enemy) continue;
				Node3D* enemigo = Object::cast_to<Node3D>(escena->instantiate());
				if (!enemigo) continue;
				enemigo->set_name(NombreEnemigo(Vector2i(x, y)));
				enemigo->set_position(GridToWorld(Vector2i(x, y)));
				enemigo->set_meta("in_combat", false);
				int frame = frames[UtilityFunctions::randi_range(0, 5)];
				enemigo->set_meta("nombre_display", String(frame == 8 ? "Fantasma" : (frame <= 10 ? "Zombi" : "Esqueleto")));
				enemigo->set_meta("frame", frame);
				enemigo->set_meta("hp_max", 12 + 3 * piso);
				enemigo->set_meta("hp_actual", 12 + 3 * piso);
				enemigo->set_meta("fuerza", 5 + piso);
				enemigo->set_meta("defensa", 2);
				enemigo->set_visible(false);
				Sprite3D* cuerpo = Object::cast_to<Sprite3D>(enemigo->get_node_or_null("Cuerpo"));
				if (cuerpo) cuerpo->set_frame(frame);
				self->add_child(enemigo);
			}
	} else {
		UtilityFunctions::printerr("[DungeonManager] ERROR: no se pudo cargar res://Scenes/Enemy.tscn");
	}

	RefrescarVision();
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

	Vector2i celda = m_spawn_cells.empty() ? m_inicio : m_spawn_cells[std::min(index_posicion, (int)m_spawn_cells.size() - 1)];
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
	return InBounds(x, y) && !map_data.empty() && map_data[x][y].is_lit;
}

void UpdateHighlightForWarrior(Caller* instance) { UpdateHighlight(); }

void RefrescarLuces(Caller* instance) { UpdateHighlight(); }

void ApagarLucesMapa(Caller* instance) { LimpiarAlcance(); }

void ApagarLucesRuta(Caller* instance) { LimpiarAlcance(); }

// Lo usa el PathManager para inyectar entidades dinámicas en el AStar (pares x,y)
PackedInt32Array ObtenerObstaculosDinamicos(Caller* instance) {
	std::vector<Vector2i> obst;
	for (int x = 0; x < GRID_SIZE && !map_data.empty(); x++)
		for (int y = 0; y < GRID_SIZE; y++)
			if (map_data[x][y].has_enemy || map_data[x][y].is_chest) obst.push_back(Vector2i(x, y));
	for (Node* h : Heroes())
		if (HeroeEnMapa(h)) obst.push_back(h->get_meta("grid_pos"));
	return AParesInt(obst);
}

// Para el minimapa del HUD: un código por casilla (ver minimapa.gd)
PackedByteArray ObtenerMinimapa(Caller* instance) {
	PackedByteArray out;
	out.resize(GRID_SIZE * GRID_SIZE);
	if (map_data.empty()) return out;
	for (int x = 0; x < GRID_SIZE; x++)
		for (int y = 0; y < GRID_SIZE; y++) {
			const Cell& c = map_data[x][y];
			bool visto = c.is_explored || debug_mode;
			bool a_la_vista = c.is_visible || debug_mode;
			uint8_t v = 0;
			if (visto) {
				if (c.is_wall) v = c.is_explored ? 3 : 0;
				else if (c.is_door && !c.door_open) v = 4;
				else if (c.is_brasero) v = 10;
				else if (c.is_chest && !c.chest_opened) v = 5;
				else if (Vector2i(x, y) == m_salida) v = 6;
				else if (c.has_enemy && a_la_vista) v = 7;
				else if (EventoManualActivo(c) && a_la_vista) v = 11;
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
	if (!InBounds(x, y) || map_data.empty() || Bloqueado() || MenuAbierto()) return 0;
	const Cell& c = map_data[x][y];
	if (!(c.is_explored || debug_mode)) return 0;
	Node* act = ActiveChar();
	if (!act || !HeroeEnMapa(act)) return 0;
	if (Vector2i(x, y) == ActivePos()) return 0;
	if (HayHeroeEn(Vector2i(x, y), act)) return 1; // clic = seleccionarlo
	String accion; int coste = 0, tipo = 0;
	if ((c.is_visible || debug_mode) && DescribirInteraccion(Vector2i(x, y), act, accion, coste, tipo)) {
		if (Vector2i(x, y) == m_salida) return c.is_highlighted ? 1 : 3;
		return tipo == 2 ? 2 : 1;
	}
	if (c.is_highlighted) return 0;
	return c.is_wall ? 0 : 3;
}

// Para tests/depuración: casillas iluminadas ahora mismo (pares x,y)
PackedInt32Array DebugAlcance(Caller* instance) {
	std::vector<Vector2i> out;
	for (int x = 0; x < GRID_SIZE && !map_data.empty(); x++)
		for (int y = 0; y < GRID_SIZE; y++)
			if (map_data[x][y].is_highlighted && Vector2i(x, y) != ActivePos()) out.push_back(Vector2i(x, y));
	return AParesInt(out);
}

// ==========================================
// INPUT: HOVER Y CLIC
// ==========================================

void ActualizarHover(Caller* instance, int hover_x, int hover_y) {
	if (!m_renderer || map_data.empty()) return;
	self->set_meta("hover_celda", Vector2i(hover_x, hover_y)); // lo lee el HUD (ficha del enemigo bajo el cursor)

	bool dentro = InBounds(hover_x, hover_y) && (map_data[hover_x][hover_y].is_explored || debug_mode);
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

	const Cell& c = map_data[hover_x][hover_y];
	String accion; int coste_accion = 0, tipo = 0;
	bool interactuable = (c.is_visible || debug_mode) && DescribirInteraccion(destino, act, accion, coste_accion, tipo);
	if (!c.is_highlighted && !interactuable) return;

	Array ruta = m_path_manager->call("CalcularRuta", origen.x, origen.y, hover_x, hover_y);
	int pa = act->get_meta("pa_actual", 0);
	if (debug_mode) pa = 999;

	std::vector<Vector2i> puntos;
	for (int i = 1; i < ruta.size(); i++) puntos.push_back(ruta[i]);
	int pasos = (int)puntos.size();
	m_renderer->call("PintarRuta", AParesInt(puntos), std::min(pa, pasos));

	int coste_paso = debug_mode ? 0 : pasos;
	bool adyacente = std::abs(origen.x - hover_x) + std::abs(origen.y - hover_y) == 1;

	if (interactuable && destino != m_salida) {
		// Los pegados al héroe ya tienen su bocadillo fijo (canal 0)
		if (adyacente) return;
		int total = coste_paso + coste_accion;
		PackedInt32Array celdas; celdas.push_back(hover_x); celdas.push_back(hover_y);
		PackedStringArray textos; textos.push_back(TextoBocadillo(accion, total));
		PackedInt32Array estilos; estilos.push_back(pa < total ? 1 : 2);
		m_renderer->call("FijarBocadillos", 1, celdas, textos, estilos);
		return;
	}

	if (destino == m_salida && !adyacente) {
		PackedInt32Array celdas; celdas.push_back(hover_x); celdas.push_back(hover_y);
		PackedStringArray textos; textos.push_back(TextoBocadillo("Bajar la escalera", coste_paso));
		PackedInt32Array estilos; estilos.push_back(pa < coste_paso ? 1 : 2);
		m_renderer->call("FijarBocadillos", 1, celdas, textos, estilos);
		return;
	}

	if (c.is_highlighted) {
		m_renderer->call("MostrarCoste", hover_x, hover_y, String::num_int64(coste_paso) + String(" PA"), 0);
	}
}

void ToggleMenuTactico(Caller* instance) {
	if (!m_ui_manager) return;

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
	if (!m_party_manager || map_data.empty()) return;

	// 🛡️ ESCUDO ANTI-INPUT BLEEDING: con el menú abierto, cualquier clic en el tablero lo cierra
	if (MenuAbierto()) {
		Node* menu = m_ui_manager->get_node_or_null("MenuTactico_Instancia");
		menu->set_name("MenuTactico_Cerrando");
		menu->queue_free();
		UpdateHighlight();
		return;
	}

	if (!InBounds(x, y) || Bloqueado()) return;

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
		RecentrarCamara();
		return;
	}

	const Cell& c = map_data[x][y];
	bool cofre = c.is_chest && !c.chest_opened;
	bool puerta_cerrada = c.is_door && !c.door_open;
	bool enemigo = c.has_enemy;
	bool evento_manual = EventoManualActivo(c);
	bool visible = c.is_visible || debug_mode;

	if ((cofre || puerta_cerrada || enemigo || evento_manual) && visible) {
		int dist = std::abs(actual.x - x) + std::abs(actual.y - y);
		if (dist == 1) {
			if (enemigo) {
				Node* combat = self->get_node_or_null("CombatManager");
				if (combat) combat->call("AtacarEnemigo", x, y);
			} else if (m_event_manager) {
				String tipo = cofre ? "COFRE" : (puerta_cerrada ? "PUERTA" : "EVENTO");
				String path = evento_manual ? m_eventos[c.event_idx].path : String("");
				m_event_manager->call("Interactuar", x, y, tipo, path);
			}
			return;
		}
		// MODO ENJAMBRE: el PathManager nos lleva a la casilla adyacente alcanzable
		if (m_path_manager) m_path_manager->call("MoverPersonaje", x, y);
		return;
	}

	// Caminar
	if (c.is_highlighted && m_path_manager) m_path_manager->call("MoverPersonaje", x, y);
}

// Cada paso del tween
void PasoDado(Caller* instance, int x, int y, int pasos) {
	if (!self || !m_party_manager) return;
	Vector2i pos(x, y);
	m_party_manager->call("SetActiveGridPos", x, y);

	Node* audio = self->get_node_or_null("/root/AudioManager");
	if (audio) audio->call("play_sfx", "walk");

	RefrescarVision();

	Node* act = ActiveChar();
	if (!act) return;

	int idx = BuscarEventoAutomatico(act, pos);
	bool aggro = ComprobarAggro(act, pos);
	if (idx < 0 && !aggro) return;

	// ¡Frenada! Cobramos SOLO los pasos dados hasta aquí
	DetenerMovimiento(pos, pasos);

	if (idx >= 0) {
		UtilityFunctions::print("[DungeonManager] ¡Trampa detonada en ", m_eventos[idx].pos.x, ", ", m_eventos[idx].pos.y, "!");
		if (m_event_manager) m_event_manager->call("Interactuar", m_eventos[idx].pos.x, m_eventos[idx].pos.y, String("TRAMPA"), m_eventos[idx].path);
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
	if (act && Vector2i(x, y) == m_salida) {
		act->set_meta("pa_actual", 0);
		Node3D* token = Token(act);
		if (token) token->set_visible(false);
		m_party_manager->call("PersonajeEscapa", act);
	}

	UpdateUI();
	ComprobarFinDeTurno();
	UpdateHighlight();
}

// ==========================================
// FUNCIONES SIRVIENTES (EventManager / CombatManager)
// ==========================================

void AplicarEstadoCofre(Caller* instance, int x, int y) {
	if (!InBounds(x, y)) return;
	map_data[x][y].chest_opened = true;
	if (m_renderer) m_renderer->call("AbrirCofre", x, y);
}

void AplicarEstadoPuerta(Caller* instance, int x, int y) {
	if (!InBounds(x, y)) return;
	map_data[x][y].door_open = true;
	if (m_path_manager) m_path_manager->call("SetCasillaSolida", x, y, 0);
	if (m_renderer) m_renderer->call("AbrirPuerta", x, y);
	RefrescarVision();
}

void AplicarEstadoEvento(Caller* instance, int x, int y) {
	if (!InBounds(x, y)) return;
	Cell& c = map_data[x][y];
	if (c.event_idx >= 0) m_eventos[c.event_idx].activo = false;
	c.has_event = false;

	Array lista = self->get_meta("lista_eventos", Array());
	lista.erase(Vector2i(x, y));
	self->set_meta("lista_eventos", lista);
	if (m_renderer) m_renderer->call("QuitarMarcadorEvento", x, y);
}

Vector2i BuscarEnemigoAdyacente(Caller* instance) {
	Vector2i p = ActivePos();
	for (const Vector2i& d : DIRS4) {
		Vector2i n = p + d;
		if (InBounds(n.x, n.y) && map_data[n.x][n.y].has_enemy) return n;
	}
	return Vector2i(-1, -1);
}

void EliminarEnemigo(Caller* instance, int x, int y) {
	if (!InBounds(x, y)) return;
	map_data[x][y].has_enemy = false;
	self->set_meta("lista_enemigos", ListaEnemigos());

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
			if (map_data[ex][ey].has_enemy) {
				Node* e = self->get_node_or_null(NodePath(NombreEnemigo(Vector2i(ex, ey))));
				if (MetaBool(e, "in_combat")) quedan = true;
			}
	if (!quedan)
		for (Node* h : Heroes()) h->set_meta("in_combat", false);
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
		for (const EventInfo& e : m_eventos) {
			if (!e.activo || e.gatillo == 0) continue;
			if (debug_mode) m_renderer->call("CrearMarcadorEvento", e.pos.x, e.pos.y);
			else m_renderer->call("QuitarMarcadorEvento", e.pos.x, e.pos.y);
		}
	UpdateHighlight();
}

void TeletransporteDebug(Caller* instance, int x, int y) {
	if (!InBounds(x, y) || Bloqueado() || !m_party_manager) return;
	Node* act = ActiveChar();
	if (!act || !EsSueloLibre(x, y) || HayHeroeEn(Vector2i(x, y), act)) return;

	m_party_manager->call("SetActiveGridPos", x, y);
	Node3D* token = Token(act);
	if (token) token->set_position(GridToWorld(Vector2i(x, y)));
	RecentrarCamara();
	UtilityFunctions::print("[DEBUG] Teletransporte a ", x, ", ", y);

	if (Vector2i(x, y) == m_salida) {
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

	ConstruirLogica(instance);
	ConstruirVisual(instance);
	ColocarHeroesEnSpawn();

	if (m_party_manager) m_party_manager->call("SeleccionarPrimerDisponible");
	RecentrarCamara();
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
	map_data.clear();
	m_rooms.clear();
	m_eventos.clear();
	m_spawn_cells.clear();
	m_braseros.clear();
	m_party_manager = nullptr;
	m_ui_manager = nullptr;
	m_renderer = nullptr;
	m_path_manager = nullptr;
	m_event_manager = nullptr;
	debug_mode = false;
}

JENOVA_SCRIPT_END
