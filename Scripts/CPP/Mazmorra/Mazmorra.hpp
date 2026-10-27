#pragma once
// Mazmorra: la cuadrícula lógica de un piso (lo que antes eran las globales del DungeonManager).
// Sabe generarse (salas, pasillos, puertas, eventos, cofres, braseros, enemigos y decoración),
// calcular la visión/niebla y responder consultas de la cuadrícula. No conoce nodos de Godot:
// el DungeonManager le pasa lo que necesita (p.ej. los focos de luz de los héroes).

#include <Godot/godot.hpp>
#include <Godot/classes/resource_loader.hpp>
#include <Godot/classes/resource.hpp>
#include <Godot/classes/dir_access.hpp>
#include <Godot/variant/utility_functions.hpp>
#include <algorithm>
#include <vector>
#include <queue>
#include <cmath>

namespace mazmorra {

using namespace godot;

const int GRID_SIZE = 35;
const int RADIO_BRASERO = 3;
const Vector2i DIRS4[4] = { Vector2i(0, -1), Vector2i(0, 1), Vector2i(-1, 0), Vector2i(1, 0) };

inline Vector3 GridToWorld(Vector2i c) { return Vector3(c.x + 0.5f, 0.0f, c.y + 0.5f); }

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

// Un punto que ilumina a su alrededor (un héroe en el mapa)
struct Foco {
	Vector2i pos;
	int radio = 4;
};

struct Mazmorra {
	std::vector<std::vector<Cell>> map_data;
	std::vector<Room> m_rooms;
	std::vector<EventInfo> m_eventos;
	std::vector<Vector2i> m_spawn_cells;
	std::vector<Vector2i> m_braseros;
	Vector2i m_inicio = Vector2i(1, 1);
	Vector2i m_salida = Vector2i(2, 2);

	bool vacia() const { return map_data.empty(); }

	// Genera un piso completo desde cero
	void GenerarPiso() {
		GenerarMapa();
		GenerarPuertas();
		DefinirInicioYSalida();
		GenerarEventos();
		GenerarCofres();
		GenerarBraseros();
		GenerarEnemigos();
	}

	// ==========================================
	// CONSULTAS DE LA CUADRÍCULA
	// ==========================================

		static bool InBounds(int x, int y) { return x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE; }

		bool BloqueaVision(int x, int y) {
			const Cell& c = map_data[x][y];
			return c.is_wall || (c.is_door && !c.door_open);
		}

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

		// ¿Puede alguien estar de pie aquí? (sin mirar a los héroes)
		bool EsSueloLibre(int x, int y) {
			if (!InBounds(x, y)) return false;
			const Cell& c = map_data[x][y];
			return !c.is_wall && !(c.is_door && !c.door_open) && !c.is_chest && !c.has_enemy && !c.is_brasero;
		}

		bool EventoManualActivo(const Cell& c) {
			return c.has_event && c.event_idx >= 0 && m_eventos[c.event_idx].activo && m_eventos[c.event_idx].gatillo == 0;
		}

		Array ListaEnemigos() {
			Array out;
			for (int x = 0; x < GRID_SIZE; x++)
				for (int y = 0; y < GRID_SIZE; y++)
					if (map_data[x][y].has_enemy) out.push_back(Vector2i(x, y));
			return out;
		}

	// ==========================================
	// VISIÓN Y NIEBLA DE GUERRA
	// ==========================================

	// Recalcula qué casillas se ven (y marca las exploradas) a partir de los focos de luz
	void CalcularVision(const std::vector<Foco>& focos) {
		if (map_data.empty()) return;
			for (int x = 0; x < GRID_SIZE; x++)
				for (int y = 0; y < GRID_SIZE; y++) {
					map_data[x][y].is_visible = false;
					map_data[x][y].is_lit = false;
				}

			// Cada foco (héroe en el mapa) ilumina su radio de luz (con línea de visión)
			for (const Foco& f : focos) {
				Vector2i pos = f.pos;
				int r = f.radio;
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
	}

	// Estado de niebla por casilla para el renderer: 0 = sin explorar, 1 = explorada, 2 = visible
	PackedByteArray EstadosNiebla(bool revelar_todo) const {
		PackedByteArray estados;
		estados.resize(GRID_SIZE * GRID_SIZE);
		for (int x = 0; x < GRID_SIZE; x++)
			for (int y = 0; y < GRID_SIZE; y++) {
				const Cell& c = map_data[x][y];
				estados[x * GRID_SIZE + y] = (revelar_todo || c.is_visible) ? 2 : (c.is_explored ? 1 : 0);
			}
		return estados;
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

private:
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
};

} // namespace mazmorra
