#pragma once
// Mazmorra: the logic grid of a floor (what used to be the DungeonManager globals). It knows how to
// generate itself (rooms, corridors, doors, events, chests, braziers, enemies and decoration),
// compute vision/fog and answer grid queries. It doesn't know any Godot nodes: the DungeonManager
// passes it what it needs (e.g. the heroes' light sources).

#include "Scripts/CPP/Mazmorra/Alturas.hpp"
#include <Godot/godot.hpp>
#include <Godot/classes/node.hpp>
#include <Godot/classes/resource_loader.hpp>
#include <Godot/classes/resource.hpp>
#include <Godot/classes/dir_access.hpp>
#include <Godot/variant/utility_functions.hpp>
#include <algorithm>
#include <cmath>
#include <vector>
#include <queue>

namespace mazmorra {

using namespace godot;

const int GRID_SIZE = 40; // Dungeon with zones: entrance, camps and boss room
const int RADIO_BRASERO = 3;
// What a hero sees = their light (radio_luz) + VISION_EXTRA tiles. Without a torch,
// ANTORCHA_APAGADA. Enemies always see less than them (Sigilo.gd: their sight minus 2): you see
// them first.
const int VISION_EXTRA = 3;
const int ANTORCHA_APAGADA = 2;
// Vision radius of a hero (fog, shadows and the light the shader paints)
inline int RadioVision(godot::Node* h) {
	int radio = (h && h->has_meta("radio_luz") ? (int)h->get_meta("radio_luz") : 4) + VISION_EXTRA;
	if (h && h->has_meta("antorcha_apagada") && (bool)h->get_meta("antorcha_apagada")) radio = std::min(radio, ANTORCHA_APAGADA);
	return radio;
}
const Vector2i DIRS4[4] = { Vector2i(0, -1), Vector2i(0, 1), Vector2i(-1, 0), Vector2i(1, 0) };

// Centre of the tile, at the height of its floor (high rooms and steps)
inline Vector3 GridToWorld(Vector2i c) { return Vector3(c.x + 0.5f, alturas::Y(c.x, c.y), c.y + 0.5f); }

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
	bool is_brasero = false; // Static obstacle that lights its room
	bool is_pillar = false;  // Column inside a room: it's a wall (you can't walk or see through it), but it's drawn thin
	bool is_locked = false;  // Boss room door: only opens with the key
	int peloton = -1;        // Room (camp/boss) the enemy spawning here belongs to
	int nivel = 0;           // 0 lower floor, 1 step (joins the two heights), 2 high room
	int event_idx = -1;
};

// Struct to define and handle rooms
struct Room {
	int x, y, w, h;
	// Optional wing (L or T rooms): another rectangle stuck to one side. aw = 0 -> none
	int ax = 0, ay = 0, aw = 0, ah = 0;
	bool elevada = false; // High room (level 2): you go up by steps
	int tipo = 0;         // SALA_NORMAL, SALA_ENTRADA, SALA_CAMPAMENTO, SALA_JEFE
	int zona = -1;        // Camps: 0 the closest to the entrance, 1 the next one...
	int dist = 0;         // Walking tiles from the entrance
	Vector2i cofre = Vector2i(-1, -1);

	Vector2i get_center() const { return Vector2i(x + w / 2, y + h / 2); }

	// Is the point inside the room or its wing?
	bool contains(int px, int py) const {
		return (px >= x && px < x + w && py >= y && py < y + h) ||
			   (aw > 0 && px >= ax && px < ax + aw && py >= ay && py < ay + ah);
	}

	// Rectangle that contains everything (room + wing)
	Rect2i caja() const {
		Rect2i r(x, y, w, h);
		if (aw > 0) r = r.merge(Rect2i(ax, ay, aw, ah));
		return r;
	}

	// They touch (with 1 tile of margin: there's always a wall between two rooms)
	bool intersects(const Room& other) const {
		Rect2i a = caja().grow(1), b = other.caja();
		return a.intersects(b);
	}
};

enum TipoSala { SALA_NORMAL = 0, SALA_ENTRADA = 1, SALA_CAMPAMENTO = 2, SALA_JEFE = 3 };
// Room type name for the scripts (Zonas.gd)
inline const char* NombreTipoSala(int t) {
	return t == SALA_ENTRADA ? "entrada" : (t == SALA_CAMPAMENTO ? "campamento" : (t == SALA_JEFE ? "jefe" : "sala"));
}

// Event data, read from the .tres ONCE when generating the floor
struct EventInfo {
	Vector2i pos;
	String path;
	int gatillo = 0; // 0 = MANUAL, 1 = PROXIMIDAD, 2 = CONDICIONAL
	int radio = 3;
	String stat = "ninguno";
	int critico = 30;
	bool activo = true;
};

// A point that lights around it (a hero on the map)
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
	int m_piso = 1; // The DungeonManager sets it before generating: more rooms on the lower floors
	// The floor structure (like an Elden Ring cave): entrance with a campfire, camps with their
	// squad and their chest, and at the back the boss room, locked with a key, with the stairs
	// behind
	Vector2i m_hoguera = Vector2i(-1, -1);
	Vector2i m_jefe = Vector2i(-1, -1);
	Vector2i m_cofre_llave = Vector2i(-1, -1);
	Vector2i m_cofre_jefe = Vector2i(-1, -1);
	int m_sala_jefe = -1;
	std::vector<Vector2i> m_puertas_jefe;

	// The heights of every tile, for the rest of the scripts (alturas::Nivel / alturas::Y)
	void PublicarAlturas() {
		PackedByteArray niveles;
		niveles.resize(GRID_SIZE * GRID_SIZE);
		for (int x = 0; x < GRID_SIZE; x++)
			for (int y = 0; y < GRID_SIZE; y++) niveles[x * GRID_SIZE + y] = map_data.empty() ? 0 : (uint8_t)map_data[x][y].nivel;
		alturas::Publicar(niveles, GRID_SIZE);
	}

	bool vacia() const { return map_data.empty(); }

	// Generates a whole floor from scratch
	void GenerarPiso() {
		GenerarMapa();
		PublicarAlturas();
		GenerarPuertas();
		DefinirInicioYSalida();
		AsignarZonas();
		GenerarCofres();
		GenerarBraseros();
		GenerarEnemigos();
		GenerarEventos();
	}

	// ==========================================
	// GRID QUERIES
	// ==========================================

		static bool InBounds(int x, int y) { return x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE; }

		// Walls, columns and closed doors block sight
		bool BloqueaVision(int x, int y) {
			const Cell& c = map_data[x][y];
			return c.is_wall || (c.is_door && !c.door_open);
		}

		// Bresenham between two tiles; the GDScript copy in Tactica.linea_de_vision must match it exactly
		bool CheckLineOfSight(Vector2i start, Vector2i end) {
			int x0 = start.x, y0 = start.y, x1 = end.x, y1 = end.y;
			int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
			int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
			int err = dx + dy;

			while (true) {
				if (!InBounds(x0, y0)) return false;

				// If we hit an obstacle we can see it, but vision stops there
				if (BloqueaVision(x0, y0) && !(x0 == start.x && y0 == start.y)) {
					return (x0 == x1 && y0 == y1);
				}
				if (x0 == x1 && y0 == y1) break;

				int e2 = 2 * err;
				int prev_x = x0, prev_y = y0;
				if (e2 >= dy) { err += dy; x0 += sx; }
				if (e2 <= dx) { err += dx; y0 += sy; }

				// Strict corner check (diagonal step)
				if (x0 != prev_x && y0 != prev_y) {
					bool wall1 = InBounds(x0, prev_y) && BloqueaVision(x0, prev_y);
					bool wall2 = InBounds(prev_x, y0) && BloqueaVision(prev_x, y0);
					if (wall1 || wall2) return false;
				}
			}
			return true;
		}

		// THE game's vision rule, identical to the shadow shader (ps1_world.gdshader, rayo()): a
		// ray from 'o' (the viewer) to 'p' (the viewed), in world coordinates, that walks exactly
		// the tiles it crosses (DDA). Walls, columns and closed doors cut it; the viewed tile and
		// the viewer's tile don't count. So a tile is "in view" if and only if its centre comes out
		// lit on screen.
		bool TapaVista(Vector2i c) {
			return !InBounds(c.x, c.y) || BloqueaVision(c.x, c.y);
		}

		bool RayoLibre(Vector2 o, Vector2 p) {
			Vector2 d = o - p;
			float largo = d.length();
			if (largo < 0.05f) return true;
			Vector2 dir = d / largo;
			Vector2i propia((int)std::floor(p.x), (int)std::floor(p.y));
			Vector2i destino((int)std::floor(o.x), (int)std::floor(o.y));
			Vector2 q = p + dir * 0.03f;
			Vector2i c((int)std::floor(q.x), (int)std::floor(q.y));
			if (c != propia && c != destino && TapaVista(c)) return false;
			Vector2i paso(dir.x > 0 ? 1 : (dir.x < 0 ? -1 : 0), dir.y > 0 ? 1 : (dir.y < 0 ? -1 : 0));
			Vector2 inv(1.0f / std::max(std::abs(dir.x), 1e-5f), 1.0f / std::max(std::abs(dir.y), 1e-5f));
			Vector2 t_max((dir.x > 0.0f ? (float)(c.x + 1) - q.x : q.x - (float)c.x) * inv.x,
						  (dir.y > 0.0f ? (float)(c.y + 1) - q.y : q.y - (float)c.y) * inv.y);
			for (int i = 0; i < 64; i++) {
				float t;
				if (t_max.x < t_max.y) { c.x += paso.x; t = t_max.x; t_max.x += inv.x; }
				else { c.y += paso.y; t = t_max.y; t_max.y += inv.y; }
				if (t > largo || c == destino) break;
				if (c != propia && TapaVista(c)) return false;
			}
			return true;
		}

		// Does the light source (centre of its tile, radius in tiles) see the centre of this tile?
		bool VeCasilla(Vector2i foco, float radio, Vector2i c) {
			Vector2 o(foco.x + 0.5f, foco.y + 0.5f), p(c.x + 0.5f, c.y + 0.5f);
			return (o - p).length() <= radio && RayoLibre(o, p);
		}

		// Can someone stand here? (ignoring the heroes)
		bool EsSueloLibre(int x, int y) {
			if (!InBounds(x, y)) return false;
			const Cell& c = map_data[x][y];
			return !c.is_wall && !(c.is_door && !c.door_open) && !c.is_chest && !c.has_enemy && !c.is_brasero;
		}

		// A manual event that's still there (investigated by clicking)
		bool EventoManualActivo(const Cell& c) {
			return c.has_event && c.event_idx >= 0 && m_eventos[c.event_idx].activo && m_eventos[c.event_idx].gatillo == 0;
		}

		// Tiles of every living enemy (the DungeonManager publishes it as a meta)
		Array ListaEnemigos() {
			Array out;
			for (int x = 0; x < GRID_SIZE; x++)
				for (int y = 0; y < GRID_SIZE; y++)
					if (map_data[x][y].has_enemy) out.push_back(Vector2i(x, y));
			return out;
		}

	// ==========================================
	// VISION AND FOG OF WAR
	// ==========================================

	// Recomputes which tiles are seen (and marks the explored ones) from the light sources
	void CalcularVision(const std::vector<Foco>& focos) {
		if (map_data.empty()) return;
			for (int x = 0; x < GRID_SIZE; x++)
				for (int y = 0; y < GRID_SIZE; y++) {
					map_data[x][y].is_visible = false;
					map_data[x][y].is_lit = false;
				}

			// Every light source (hero on the map) lights its light radius (with line of sight)
			for (const Foco& f : focos) {
				Vector2i pos = f.pos;
				int r = f.radio;
				for (int x = std::max(0, pos.x - r); x <= std::min(GRID_SIZE - 1, pos.x + r); x++) {
					for (int y = std::max(0, pos.y - r); y <= std::min(GRID_SIZE - 1, pos.y + r); y++) {
						if (!VeCasilla(pos, r + 0.5f, Vector2i(x, y))) continue; // = the shader
						map_data[x][y].is_visible = true;
						map_data[x][y].is_lit = true;
						map_data[x][y].is_explored = true;
					}
				}
			}

			// If a brazier is seen, whatever it lights is seen too
			for (const Vector2i& b : m_braseros) {
				if (!map_data[b.x][b.y].is_visible) continue;
				for (int x = std::max(0, b.x - RADIO_BRASERO); x <= std::min(GRID_SIZE - 1, b.x + RADIO_BRASERO); x++)
					for (int y = std::max(0, b.y - RADIO_BRASERO); y <= std::min(GRID_SIZE - 1, b.y + RADIO_BRASERO); y++) {
						if (!VeCasilla(b, RADIO_BRASERO + 0.5f, Vector2i(x, y))) continue; // = the shader
						map_data[x][y].is_visible = true;
						map_data[x][y].is_lit = true;
						map_data[x][y].is_explored = true;
					}
			}

			// The walls around the visible floor are seen too
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

	// Fog state per tile for the renderer: 0 = unexplored, 1 = explored, 2 = visible
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

		// Triplets (x, y, type): 0 = cobweb (corner), 1 = bones, 2 = skull, 3 = rocks, 4 = stones
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
						if (!muro_x && !muro_y) continue; // Only against the wall

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
		// PROCEDURAL GENERATION
		// ==========================================

		// ==========================================
		// ROOM AND CORRIDOR GENERATOR
		// ==========================================
		// Big rooms (5-9 per side), some in L or T shape and with cut corners; corridors 1 to 3
		// tiles wide with the odd bend; a tree that joins them all plus some loop (two ways to the
		// same room); and columns in the big rooms, which never cut anything off.

		// Turns a tile into floor (never the outer border)
		void Excavar(int x, int y) {
			if (x >= 1 && y >= 1 && x < GRID_SIZE - 1 && y < GRID_SIZE - 1) map_data[x][y].is_wall = false;
		}

		// Digs a whole rectangle
		void ExcavarRect(const Rect2i& r) {
			for (int x = r.position.x; x < r.position.x + r.size.x; x++)
				for (int y = r.position.y; y < r.position.y + r.size.y; y++) Excavar(x, y);
		}

		// A straight stretch of corridor 'ancho' tiles wide
		void ExcavarTramo(Vector2i a, Vector2i b, int ancho) {
			int x0 = std::min(a.x, b.x), x1 = std::max(a.x, b.x);
			int y0 = std::min(a.y, b.y), y1 = std::max(a.y, b.y);
			for (int x = x0; x <= x1 + ancho - 1; x++)
				for (int y = y0; y <= y1 + ancho - 1; y++) Excavar(x, y);
		}

		// L corridor (with an optional bend halfway) between two points
		void ExcavarPasillo(Vector2i a, Vector2i b, int ancho) {
			Vector2i medio = a;
			if (UtilityFunctions::randi_range(0, 99) < 45) {
				medio = Vector2i(UtilityFunctions::randi_range(std::min(a.x, b.x), std::max(a.x, b.x)),
								 UtilityFunctions::randi_range(std::min(a.y, b.y), std::max(a.y, b.y)));
			}
			auto en_l = [&](Vector2i p, Vector2i q) {
				Vector2i codo = UtilityFunctions::randi_range(0, 1) == 0 ? Vector2i(q.x, p.y) : Vector2i(p.x, q.y);
				ExcavarTramo(p, codo, ancho);
				ExcavarTramo(codo, q, ancho);
			};
			en_l(a, medio);
			en_l(medio, b);
		}

		// Random corridor width: 40% narrow, 45% medium, 15% wide
		int AnchoDePasillo() {
			int r = UtilityFunctions::randi_range(0, 99);
			return r < 40 ? 1 : (r < 85 ? 2 : 3); // The 1-wide ones are the ones that can have a door
		}

		// Can every floor tile be reached from any other? (flood fill)
		bool TodoConectado() {
			Vector2i inicio(-1, -1);
			int suelos = 0;
			for (int x = 0; x < GRID_SIZE; x++)
				for (int y = 0; y < GRID_SIZE; y++)
					if (!map_data[x][y].is_wall) { suelos++; if (inicio.x < 0) inicio = Vector2i(x, y); }
			if (inicio.x < 0) return true;
			std::vector<std::vector<bool>> visto(GRID_SIZE, std::vector<bool>(GRID_SIZE, false));
			std::queue<Vector2i> cola;
			cola.push(inicio);
			visto[inicio.x][inicio.y] = true;
			int alcanzados = 0;
			while (!cola.empty()) {
				Vector2i p = cola.front(); cola.pop();
				alcanzados++;
				for (const Vector2i& d : DIRS4) {
					Vector2i n = p + d;
					if (InBounds(n.x, n.y) && !visto[n.x][n.y] && !map_data[n.x][n.y].is_wall) { visto[n.x][n.y] = true; cola.push(n); }
				}
			}
			return alcanzados == suelos;
		}

		// Rooms, corridors, heights and columns (the floor layout)
		void GenerarMapa() {
			// Black canvas: we reset the WHOLE matrix (doors and enemies from the previous floor
			// used to stay)
			map_data.assign(GRID_SIZE, std::vector<Cell>(GRID_SIZE));
			m_rooms.clear();

			// Floor 1: 9 rooms; one more every two floors, up to 12. Big ones: the camps need space
			const int MAX_ROOMS = std::clamp(9 + (m_piso - 1) / 2, 9, 12), MIN_SIZE = 5, MAX_SIZE = 10, MAX_ATTEMPTS = 900;

			// 1) Rooms (some with a wing: L or T shape)
			for (int i = 0; i < MAX_ATTEMPTS && (int)m_rooms.size() < MAX_ROOMS; i++) {
				int tope = i > 450 ? MAX_SIZE - 3 : MAX_SIZE; // If they're hard to fit, smaller
				int w = UtilityFunctions::randi_range(MIN_SIZE + 1, tope);
				int h = UtilityFunctions::randi_range(MIN_SIZE, tope);
				Room nueva = { UtilityFunctions::randi_range(2, GRID_SIZE - w - 3), UtilityFunctions::randi_range(2, GRID_SIZE - h - 3), w, h };
				if (UtilityFunctions::randi_range(0, 99) < 35) {
					int aw = UtilityFunctions::randi_range(3, 4), ah = UtilityFunctions::randi_range(3, 4);
					switch (UtilityFunctions::randi_range(0, 3)) {
						case 0: nueva.ax = nueva.x + nueva.w; nueva.ay = nueva.y + UtilityFunctions::randi_range(0, std::max(0, h - ah)); break;
						case 1: nueva.ax = nueva.x - aw; nueva.ay = nueva.y + UtilityFunctions::randi_range(0, std::max(0, h - ah)); break;
						case 2: nueva.ay = nueva.y + nueva.h; nueva.ax = nueva.x + UtilityFunctions::randi_range(0, std::max(0, w - aw)); break;
						default: nueva.ay = nueva.y - ah; nueva.ax = nueva.x + UtilityFunctions::randi_range(0, std::max(0, w - aw)); break;
					}
					nueva.aw = aw; nueva.ah = ah;
					Rect2i caja = nueva.caja();
					if (caja.position.x < 2 || caja.position.y < 2 || caja.get_end().x > GRID_SIZE - 2 || caja.get_end().y > GRID_SIZE - 2)
						nueva.aw = nueva.ah = 0; // The wing goes off the map: normal room
				}
				bool choca = false;
				for (const Room& otra : m_rooms)
					if (nueva.intersects(otra)) { choca = true; break; }
				if (choca) continue;

				ExcavarRect(Rect2i(nueva.x, nueva.y, nueva.w, nueva.h));
				if (nueva.aw > 0) ExcavarRect(Rect2i(nueva.ax, nueva.ay, nueva.aw, nueva.ah));
				// Cut corners in the big rooms (so they don't look like boxes)
				if (w >= 6 && h >= 6) {
					const Vector2i esquinas[4] = { Vector2i(nueva.x, nueva.y), Vector2i(nueva.x + w - 1, nueva.y),
												   Vector2i(nueva.x, nueva.y + h - 1), Vector2i(nueva.x + w - 1, nueva.y + h - 1) };
					for (const Vector2i& e : esquinas)
						if (UtilityFunctions::randi_range(0, 99) < 50) map_data[e.x][e.y].is_wall = true;
				}
				m_rooms.push_back(nueva);
			}

			// 2) Corridors: minimum spanning tree (every room with the closest one already
			// joined)...
			int n = (int)m_rooms.size();
			auto dist = [&](int a, int b) {
				Vector2i ca = m_rooms[a].get_center(), cb = m_rooms[b].get_center();
				return std::abs(ca.x - cb.x) + std::abs(ca.y - cb.y);
			};
			std::vector<std::pair<int, int>> uniones;
			std::vector<bool> unida(n, false);
			if (n > 0) unida[0] = true;
			for (int paso = 1; paso < n; paso++) {
				int mejor_a = -1, mejor_b = -1, mejor_d = 1 << 30;
				for (int a = 0; a < n; a++) {
					if (!unida[a]) continue;
					for (int b = 0; b < n; b++)
						if (!unida[b] && dist(a, b) < mejor_d) { mejor_d = dist(a, b); mejor_a = a; mejor_b = b; }
				}
				if (mejor_b < 0) break;
				unida[mejor_b] = true;
				uniones.push_back({ mejor_a, mejor_b });
			}
			// ...and some loop: 1 or 2 extra links between rooms that weren't joined
			int extras = n >= 6 ? UtilityFunctions::randi_range(1, 2) : 1;
			for (int intento = 0; intento < 40 && extras > 0 && n > 2; intento++) {
				int a = UtilityFunctions::randi_range(0, n - 1), b = UtilityFunctions::randi_range(0, n - 1);
				if (a == b || dist(a, b) > GRID_SIZE) continue;
				bool ya = false;
				for (auto& u : uniones) if ((u.first == a && u.second == b) || (u.first == b && u.second == a)) ya = true;
				if (ya) continue;
				uniones.push_back({ a, b });
				extras--;
			}
			for (auto& u : uniones) ExcavarPasillo(m_rooms[u.first].get_center(), m_rooms[u.second].get_center(), AnchoDePasillo());

			// 3) Two heights: some whole room is up high (never the starting one). You go up by
			// steps: every lower floor next to a higher one is a step, so you only change height
			// through them (the pathfinding doesn't need to know anything else)
			int altas = 0;
			if (n >= 3) {
				int quiero = std::max(1, n / 3);
				for (int intento = 0; intento < 20 && altas < quiero; intento++) {
					int k = UtilityFunctions::randi_range(1, n - 1);
					Room& r = m_rooms[k];
					if (r.elevada) continue;
					r.elevada = true;
					altas++;
				}
				for (const Room& r : m_rooms) {
					if (!r.elevada) continue;
					Rect2i c = r.caja();
					for (int x = c.position.x; x < c.get_end().x; x++)
						for (int y = c.position.y; y < c.get_end().y; y++)
							if (r.contains(x, y) && !map_data[x][y].is_wall) map_data[x][y].nivel = alturas::ALTO;
				}
				for (int x = 1; x < GRID_SIZE - 1; x++)
					for (int y = 1; y < GRID_SIZE - 1; y++) {
						Cell& c = map_data[x][y];
						if (c.is_wall || c.nivel != 0) continue;
						for (const Vector2i& d : DIRS4)
							if (map_data[x + d.x][y + d.y].nivel == alturas::ALTO && !map_data[x + d.x][y + d.y].is_wall) { c.nivel = alturas::ESCALON; break; }
					}
			}

			// 4) Columns in the big rooms: on a grid, away from the walls and the centre, and only
			// if they don't cut anything off
			int columnas = 0;
			for (const Room& r : m_rooms) {
				if (r.w < 7 || r.h < 6 || UtilityFunctions::randi_range(0, 99) >= 75) continue;
				Vector2i c = r.get_center();
				for (int x = r.x + 2; x < r.x + r.w - 2; x += 3)
					for (int y = r.y + 2; y < r.y + r.h - 2; y += 3) {
						if (std::abs(x - c.x) + std::abs(y - c.y) <= 1 || map_data[x][y].is_wall) continue;
						map_data[x][y].is_wall = true;
						map_data[x][y].is_pillar = true;
						if (!TodoConectado()) { map_data[x][y].is_wall = false; map_data[x][y].is_pillar = false; }
						else columnas++;
					}
			}
			UtilityFunctions::print("[DungeonManager] Mapa generado: ", (int)m_rooms.size(), " salas (", altas, " altas), ", (int)uniones.size(), " pasillos, ", columnas, " columnas.");
		}

		// Is the tile inside any room?
		bool EnAlgunaSala(int x, int y) {
			for (const Room& r : m_rooms) if (r.contains(x, y)) return true;
			return false;
		}

		// Doors on narrow corridor mouths, spread out so they don't pile up
		void GenerarPuertas() {
			const int DISTANCIA_MINIMA = 6;
			const int PROBABILIDAD_PUERTA = 40;

			for (int x = 1; x < GRID_SIZE - 1; x++) {
				for (int y = 1; y < GRID_SIZE - 1; y++) {
					if (map_data[x][y].is_wall || EnAlgunaSala(x, y) || map_data[x][y].nivel == alturas::ESCALON) continue;

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

		// Spawn in the first room; exit in one of the furthest rooms (random among the top 35%)
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

			// No doors next to the spawn or at the exit
			for (int dx = -2; dx <= 2; dx++)
				for (int dy = -2; dy <= 2; dy++)
					if (InBounds(m_inicio.x + dx, m_inicio.y + dy)) map_data[m_inicio.x + dx][m_inicio.y + dy].is_door = false;
			map_data[m_salida.x][m_salida.y].is_door = false;

			// Spawn tiles: the 4 closest to the centre of the starting room
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

		// Within 'radio' tiles of the spawn?
		bool CercaDelInicio(Vector2i p, int radio) {
			return std::abs(p.x - m_inicio.x) + std::abs(p.y - m_inicio.y) <= radio;
		}

		// Is there a door next to this tile?
		bool PegadoAPuerta(int x, int y) {
			for (const Vector2i& d : DIRS4)
				if (InBounds(x + d.x, y + d.y) && map_data[x + d.x][y + d.y].is_door) return true;
			return false;
		}

		// Empty tile, far from the spawn and the exit, without blocking doors
		bool CasillaDeSalaValida(int x, int y) {
			const Cell& c = map_data[x][y];
			if (c.is_wall || c.is_door || c.is_chest || c.has_event || c.has_enemy || c.is_brasero || c.nivel == alturas::ESCALON) return false;
			if (Vector2i(x, y) == m_salida || CercaDelInicio(Vector2i(x, y), 3) || PegadoAPuerta(x, y)) return false;
			return true;
		}

		// ==========================================
		// ZONES: entrance, camps and boss room
		// ==========================================

		// Walking tiles from 'origen' (-1 = unreachable). Closed doors can be crossed (they can be
		// opened) except the key ones if 'llave_bloquea'; 'sala_vetada' is never stepped on.
		std::vector<std::vector<int>> DistanciasDesde(Vector2i origen, bool llave_bloquea, int sala_vetada = -1) {
			std::vector<std::vector<int>> d(GRID_SIZE, std::vector<int>(GRID_SIZE, -1));
			if (!InBounds(origen.x, origen.y) || map_data[origen.x][origen.y].is_wall) return d;
			std::queue<Vector2i> cola;
			cola.push(origen);
			d[origen.x][origen.y] = 0;
			while (!cola.empty()) {
				Vector2i p = cola.front(); cola.pop();
				for (const Vector2i& dir : DIRS4) {
					Vector2i n = p + dir;
					if (!InBounds(n.x, n.y) || d[n.x][n.y] >= 0) continue;
					const Cell& c = map_data[n.x][n.y];
					if (c.is_wall || (llave_bloquea && c.is_locked)) continue;
					if (sala_vetada >= 0 && m_rooms[sala_vetada].contains(n.x, n.y)) continue;
					d[n.x][n.y] = d[p.x][p.y] + 1;
					cola.push(n);
				}
			}
			return d;
		}

		// The closest you can get to a room (-1 = none of its tiles)
		int DistanciaASala(const std::vector<std::vector<int>>& d, int i) {
			const Room& r = m_rooms[i];
			Rect2i c = r.caja();
			int mejor = -1;
			for (int x = c.position.x; x < c.get_end().x; x++)
				for (int y = c.position.y; y < c.get_end().y; y++)
					if (r.contains(x, y) && d[x][y] >= 0 && (mejor < 0 || d[x][y] < mejor)) mejor = d[x][y];
			return mejor;
		}

		// The room tiles you come in through (next to a floor outside)
		std::vector<Vector2i> EntradasDeSala(int i) {
			const Room& r = m_rooms[i];
			Rect2i c = r.caja();
			std::vector<Vector2i> out;
			for (int x = c.position.x; x < c.get_end().x; x++)
				for (int y = c.position.y; y < c.get_end().y; y++) {
					if (!r.contains(x, y) || map_data[x][y].is_wall) continue;
					for (const Vector2i& dir : DIRS4) {
						Vector2i n(x + dir.x, y + dir.y);
						if (InBounds(n.x, n.y) && !map_data[n.x][n.y].is_wall && !r.contains(n.x, n.y)) { out.push_back(Vector2i(x, y)); break; }
					}
				}
			return out;
		}

		// Closes the boss room: every opening gets walled off except one tile, which becomes a
		// locked door. If something ends up cut off, everything is undone (the floor has no key,
		// but it's still playable).
		bool SellarSalaJefe(int i) {
			std::vector<Vector2i> entradas = EntradasDeSala(i);
			if (entradas.empty()) return false;
			std::vector<std::pair<Vector2i, Cell>> antes;
			std::vector<bool> usado(entradas.size(), false);
			for (size_t k = 0; k < entradas.size(); k++) {
				if (usado[k]) continue;
				// Group of adjacent entrances (the mouth of a wide corridor)
				std::vector<Vector2i> grupo;
				std::vector<size_t> pila = { k };
				usado[k] = true;
				while (!pila.empty()) {
					size_t a = pila.back(); pila.pop_back();
					grupo.push_back(entradas[a]);
					for (size_t b = 0; b < entradas.size(); b++)
						if (!usado[b] && std::abs(entradas[a].x - entradas[b].x) <= 1 && std::abs(entradas[a].y - entradas[b].y) <= 1) { usado[b] = true; pila.push_back(b); }
				}
				std::sort(grupo.begin(), grupo.end(), [](const Vector2i& p, const Vector2i& q) { return p.x != q.x ? p.x < q.x : p.y < q.y; });
				Vector2i puerta = grupo[grupo.size() / 2];
				for (const Vector2i& p : grupo) {
					antes.push_back({ p, map_data[p.x][p.y] });
					Cell& c = map_data[p.x][p.y];
					if (p == puerta) { c.is_door = true; c.door_open = false; c.is_locked = true; m_puertas_jefe.push_back(p); }
					else { c.is_wall = true; c.is_door = false; }
				}
			}
			// Check: without the key you reach every other room but not the boss one; with the key,
			// that one too
			auto sin_llave = DistanciasDesde(m_inicio, true);
			auto con_llave = DistanciasDesde(m_inicio, false);
			bool ok = DistanciaASala(sin_llave, i) < 0 && DistanciaASala(con_llave, i) >= 0;
			for (size_t k = 0; k < m_rooms.size() && ok; k++)
				if ((int)k != i && DistanciaASala(sin_llave, (int)k) < 0) ok = false;
			// And inside the room, all its floor is still connected to the door
			if (ok) {
				const Room& r = m_rooms[i];
				Rect2i c = r.caja();
				for (int x = c.position.x; x < c.get_end().x && ok; x++)
					for (int y = c.position.y; y < c.get_end().y; y++)
						if (r.contains(x, y) && !map_data[x][y].is_wall && con_llave[x][y] < 0) { ok = false; break; }
			}
			if (!ok) {
				for (auto& par : antes) map_data[par.first.x][par.first.y] = par.second;
				m_puertas_jefe.clear();
			}
			return ok;
		}

		// Gives every room its role: entrance, camps and the boss room
		void AsignarZonas() {
			m_hoguera = m_jefe = m_cofre_llave = m_cofre_jefe = Vector2i(-1, -1);
			m_sala_jefe = -1;
			m_puertas_jefe.clear();
			int n = (int)m_rooms.size();
			for (Room& r : m_rooms) { r.tipo = SALA_NORMAL; r.zona = -1; r.cofre = Vector2i(-1, -1); }
			if (n == 0) return;
			m_rooms[0].tipo = SALA_ENTRADA;
			auto d = DistanciasDesde(m_inicio, false);
			for (int i = 0; i < n; i++) m_rooms[i].dist = std::max(0, DistanciaASala(d, i));
			if (n < 3) return;

			// 1) The boss room: the furthest one that, if removed, doesn't leave anyone isolated
			// (big is better)
			std::vector<int> orden;
			for (int i = 1; i < n; i++) orden.push_back(i);
			std::sort(orden.begin(), orden.end(), [&](int a, int b) { return m_rooms[a].dist > m_rooms[b].dist; });
			for (int pasada = 0; pasada < 2 && m_sala_jefe < 0; pasada++)
				for (int i : orden) {
					if (pasada == 0 && m_rooms[i].w * m_rooms[i].h < 30) continue;
					auto sin = DistanciasDesde(m_inicio, false, i);
					bool aisla = false;
					for (int k = 1; k < n && !aisla; k++)
						if (k != i && DistanciaASala(sin, k) < 0) aisla = true;
					if (aisla) continue;
					m_sala_jefe = i;
					break;
				}
			if (m_sala_jefe < 0) m_sala_jefe = orden.front();
			Room& jefe = m_rooms[m_sala_jefe];
			jefe.tipo = SALA_JEFE;
			SellarSalaJefe(m_sala_jefe);

			// The stairs, at the back of the boss room (as far as possible from the door)
			auto dj = DistanciasDesde(m_inicio, false);
			Rect2i cj = jefe.caja();
			int mejor = -1;
			for (int x = cj.position.x; x < cj.get_end().x; x++)
				for (int y = cj.position.y; y < cj.get_end().y; y++) {
					if (!jefe.contains(x, y) || map_data[x][y].is_wall || map_data[x][y].is_door || PegadoAPuerta(x, y) || dj[x][y] < 0) continue;
					if (map_data[x][y].nivel == alturas::ESCALON) continue;
					if (dj[x][y] > mejor) { mejor = dj[x][y]; m_salida = Vector2i(x, y); }
				}
			map_data[m_salida.x][m_salida.y].is_door = false;

			// 2) Camps: the biggest rooms left (keeping a couple for events), numbered from the
			// entrance to the back
			std::vector<int> libres;
			for (int i = 1; i < n; i++) if (m_rooms[i].tipo == SALA_NORMAL) libres.push_back(i);
			std::sort(libres.begin(), libres.end(), [&](int a, int b) { return m_rooms[a].w * m_rooms[a].h > m_rooms[b].w * m_rooms[b].h; });
			int k = (int)libres.size() <= 3 ? std::max(1, (int)libres.size() - 1) : std::clamp((int)libres.size() - 2, 2, 4);
			std::vector<int> campamentos(libres.begin(), libres.begin() + std::min(k, (int)libres.size()));
			std::sort(campamentos.begin(), campamentos.end(), [&](int a, int b) { return m_rooms[a].dist < m_rooms[b].dist; });
			for (int z = 0; z < (int)campamentos.size(); z++) {
				m_rooms[campamentos[z]].tipo = SALA_CAMPAMENTO;
				m_rooms[campamentos[z]].zona = z;
			}
			UtilityFunctions::print("[Mazmorra] Zonas: jefe en la sala ", m_sala_jefe, (m_puertas_jefe.empty() ? " (sin sellar)" : " (sellada)"), ", ", (int)campamentos.size(), " campamentos.");
		}

		// The free tile of the room closest to 'centro' that passes 'vale'
		template <typename F>
		Vector2i CasillaCercana(const Room& r, Vector2i centro, F vale) {
			Rect2i c = r.caja();
			Vector2i mejor(-1, -1);
			int md = 1 << 30;
			for (int x = c.position.x; x < c.get_end().x; x++)
				for (int y = c.position.y; y < c.get_end().y; y++) {
					if (!r.contains(x, y) || !vale(x, y)) continue;
					int dd = std::abs(x - centro.x) + std::abs(y - centro.y) + UtilityFunctions::randi_range(0, 1);
					if (dd < md) { md = dd; mejor = Vector2i(x, y); }
				}
			return mejor;
		}

		// Is there a chest next to this tile?
		bool PegadoACofre(int x, int y) {
			for (const Vector2i& d : DIRS4)
				if (InBounds(x + d.x, y + d.y) && map_data[x + d.x][y + d.y].is_chest) return true;
			return false;
		}

		// Room interior (without the edge, where the corridors come in)
		bool Interior(const Room& r, int x, int y) {
			for (int dx = -1; dx <= 1; dx++)
				for (int dy = -1; dy <= 1; dy++)
					if (!r.contains(x + dx, y + dy)) return false;
			return true;
		}

		// Camp chests, the key, the boss prize and a few hidden ones
		void GenerarCofres() {
			// Every camp keeps its chest, in plain sight, in the centre of the camp
			int ultimo = -1;
			for (size_t i = 0; i < m_rooms.size(); i++) {
				Room& r = m_rooms[i];
				if (r.tipo != SALA_CAMPAMENTO) continue;
				Vector2i p = CasillaCercana(r, r.get_center(), [&](int x, int y) { return Interior(r, x, y) && CasillaDeSalaValida(x, y); });
				if (p.x < 0) continue;
				map_data[p.x][p.y].is_chest = true;
				r.cofre = p;
				if (ultimo < 0 || r.zona > m_rooms[ultimo].zona) ultimo = (int)i;
			}
			// The boss key, in the chest of the deepest camp
			if (ultimo >= 0 && !m_puertas_jefe.empty()) m_cofre_llave = m_rooms[ultimo].cofre;

			// The boss prize, next to the stairs
			if (m_sala_jefe >= 0) {
				Room& j = m_rooms[m_sala_jefe];
				Vector2i p = CasillaCercana(j, m_salida, [&](int x, int y) { return Vector2i(x, y) != m_salida && CasillaDeSalaValida(x, y); });
				if (p.x >= 0) { map_data[p.x][p.y].is_chest = true; m_cofre_jefe = p; j.cofre = p; }
			}

			// And one or two hidden in the unguarded rooms
			std::vector<Vector2i> candidatas;
			for (const Room& r : m_rooms)
				if (r.tipo == SALA_NORMAL)
					for (int x = r.x; x < r.x + r.w; x++)
						for (int y = r.y; y < r.y + r.h; y++)
							if (CasillaDeSalaValida(x, y) && !PegadoACofre(x, y)) candidatas.push_back(Vector2i(x, y));
			int quiero = UtilityFunctions::randi_range(1, 2);
			while (quiero > 0 && !candidatas.empty()) {
				int idx = UtilityFunctions::randi_range(0, (int)candidatas.size() - 1);
				Vector2i p = candidatas[idx];
				candidatas.erase(candidatas.begin() + idx);
				if (!CasillaDeSalaValida(p.x, p.y) || PegadoACofre(p.x, p.y)) continue;
				map_data[p.x][p.y].is_chest = true;
				quiero--;
			}
		}

		// A brazier on that tile (it lights its room)
		void PonerBrasero(Vector2i p) {
			if (p.x < 0) return;
			map_data[p.x][p.y].is_brasero = true;
			m_braseros.push_back(p);
		}

		// Campfire at the entrance, a fire in every camp (seen from far away: there's something to
		// guard) and another one in the boss room. The other rooms, dark.
		void GenerarBraseros() {
			m_braseros.clear();
			// Never on the central row/column of the room, which is where the corridors come in
			auto fuera_del_paso = [&](const Room& r, int x, int y) {
				Vector2i c = r.get_center();
				return x != c.x && y != c.y && Interior(r, x, y) && !PegadoACofre(x, y);
			};
			if (!m_rooms.empty()) {
				const Room& e = m_rooms[0];
				m_hoguera = CasillaCercana(e, m_inicio, [&](int x, int y) {
					const Cell& c = map_data[x][y];
					if (c.is_wall || c.is_door || c.is_chest || c.nivel == alturas::ESCALON || PegadoAPuerta(x, y)) return false;
					for (const Vector2i& s : m_spawn_cells) if (s == Vector2i(x, y)) return false;
					int d = std::abs(x - m_inicio.x) + std::abs(y - m_inicio.y);
					return d >= 2 && fuera_del_paso(e, x, y);
				});
				PonerBrasero(m_hoguera);
			}
			for (const Room& r : m_rooms) {
				if (r.tipo != SALA_CAMPAMENTO && r.tipo != SALA_JEFE) continue;
				Vector2i junto = r.cofre.x >= 0 ? r.cofre : r.get_center();
				Vector2i p = CasillaCercana(r, junto, [&](int x, int y) {
					return CasillaDeSalaValida(x, y) && fuera_del_paso(r, x, y) && std::abs(x - junto.x) + std::abs(y - junto.y) >= 2;
				});
				PonerBrasero(p);
			}
		}

		// An enemy on that tile, belonging to squad 'peloton' (-1 = loose)
		void PonerEnemigo(Vector2i p, int peloton) {
			if (p.x < 0) return;
			map_data[p.x][p.y].has_enemy = true;
			map_data[p.x][p.y].peloton = peloton;
		}

		// Camp squads, the boss with its escort and a few loose prowlers
		void GenerarEnemigos() {
			for (size_t i = 0; i < m_rooms.size(); i++) {
				Room& r = m_rooms[i];
				if (r.tipo == SALA_CAMPAMENTO) {
					// The squad, around its chest: more people in the deeper ones and on the lower
					// floors
					int num = 3 + (r.zona >= 2 ? 1 : 0) + (m_piso >= 3 ? 1 : 0);
					Vector2i centro = r.cofre.x >= 0 ? r.cofre : r.get_center();
					for (int e = 0; e < num; e++) {
						Vector2i p = CasillaCercana(r, centro, [&](int x, int y) {
							int d = std::max(std::abs(x - centro.x), std::abs(y - centro.y));
							return d >= 1 && d <= 3 && Interior(r, x, y) && CasillaDeSalaValida(x, y);
						});
						PonerEnemigo(p, (int)i);
					}
				} else if (r.tipo == SALA_JEFE) {
					Vector2i c = r.get_center();
					m_jefe = CasillaCercana(r, c, [&](int x, int y) { return CasillaDeSalaValida(x, y) && !PegadoACofre(x, y); });
					PonerEnemigo(m_jefe, (int)i);
					// Escort from floor 2
					for (int e = 0; e < (m_piso >= 2 ? 1 : 0) + (m_piso >= 4 ? 1 : 0); e++) {
						Vector2i p = CasillaCercana(r, m_jefe, [&](int x, int y) {
							int d = std::abs(x - m_jefe.x) + std::abs(y - m_jefe.y);
							return d >= 2 && d <= 3 && CasillaDeSalaValida(x, y);
						});
						PonerEnemigo(p, (int)i);
					}
				} else if (r.tipo == SALA_NORMAL && UtilityFunctions::randi_range(0, 99) < 35) {
					// Some loose prowler
					std::vector<Vector2i> validas;
					for (int x = r.x + 1; x < r.x + r.w - 1; x++)
						for (int y = r.y + 1; y < r.y + r.h - 1; y++)
							if (CasillaDeSalaValida(x, y)) validas.push_back(Vector2i(x, y));
					if (!validas.empty()) PonerEnemigo(validas[UtilityFunctions::randi_range(0, (int)validas.size() - 1)], -1);
				}
			}
		}

		// Events go in rooms without a camp (one or two per room); if they don't fit, in the
		// corridors
		void GenerarEventos() {
			m_eventos.clear();

			std::vector<String> disponibles;
			String carpeta = "res://Data/Events/";
			Ref<DirAccess> dir = DirAccess::open(carpeta);
			if (dir.is_valid()) {
				dir->list_dir_begin();
				for (String f = dir->get_next(); !f.is_empty(); f = dir->get_next()) {
					if (dir->current_is_dir()) continue;
					// In exported builds the .tres can show up as .tres.remap
					if (f.ends_with(".tres.remap")) f = f.trim_suffix(".remap");
					if (f.ends_with(".tres")) disponibles.push_back(carpeta + f);
				}
			}
			if (disponibles.empty()) return;

			auto crear = [&](Vector2i pos) {
				String path = disponibles[UtilityFunctions::randi_range(0, (int)disponibles.size() - 1)];
				Ref<Resource> evt = ResourceLoader::get_singleton()->load(path);
				if (evt.is_null()) return false;
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
				return true;
			};

			int normales = 0;
			for (const Room& r : m_rooms) if (r.tipo == SALA_NORMAL) normales++;
			int a_crear = std::clamp(normales * 2, 3, 6);
			for (const Room& r : m_rooms) {
				if (r.tipo != SALA_NORMAL) continue;
				std::vector<Vector2i> libres;
				for (int x = r.x; x < r.x + r.w; x++)
					for (int y = r.y; y < r.y + r.h; y++)
						if (CasillaDeSalaValida(x, y)) libres.push_back(Vector2i(x, y));
				int aqui = UtilityFunctions::randi_range(1, 2);
				while (aqui > 0 && (int)m_eventos.size() < a_crear && !libres.empty()) {
					int idx = UtilityFunctions::randi_range(0, (int)libres.size() - 1);
					Vector2i pos = libres[idx];
					libres.erase(libres.begin() + idx);
					if (crear(pos)) aqui--;
				}
			}
			// Corridors (outside every room)
			std::vector<Vector2i> pasillos;
			for (int x = 0; x < GRID_SIZE; x++)
				for (int y = 0; y < GRID_SIZE; y++)
					if (!EnAlgunaSala(x, y) && CasillaDeSalaValida(x, y)) pasillos.push_back(Vector2i(x, y));
			while ((int)m_eventos.size() < a_crear && !pasillos.empty()) {
				int idx = UtilityFunctions::randi_range(0, (int)pasillos.size() - 1);
				Vector2i pos = pasillos[idx];
				pasillos.erase(pasillos.begin() + idx);
				crear(pos);
			}
		}

public:
		// For the rest of the scripts (Zonas.gd): [{tipo, zona, caja, centro, cofre}]
		Array ListaZonas() {
			Array out;
			for (size_t i = 0; i < m_rooms.size(); i++) {
				const Room& r = m_rooms[i];
				Dictionary z;
				z["indice"] = (int)i;
				z["tipo"] = String(NombreTipoSala(r.tipo));
				z["zona"] = r.zona;
				z["caja"] = r.caja();
				z["centro"] = r.get_center();
				z["cofre"] = r.cofre;
				z["dist"] = r.dist;
				out.push_back(z);
			}
			return out;
		}
};

} // namespace mazmorra
