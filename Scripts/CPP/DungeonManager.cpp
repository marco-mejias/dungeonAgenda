/* Jenova C++ Node Base Script (Meteora) - DungeonManager */

#include <Godot/godot.hpp>
#include <Godot/classes/node2d.hpp>
#include <Godot/classes/tile_map_layer.hpp>
#include <Godot/classes/a_star_grid2d.hpp>
#include <Godot/variant/utility_functions.hpp>
#include <Godot/classes/line2d.hpp>
#include <Godot/variant/string.hpp>
#include <Godot/classes/tween.hpp>
#include <Godot/classes/property_tweener.hpp>
#include <Godot/classes/resource_loader.hpp>
#include <Godot/classes/callback_tweener.hpp>
#include <Godot/classes/packed_scene.hpp>
#include <Godot/classes/color_rect.hpp>
#include <Godot/classes/label.hpp>
#include <Godot/classes/viewport.hpp>
#include <Godot/classes/input.hpp>
#include <Godot/classes/point_light2d.hpp>
#include <Godot/classes/tile_data.hpp>
#include <Godot/classes/interval_tweener.hpp>
#include <Godot/classes/dir_access.hpp>
#include <Godot/classes/viewport.hpp>
#include <Godot/classes/sub_viewport.hpp>
#include <Godot/classes/control.hpp>
#include <Godot/classes/window.hpp>
#include <algorithm>
#include <vector>
#include <random>
#include <queue>

using namespace godot;
using namespace jenova::sdk;

struct Cell {
	Vector2i grid_pos;
	bool is_wall = false;
	bool is_highlighted = false;
	bool is_explored = false;
	bool is_visible = false; 
	bool is_lit = false; 
	bool is_door; // NUEVO: La casilla es una puerta
	bool door_open; // NUEVO: ¿Está abierta o cerrada?     
	Vector2i atlas_coords; 
	bool is_chest = false;
	bool chest_opened = false;
	bool has_event = false;
	bool has_enemy = false;
	String event_path = ""; 
};

// Estructura para definir y manipular habitaciones
struct Room {
	int x, y, w, h;
	
	Vector2i get_center() const {
		return Vector2i(x + w / 2, y + h / 2);
	}
	
	bool intersects(const Room& other) const {
		return (x <= other.x + other.w + 1 && x + w + 1 >= other.x &&
				y <= other.y + other.h + 1 && y + h + 1 >= other.y);
	}
};

std::vector<Room> m_rooms;

namespace {
	static Node2D* self = nullptr;
	int GRID_SIZE = 35;
	
	std::vector<std::vector<Cell>> map_data;

	std::vector<String> event_paths; 
	Node* m_party_manager = nullptr;   // Cache for PartyManager
	Node* m_ui_manager = nullptr;      // Cache for UI
	TileMapLayer* m_board = nullptr;
	Line2D* m_path_line = nullptr;
	
	// DEBUGGING
	static bool debug_mode = false;
	static bool debug_key_pressed = false; // Para evitar que se active/desactive 60 veces por segundo
}

JENOVA_SCRIPT_BEGIN

void UpdateHighlightForWarrior(Caller* instance);
void CalcularMapaDeLuces(Caller* instance);
bool IsCasillaIluminada(Caller* instance, int x, int y);
void ActualizarVisibilidadEntidades(Caller* instance);
bool CheckLineOfSight(Vector2i start, Vector2i end);

// ==========================================
// VISUAL DUNGEON FUNCTIONS
// ==========================================

bool IsCasillaIluminada(Caller* instance, int x, int y) {
	if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE) return false;
	return map_data[x][y].is_lit;
}

void CalcularMapaDeLuces(Caller* instance) {
	if (!self) return;

	// 1. Apagamos toda la luz del mapa
	for (int x = 0; x < GRID_SIZE; ++x) {
		for (int y = 0; y < GRID_SIZE; ++y) {
			map_data[x][y].is_lit = false;
		}
	}

	// 2. Encendemos la luz alrededor de TODOS los personajes vivos
	if (m_party_manager) {
		TypedArray<Node> heroes = m_party_manager->get_children();
		for (int i = 0; i < heroes.size(); i++) {
			Node* hero = Object::cast_to<Node>(heroes[i]);
			
			if (hero && hero->get_name() != StringName("DatosEquipo")) {
				bool is_dead = hero->has_meta("esta_muerto") ? (bool)hero->get_meta("esta_muerto") : false;
				
				if (!is_dead && hero->has_meta("grid_pos")) {
					Vector2i pos = hero->get_meta("grid_pos");
					
					// Puedes darle a cada clase un radio distinto. Ej: El Mago tiene radio 4, el Ladrón 2.
					int radio_luz = hero->has_meta("radio_luz") ? (int)hero->get_meta("radio_luz") : 3;

					for (int cx = 0; cx < GRID_SIZE; ++cx) {
						for (int cy = 0; cy < GRID_SIZE; ++cy) {
							if (abs(cx - pos.x) + abs(cy - pos.y) <= radio_luz) {
								if (CheckLineOfSight(pos, Vector2i(cx, cy))) {
									map_data[cx][cy].is_lit = true;
								}
							}
						}
					}
				}
			}
		}
	}
	
	// (En el futuro, aquí puedes añadir un bucle para encender luz alrededor de braseros/antorchas en las paredes)
}

void DibujarMarcadoresEventos(Caller* instance) {
	if (!self) return;
	TileMapLayer* board = Object::cast_to<TileMapLayer>(self->get_node_or_null("Tablero"));
	if (!board) return;

	// MARCADORES EVENTOS
	if (self->has_meta("lista_eventos")) {
		Array event_tiles = self->get_meta("lista_eventos");
		
		for (int i = 0; i < event_tiles.size(); i++) {
			Vector2i grid_pos = event_tiles[i];
			
			ColorRect* marker = memnew(ColorRect);
			
			String marker_name = "Marcador_Evento_" + String::num_int64(grid_pos.x) + "_" + String::num_int64(grid_pos.y);
			marker->set_name(marker_name);
			marker->set_size(Vector2(6, 6)); 
			marker->set_color(Color(1.0, 0.0, 1.0, 0.6));
			
			Vector2 center_pos = board->map_to_local(grid_pos);
			marker->set_position(center_pos - Vector2(3, 3)); 
			
			self->add_child(marker);
		}
	}
	
	// MARCADORES SALIDA
	if (self->has_meta("casilla_salida")) {
		Vector2i exit_pos = self->get_meta("casilla_salida");
		
		ColorRect* exit_marker = memnew(ColorRect);
		exit_marker->set_name("Marcador_Salida");
		exit_marker->set_size(Vector2(6, 6)); 
		exit_marker->set_color(Color(0.0, 0.5, 1.0, 0.8)); // Bright Blue
		
		Vector2 exit_center_pos = board->map_to_local(exit_pos);
		exit_marker->set_position(exit_center_pos - Vector2(3, 3)); 
		
		self->add_child(exit_marker);
		UtilityFunctions::print("[DungeonManager]: Blue exit marker rendered.");
	}

	// MARCADORES TESORO
	int drawn_chests = 0;
	for (int x = 0; x < GRID_SIZE; ++x) {
		for (int y = 0; y < GRID_SIZE; ++y) {
			if (map_data[x][y].is_chest) {
				ColorRect* chest_marker = memnew(ColorRect);
				
				// Using "Marcador_" so it gets destroyed properly during floor cleanup
				chest_marker->set_name("Marcador_Cofre_" + String::num_int64(x) + "_" + String::num_int64(y));
				chest_marker->set_size(Vector2(6, 6)); // Same size as pink ones
				chest_marker->set_color(Color(1.0, 0.8, 0.0, 0.8)); // Gold
				
				Vector2 center_pos = board->map_to_local(Vector2i(x, y));
				chest_marker->set_position(center_pos - Vector2(3, 3)); 

				self->add_child(chest_marker);
				drawn_chests++;
			}
		}
	}
	if (drawn_chests > 0) {
		UtilityFunctions::print("[DungeonManager]: Rendered ", drawn_chests, " gold markers (Chests).");
	}
}


void FinalizarEvento(Caller* instance) {
	if (!self) return;
	
	// QUITAMOS EL CANDADO DEL JUEGO
	self->set_meta("bloqueo_evento", false);
	self->set_meta("is_moving", false); // Por si se había quedado atascado
	
	godot::UtilityFunctions::print("[DungeonManager] ¡Evento finalizado, juego descongelado!");
	
	UpdateHighlightForWarrior(instance);
}

void GenerarEnemigos(Caller* instance) {
	Array enemy_tiles;

	// Empezamos en 1 para saltarnos m_rooms[0] (la sala donde hace spawn el jugador)
	for (size_t i = 1; i < m_rooms.size(); i++) {
		
		std::vector<Vector2i> casillas_validas;

		// Asumimos que tu clase/struct de habitación tiene variables para su posición y tamaño
		// Si en tu código se llaman distinto (ej: pos.x, ancho, width), cámbialo aquí:
		int rx = m_rooms[i].x; 
		int ry = m_rooms[i].y;
		int rw = m_rooms[i].w;
		int rh = m_rooms[i].h;

		// Escaneamos la habitación buscando suelo limpio
		for (int x = rx; x < rx + rw; x++) {
			for (int y = ry; y < ry + rh; y++) {
				// Evitamos salirnos del mapa por seguridad
				if (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE) {
					// Comprobamos que sea suelo normal y no esté ocupado
					if (!map_data[x][y].is_wall && !map_data[x][y].is_door &&
						!map_data[x][y].is_chest && !map_data[x][y].has_event &&
						!map_data[x][y].has_enemy) {
						
						casillas_validas.push_back(Vector2i(x, y));
					}
				}
			}
		}

		// Decidimos cuántos enemigos habrá en esta sala (ej. de 1 a 3)
		int num_enemigos = godot::UtilityFunctions::randi_range(1, 3);
		if (num_enemigos > casillas_validas.size()) {
			num_enemigos = casillas_validas.size(); // Por si la sala es muy pequeña
		}

		// Colocamos a los enemigos
		for (int e = 0; e < num_enemigos; e++) {
			int rand_idx = godot::UtilityFunctions::randi_range(0, casillas_validas.size() - 1);
			Vector2i pos = casillas_validas[rand_idx];

			map_data[pos.x][pos.y].has_enemy = true;
			enemy_tiles.push_back(pos);

			// Borramos esa casilla de las válidas para no poner dos enemigos uno encima del otro
			casillas_validas.erase(casillas_validas.begin() + rand_idx);
		}
	}

	// Guardamos la lista en la metadata del nodo para usarla después al dibujar
	self->set_meta("lista_enemigos", enemy_tiles);
	godot::UtilityFunctions::print("[DungeonManager] Enemigos asignados en la lógica: ", enemy_tiles.size());
}

void GenerarCofres(Caller* instance) {
	if (!self) return;

	int created_chests = 0;
	std::default_random_engine gen(std::chrono::system_clock::now().time_since_epoch().count());
	std::uniform_int_distribution<int> dist(1, GRID_SIZE - 2); 

	TileMapLayer* board = Object::cast_to<TileMapLayer>(self->get_node_or_null("Tablero"));
	if (!board) return;

	// Read dynamic variable to know where the real exit is
	Vector2i exit_pos = self->has_meta("casilla_salida") ? (Vector2i)self->get_meta("casilla_salida") : Vector2i(-1, -1);

	while (created_chests < 4) {
		int rx = dist(gen);
		int ry = dist(gen);

		// CONDITIONS TO PLACE THE CHEST:
		// 1. Not a wall
		// 2. Not the ENTRANCE (0,0)
		// 3. Not the random EXIT (exit_pos)
		// 4. No chest there already
		if (!map_data[rx][ry].is_wall && 
			!(rx == 0 && ry == 0) && 
			!(rx == exit_pos.x && ry == exit_pos.y) && 
			!map_data[rx][ry].is_chest) {
			
			map_data[rx][ry].is_chest = true;
			
			// Paint the chest
			board->set_cell(Vector2i(rx, ry), 0, Vector2i(4, 1)); 
			created_chests++;
		}
	}
}

void ActualizarVisibilidadEntidades(Caller* instance) {
	if (!self) return;

	for (int x = 0; x < GRID_SIZE; ++x) {
		for (int y = 0; y < GRID_SIZE; ++y) {
			if (map_data[x][y].is_chest) {
				String marker_name = "Marcador_Cofre_" + String::num_int64(x) + "_" + String::num_int64(y);
				Node2D* chest = Object::cast_to<Node2D>(self->get_node_or_null(NodePath(marker_name)));
				
				if (chest) {
					chest->set_visible(map_data[x][y].is_visible); 
				}
			}
		}
	}
	
	Array event_tiles = self->has_meta("lista_eventos") ? (Array)self->get_meta("lista_eventos") : Array();
	for (int i = 0; i < event_tiles.size(); i++) {
		Vector2i pos = event_tiles[i];
		String marker_name = "Marcador_Evento_" + String::num_int64(pos.x) + "_" + String::num_int64(pos.y);
		Node2D* marker = Object::cast_to<Node2D>(self->get_node_or_null(NodePath(marker_name)));
		
		if (marker) {
			marker->set_visible(map_data[pos.x][pos.y].is_visible);
		}
	}
}

// NUEVO: SISTEMA DE PROPAGACIÓN DE SONIDO DE MONSTRUOS
bool PuedeEscucharSonido(Caller* instance, int origin_x, int origin_y, int sound_radius, bool same_room_only) {
	if (!m_party_manager) return false;

	TypedArray<Node> heroes = m_party_manager->get_children();
	for (int i = 0; i < heroes.size(); i++) {
		Node* hero = Object::cast_to<Node>(heroes[i]);
		
		if (hero && hero->get_name() != StringName("DatosEquipo")) {
			bool is_dead = hero->has_meta("esta_muerto") ? (bool)hero->get_meta("esta_muerto") : false;
			
			if (!is_dead && hero->has_meta("grid_pos")) {
				Vector2i hero_pos = hero->get_meta("grid_pos");
				int dist = abs(hero_pos.x - origin_x) + abs(hero_pos.y - origin_y);
				
				if (dist <= sound_radius) {
					// Si es un sonido bajo (same_room_only), requiere Línea de Visión (sin muros en medio)
					if (same_room_only) {
						if (CheckLineOfSight(hero_pos, Vector2i(origin_x, origin_y))) return true;
					} else {
						// Si es un sonido fuerte (ej: un Gólem), lo oyes a través de las paredes
						return true; 
					}
				}
			}
		}
	}
	return false;
}

void UpdateTileMapCell(Caller* instance, int x, int y) {
	if (!m_board || x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE) return;

	// IMPORTANTE: Recuerda poner aquí tu ID real del TileSet
	int ID_TILESET = 1; 

	// ========================================================
	// 1. CAPA DE ILUMINACIÓN (HighlightLayer)
	// ========================================================
	TileMapLayer* highlight_layer = Object::cast_to<TileMapLayer>(self->get_node_or_null("HighlightLayer"));
	
	if (highlight_layer) {
		if (map_data[x][y].is_highlighted) {
			// Si está iluminado, pintamos la casilla amarilla en la capa superior
			// IMPORTANTE: Cambia Vector2i(5, 0) por la coordenada de tu brillo
			highlight_layer->set_cell(Vector2i(x, y), ID_TILESET, Vector2i(9, 6)); 
		} else {
			// Si NO está iluminado, borramos lo que haya en esta capa transparente
			highlight_layer->erase_cell(Vector2i(x, y)); 
		}
	}

	// ========================================================
	// 2. CAPA FÍSICA (Tablero) - ¡Ya no se borra nunca!
	// ========================================================
	if (map_data[x][y].is_chest) {
		if (map_data[x][y].chest_opened) m_board->set_cell(Vector2i(x, y), ID_TILESET, Vector2i(4, 8)); 
		else m_board->set_cell(Vector2i(x, y), ID_TILESET, Vector2i(3, 8)); 
		return;
	}

	// Terreno base (Suelos, Muros y Puertas)
	m_board->set_cell(Vector2i(x, y), ID_TILESET, map_data[x][y].atlas_coords);
}

void ApagarLucesMapa(Caller* instance) {
	for (int cx = 0; cx < GRID_SIZE; ++cx) {
		for (int cy = 0; cy < GRID_SIZE; ++cy) {
			if (map_data[cx][cy].is_highlighted) {
				map_data[cx][cy].is_highlighted = false;
				UpdateTileMapCell(instance, cx, cy);
			}
		}
	}
}

// ==========================================
// MOVEMENT LOGIC FUNCTIONS
// ==========================================

bool IsCellWalkable(Caller* instance, int x, int y) 
{
	if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE) {
		return false; 
	}
	return !map_data[x][y].is_wall;
}

bool CheckLineOfSight(Vector2i start, Vector2i end) {
	int x0 = start.x;
	int y0 = start.y;
	int x1 = end.x;
	int y1 = end.y;

	int dx = abs(x1 - x0);
	int sx = x0 < x1 ? 1 : -1;
	int dy = -abs(y1 - y0);
	int sy = y0 < y1 ? 1 : -1;
	int err = dx + dy;
	int e2;

	while (true) {
		if (x0 < 0 || x0 >= GRID_SIZE || y0 < 0 || y0 >= GRID_SIZE) return false;

		// NUEVO: Definimos qué es un obstáculo visual (Muro o Puerta Cerrada)
		bool obstaculo_visual = map_data[x0][y0].is_wall || (map_data[x0][y0].is_door && !map_data[x0][y0].door_open);

		// Si golpeamos un obstáculo, podemos verlo, pero la visión se detiene ahí
		if (obstaculo_visual) {
			if (x0 == x1 && y0 == y1) return true; 
			return false; 
		}

		if (x0 == x1 && y0 == y1) break;

		e2 = 2 * err;
		int prev_x = x0;
		int prev_y = y0;

		if (e2 >= dy) { err += dy; x0 += sx; }
		if (e2 <= dx) { err += dx; y0 += sy; }

		// Control estricto de esquinas (Diagonal move)
		if (x0 != prev_x && y0 != prev_y) {
			bool wall1 = false;
			bool wall2 = false;
			
			// Comprobamos la casilla ortogonal 1
			if (x0 >= 0 && x0 < GRID_SIZE && prev_y >= 0 && prev_y < GRID_SIZE) {
				wall1 = map_data[x0][prev_y].is_wall || (map_data[x0][prev_y].is_door && !map_data[x0][prev_y].door_open);
			}
			// Comprobamos la casilla ortogonal 2
			if (prev_x >= 0 && prev_x < GRID_SIZE && y0 >= 0 && y0 < GRID_SIZE) {
				wall2 = map_data[prev_x][y0].is_wall || (map_data[prev_x][y0].is_door && !map_data[prev_x][y0].door_open);
			}

			// Si al menos una casilla lateral es un muro o puerta cerrada, la visión choca en la esquina
			if (wall1 || wall2) {
				return false;
			}
		}
	}

	return true;
}

void SpawnVisualCharacter(Caller* instance, Variant recurso_ficha, Variant index_posicion_var) {
	Ref<Resource> res = recurso_ficha;
	if (res.is_null()) return;

	int position_index = (int)index_posicion_var;

	String name = res->get("nombre_display");
	String prefab_path = res->get("ruta_cuerpo");

	Ref<PackedScene> prefab = ResourceLoader::get_singleton()->load(prefab_path);
	if (prefab.is_null()) {
		godot::UtilityFunctions::print("[DungeonManager] Visual scene not found: ", prefab_path);
		return;
	}

	Node2D* visual_token = Object::cast_to<Node2D>(prefab->instantiate());
	visual_token->set_name(name);

	// Dynamic light for each character
	PointLight2D* light = Object::cast_to<PointLight2D>(visual_token->get_node_or_null("PointLight2D"));
	if (light) {
		int speed = 3; // Default just in case
		Variant spd_var = res->get("velocidad");
		if (spd_var.get_type() != Variant::NIL) {
			speed = (int)spd_var;
		}

		// Math: 1 tile = 16 pixels. 
		float radius = (speed + 1.5) * 16.0; 
		float scale_factor = radius / 128.0; 
		float final_scale = 20.0 * scale_factor;
		light->set_scale(Vector2(final_scale, final_scale));
	}

	TileMapLayer* board = Object::cast_to<TileMapLayer>(self->get_node_or_null("Tablero"));
	
	// === LA MAGIA DEL SPAWN DINÁMICO ===
	// Leemos la coordenada segura que calculó ConstruirLogica
	Vector2i grid_pos = Vector2i(0, 0); 
	if (self->has_meta("casilla_inicio")) {
		grid_pos = self->get_meta("casilla_inicio");
	}
	
	if (board) {
		Vector2 cell_center_pos = board->map_to_local(grid_pos);
		visual_token->set_scale(Vector2(0.05, 0.05));
		
		// Spawn characters separated
		Vector2 offset(0, 0);
		if (position_index == 0) offset = Vector2(-1, -1); 
		if (position_index == 1) offset = Vector2(1, -1);  
		if (position_index == 2) offset = Vector2(1, 1);   
		if (position_index == 3) offset = Vector2(-1, 1);  

		visual_token->set_position(cell_center_pos + offset);
	}

	self->add_child(visual_token);
	godot::UtilityFunctions::print("[DungeonManager] ", name, " loaded in the dungeon at ", grid_pos.x, ", ", grid_pos.y);
	
	if (m_party_manager) {
		Node* character = m_party_manager->get_node_or_null(NodePath(name));
		if (character) {
			character->set_meta("grid_pos", grid_pos);
		}
	}
}

void DescubrirNiebla(Caller* instance, int center_x, int center_y, int vision_radius) {
	
	self->call("CalcularMapaDeLuces");

	for (int x = 0; x < GRID_SIZE; ++x) {
		for (int y = 0; y < GRID_SIZE; ++y) {
			map_data[x][y].is_visible = false; 
		}
	}

	for (int x = 0; x < GRID_SIZE; ++x) {
		for (int y = 0; y < GRID_SIZE; ++y) {
			if (abs(x - center_x) + abs(y - center_y) <= vision_radius) {
				if (CheckLineOfSight(Vector2i(center_x, center_y), Vector2i(x, y))) {
					map_data[x][y].is_explored = true; 
					map_data[x][y].is_visible = true;  
				}
			}
		}
	}

	std::vector<Vector2i> walls_to_reveal;
	for (int x = 0; x < GRID_SIZE; ++x) {
		for (int y = 0; y < GRID_SIZE; ++y) {
			if (map_data[x][y].is_visible && !map_data[x][y].is_wall) {
				for (int dx = -1; dx <= 1; dx++) {
					for (int dy = -1; dy <= 1; dy++) {
						int nx = x + dx, ny = y + dy;
						if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE) {
							if (map_data[nx][ny].is_wall && !map_data[nx][ny].is_explored) {
								walls_to_reveal.push_back(Vector2i(nx, ny));
							}
						}
					}
				}
			}
		}
	}
	
	for (Vector2i wall : walls_to_reveal) {
		map_data[wall.x][wall.y].is_explored = true;
		map_data[wall.x][wall.y].is_visible = true; 
	}

	for (int x = 0; x < GRID_SIZE; ++x) {
		for (int y = 0; y < GRID_SIZE; ++y) {
			UpdateTileMapCell(instance, x, y);
		}
	}
	
	self->call("ActualizarVisibilidadEntidades");
}

void ComprobarAggro(Caller* instance, Node* act_char, int char_x, int char_y) {
	if (!act_char) return;
	
	// Si este personaje YA está peleando, no necesita volver a asustarse
	if (act_char->has_meta("in_combat") && (bool)act_char->get_meta("in_combat")) return;

	int radio_vision = 6; // Rango de casillas para que un enemigo te detecte
	bool detectado = false;

	if (self->has_meta("lista_enemigos")) {
		godot::Array enemy_tiles = self->get_meta("lista_enemigos");
		
		for (int i = 0; i < enemy_tiles.size(); i++) {
			godot::Vector2i pos_enemigo = enemy_tiles[i];
			
			// Buscamos al enemigo en la escena
			godot::String nombre_enemigo = "Enemy_" + godot::String::num_int64(pos_enemigo.x) + "_" + godot::String::num_int64(pos_enemigo.y);
			Node2D* enemigo = Object::cast_to<Node2D>(self->get_node_or_null(NodePath(nombre_enemigo)));
			
			// Si el enemigo existe y no está ya peleando contigo...
			if (enemigo && (!enemigo->has_meta("in_combat") || !(bool)enemigo->get_meta("in_combat"))) {
				
				int dist = std::abs(char_x - pos_enemigo.x) + std::abs(char_y - pos_enemigo.y);
				
				if (dist <= radio_vision) {
					// Comprobamos si hay un muro tapando la vista
					if (CheckLineOfSight(Vector2i(char_x, char_y), pos_enemigo)) {
						detectado = true;
						enemigo->set_meta("in_combat", true); // Metemos al monstruo en la burbuja
						godot::UtilityFunctions::print("[Combate] ¡Un enemigo te ha visto!");
					}
				}
			}
		}
	}

	if (detectado) {
		act_char->set_meta("in_combat", true); 
		
		// 1. ¡FRENADA DE EMERGENCIA!
		godot::Variant saved_tween = self->get_meta("current_move_tween");
		if (saved_tween.get_type() != Variant::NIL) {
			Ref<Tween> move_tween = saved_tween;
			if (move_tween.is_valid()) move_tween->kill();
		}
		
		self->set_meta("is_moving", false);

		// 2. ARREGLO DE DESINCRONIZACIÓN Y PA
		TileMapLayer* board = Object::cast_to<TileMapLayer>(self->get_node_or_null("Tablero"));
		Node2D* token = Object::cast_to<Node2D>(self->get_node_or_null(NodePath(act_char->get_name())));
		
		if (token && board) {
			token->set_position(board->map_to_local(Vector2i(char_x, char_y)));
			token->set_rotation(0.0); 
		}

		act_char->set_meta("grid_pos", Vector2i(char_x, char_y));
		
		// 3. Usamos la función sirviente para apagar las luces
		ApagarLucesMapa(instance);

		godot::UtilityFunctions::print("[Combate] ¡", act_char->get_name(), " HA ENTRADO EN COMBATE!");
	}
}

void PasoDado(Caller* instance, int x, int y, int vision) {
	if (!self) return;
	
	Node* audio = self->get_node_or_null(NodePath("/root/AudioManager"));
	if (audio) audio->call("play_sfx", "walk");
	
	DescubrirNiebla(instance, x, y, vision);

	if (!m_party_manager) return;
	
	// FIX 1: Cast seguro del personaje
	godot::Variant char_var = m_party_manager->call("GetActiveCharacter");
	Node* act_char = godot::Object::cast_to<Node>((godot::Object*)char_var);
	if (!act_char) return;

	bool evento_detonado = false;
	Vector2i pos_detonante = Vector2i(-1, -1);
	String path_detonante = ""; // Guardamos la ruta del .tres para el EventManager

	// ESCÁNER DE EVENTOS AUTOMÁTICOS
	for (int cx = 0; cx < GRID_SIZE; ++cx) {
		for (int cy = 0; cy < GRID_SIZE; ++cy) {
			
			if (map_data[cx][cy].has_event) {
				int dist = abs(x - cx) + abs(y - cy); 
				
				if (dist <= 5) {
					Ref<Resource> evt = ResourceLoader::get_singleton()->load(map_data[cx][cy].event_path);
					
					if (evt.is_valid()) {
						// FIX 2: Extracción segura de datos del Resource
						int gatillo = (int)evt->get("gatillo"); 
						
						// CASO A: ES DE PROXIMIDAD
						if (gatillo == 1) { 
							int radio = (int)evt->get("radio_activacion");
							
							if (dist <= radio && dist <= vision && CheckLineOfSight(Vector2i(x, y), Vector2i(cx, cy))) {
								evento_detonado = true;
								pos_detonante = Vector2i(cx, cy);
								path_detonante = map_data[cx][cy].event_path;
								break;
							}
						}
						// CONDICIONAL: Solo salta si lo pisas directamente (dist == 0)
						else if (gatillo == 2 && dist == 0) { 
							String stat = (String)evt->get("stat_objetivo");
							int critico = (int)evt->get("valor_critico");
							
							if (act_char->has_meta(stat)) {
								int valor_actual = (int)act_char->get_meta(stat);
								if (valor_actual < critico) {
									evento_detonado = true;
									pos_detonante = Vector2i(cx, cy);
									path_detonante = map_data[cx][cy].event_path;
									break;
								}
							}
						}
					}
				}
			}
		}
		if (evento_detonado) break;
	}

	// ¡SI UN EVENTO AUTOMÁTICO SALTA A LA CARA!
	if (evento_detonado) {
		UtilityFunctions::print("[DungeonManager] ¡Gatillo Automático Detonado!");

		// Matar la animación de forma segura
		godot::Variant saved_tween = self->get_meta("current_move_tween");
		if (saved_tween.get_type() != Variant::NIL) {
			Ref<Tween> move_tween = saved_tween;
			if (move_tween.is_valid()) move_tween->kill();
		}

		self->set_meta("is_moving", false);
		m_party_manager->call("SetActiveGridPos", x, y);
		
		int pa = (int)act_char->get_meta("pa_actual");
		act_char->set_meta("pa_actual", UtilityFunctions::max(0, pa - 1)); 
		
		if (m_ui_manager) m_ui_manager->call("UpdateWarriorUI");

		// =======================================================
		// NUEVO: Le pasamos el "marrón" al EventManager
		// =======================================================
		// =======================================================
		// NUEVO: Le pasamos el "marrón" al EventManager en un Array
		// =======================================================
		Node* event_manager = self->get_node_or_null("EventManager");
		if (event_manager) {
			godot::Array paquete_trampa;
			paquete_trampa.push_back(pos_detonante.x);
			paquete_trampa.push_back(pos_detonante.y);
			paquete_trampa.push_back(godot::String("EVENTO"));
			paquete_trampa.push_back(path_detonante);
			paquete_trampa.push_back(debug_mode);

			event_manager->call("Interactuar", paquete_trampa);
		}
		
		UpdateHighlightForWarrior(instance);
	}
	
	ComprobarAggro(instance, act_char, x, y);
}

// Function to indicate available movement tiles for each character
void HighlightReachableCells(Caller* instance, int start_x, int start_y, int movement_points) 
{
	for (int x = 0; x < GRID_SIZE; ++x) {
		for (int y = 0; y < GRID_SIZE; ++y) {
			if (map_data[x][y].is_highlighted) {
				map_data[x][y].is_highlighted = false;
				UpdateTileMapCell(instance, x, y); 
			}
		}
	}

	std::queue<std::pair<Vector2i, int>> queue;
	std::vector<std::vector<bool>> visited(GRID_SIZE, std::vector<bool>(GRID_SIZE, false));

	Vector2i start_pos = Vector2i(start_x, start_y);
	queue.push({start_pos, 0});
	visited[start_x][start_y] = true;

	map_data[start_x][start_y].is_highlighted = true;
	UpdateTileMapCell(instance, start_x, start_y);

	std::vector<Vector2i> directions = {
		Vector2i(0, -1), Vector2i(0, 1), Vector2i(-1, 0), Vector2i(1, 0)
	};

	while (!queue.empty()) {
		auto current = queue.front();
		Vector2i pos = current.first;
		int current_cost = current.second;
		queue.pop();

		if (current_cost >= movement_points) continue;

		for (const Vector2i& dir : directions) {
			int next_x = pos.x + dir.x;
			int next_y = pos.y + dir.y;

			if (next_x >= 0 && next_x < GRID_SIZE && next_y >= 0 && next_y < GRID_SIZE) {
				Vector2i dest_pos = Vector2i(next_x, next_y);
				
				if (!CheckLineOfSight(start_pos, dest_pos)) {
					continue;
				}

				map_data[next_x][next_y].is_explored = true;

				// 1. SI ES MURO, PARAMOS.
				if (map_data[next_x][next_y].is_wall) {
					UpdateTileMapCell(instance, next_x, next_y); 
				} 
				// 2. NUEVO: SI ES UNA PUERTA CERRADA, LA ILUMINAMOS PERO NO LA ATRAVESAMOS
				else if (map_data[next_x][next_y].is_door && !map_data[next_x][next_y].door_open) {
					if (!visited[next_x][next_y]) {
						visited[next_x][next_y] = true;
						map_data[next_x][next_y].is_highlighted = true; // Se ilumina para poder interactuar
						UpdateTileMapCell(instance, next_x, next_y);
						
						// IMPORTANTE: NO hacemos queue.push(). Así la luz se detiene en la puerta.
					}
				}
				// 3. SI ES SUELO (O PUERTA ABIERTA), SEGUIMOS AVANZANDO
				else if (!visited[next_x][next_y]) {
					visited[next_x][next_y] = true;
					map_data[next_x][next_y].is_highlighted = true; 
					UpdateTileMapCell(instance, next_x, next_y);
					
					queue.push({dest_pos, current_cost + 1});
				}
			}
		}
	}

	// =======================================================
	// 2. ACTUALIZAR EL ASTAR EN UN SOLO MENSAJE MASIVO
	// =======================================================
	Node* path_manager = self->get_node_or_null("PathManager");
	
	if (path_manager) {
		godot::Array casillas_libres; // Metemos todas en una caja
		
		for (int x = 0; x < GRID_SIZE; ++x) {
			for (int y = 0; y < GRID_SIZE; ++y) {
				bool es_obstaculo = map_data[x][y].is_wall || (map_data[x][y].is_door && !map_data[x][y].door_open);
				
				// Solo guardamos las que sean SUELO y además estén ILUMINADAS
				if (!es_obstaculo && map_data[x][y].is_highlighted) {
					casillas_libres.push_back(Vector2i(x, y));
				}
			}
		}
		
		// Le enviamos la caja entera al PathManager en 1 sola llamada (Adiós lag)
		path_manager->call("ActualizarAStarLibres", casillas_libres);
	}
}

void UpdateHighlightForWarrior(Caller* instance) 
{
	if (!m_party_manager) return;

	Node* act_char = Object::cast_to<Node>(m_party_manager->call("GetActiveCharacter"));
	
	if (act_char) {
		// Usamos los PA actuales para ver hasta dónde puede caminar
		int dex = m_party_manager->call("GetCurrentStat", "pa_actual"); 
		Vector2i pos = m_party_manager->call("GetActiveGridPos");

		DescubrirNiebla(instance, pos.x, pos.y, dex);
		HighlightReachableCells(instance, pos.x, pos.y, dex);
	} else {
		UtilityFunctions::print("[Dungeon] No active character triggered.");
	}
}

// Function before finishing the character's turn
void AtLast(Caller* instance, int x, int y, int steps_taken) {
	if (!self) return;
	
	self->set_meta("is_moving", false);

	if (m_party_manager) {
		m_party_manager->call("SetActiveGridPos", x, y);
		
		// RESTAMOS LOS PA USANDO LA VARIABLE MATEMÁTICA
		Node* act_char = Object::cast_to<Node>(m_party_manager->call("GetActiveCharacter"));
		if (act_char && steps_taken > 0) {
			int pa = act_char->get_meta("pa_actual");
			
			// === MODO DEBUG: NO RESTAR PA AL CAMINAR ===
			if (!debug_mode) {
				act_char->set_meta("pa_actual", godot::UtilityFunctions::max(0, pa - steps_taken));
			}
		}
	}
	
	// 1. ESCAPE LOGIC 
	if (self->has_meta("casilla_salida")) {
		Vector2i exit_pos = self->get_meta("casilla_salida");
		if (x == exit_pos.x && y == exit_pos.y && m_party_manager) {
			Node* act_char = Object::cast_to<Node>(m_party_manager->call("GetActiveCharacter"));
			if (act_char) {
				m_party_manager->call("PersonajeEscapa", act_char);
				String node_name = act_char->get_name(); 
				Node2D* token = Object::cast_to<Node2D>(self->get_node_or_null(NodePath(node_name)));
				if (token) token->set_visible(false);
			}
		}
	}

	// 2. COMPROBAR FIN DE TURNO POR QUEDARSE SIN PA
	if (m_party_manager) {
		int pa_left = m_party_manager->call("GetCurrentStat", "pa_actual");
		if (pa_left <= 0) {
			m_party_manager->call("TerminarTurnoPersonaje");
		}
	}

	if (m_ui_manager) {
		m_ui_manager->call("UpdateWarriorUI");
	}

	UpdateHighlightForWarrior(instance);
}

// ---------------------------------------------------------
// 1. DIBUJAR LA LÍNEA 
// ---------------------------------------------------------
void ActualizarHover(Caller* instance, int hover_x, int hover_y) {
	if (!m_path_line || !m_board) return;
	
	// 1. Lo primero que hace es borrar la línea
	m_path_line->clear_points(); 

	if (!m_party_manager) return;

// =======================================================
	// 🛡️ ESCUDO ANTI-PATHLINE (Busca en la UI)
	// =======================================================
	Node* ui_layer = self->get_node_or_null("/root/Main/ContenedorMapa");
	if (ui_layer && ui_layer->get_node_or_null("MenuTactico_Instancia")) {
		return; // Menú abierto, no dibujamos la línea
	}
	// =======================================================

	if (hover_x < 0 || hover_x >= GRID_SIZE || hover_y < 0 || hover_y >= GRID_SIZE) return;

	if (map_data[hover_x][hover_y].is_highlighted) {
		Node* path_manager = self->get_node_or_null("PathManager");
		
		if (path_manager) {
			Vector2i current_pos = m_party_manager->call("GetActiveGridPos");
			
			// === EL CAMBIO CLAVE (4 INTs) ===
			godot::Array path = path_manager->call("CalcularRuta", current_pos.x, current_pos.y, hover_x, hover_y);
			
			for (int i = 0; i < path.size(); i++) {
				Vector2i step = path[i];
				Vector2 pixel_pos = m_board->map_to_local(step); 
				m_path_line->add_point(pixel_pos);
			}
		}
	}
}

void ToggleMenuTactico(Caller* instance) {
	godot::Node* self_node = GetSelf<godot::Node>(instance);
	godot::Node* ui_layer = self_node->get_node_or_null("/root/Main/ContenedorMapa");
	if (!ui_layer) return;

	// 1. Destruimos CUALQUIER menú abierto (evita menús de otros personajes)
	Node* menu_existente = ui_layer->get_node_or_null("MenuTactico_Instancia");
	if (menu_existente) {
		menu_existente->queue_free();
		self_node->call("RefrescarLuces"); // Restauramos luces al cerrarlo a mano
		return;
	}

	// 2. Creamos el menú en la UI
	godot::Ref<godot::PackedScene> menu_scene = godot::ResourceLoader::get_singleton()->load("res://Scenes/MenuTactico.tscn");
	
	if (menu_scene.is_valid()) {
		Node* nuevo_menu = menu_scene->instantiate();
		nuevo_menu->set_name("MenuTactico_Instancia");
		ui_layer->add_child(nuevo_menu); // Lo colgamos en la pantalla plana

		// Apagamos luces al abrirlo
		self_node->call("ApagarLucesMapa");
	}
}

// ---------------------------------------------------------
// 2. RECIBIR EL CLIC (El cerebro de las interacciones)
// ---------------------------------------------------------
void ProcesarClicEnCasilla(Caller* instance, int x, int y) {
	if (!m_party_manager) return;

	// =======================================================
	// 🚩 ESCUDO DEFINITIVO: ¿Está el ratón sobre la interfaz?
	// =======================================================
	bool raton_en_ui = false;
	if (self->has_meta("raton_en_ui")) {
		raton_en_ui = (bool)self->get_meta("raton_en_ui");
	}
	
	// Si el ratón está tocando el menú, ignoramos el clic en el mapa por completo.
	// Esto salva la vida al botón para que pueda pulsarse.
	if (raton_en_ui) {
		return; 
	}
	// =======================================================

	// =======================================================
	// 🛡️ EL ESCUDO ANTI-CRASHEOS: Validar los límites del mapa
	// =======================================================
	if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE) {
		return; // Si el clic está fuera del mapa, ignoramos el clic.
	}
	// =======================================================

	// 1. Obtenemos la posición actual y extraemos al personaje activo de forma segura
	Vector2i current_pos = (Vector2i)m_party_manager->call("GetActiveGridPos");
	godot::Variant char_var = m_party_manager->call("GetActiveCharacter");
	Node* act_char = nullptr;
	if (char_var.get_type() != godot::Variant::NIL) {
		act_char = godot::Object::cast_to<Node>(char_var.operator godot::Object*());
	}

	// =======================================================
	// 🛑 ESCUDO ANTI-MOVIMIENTO (Busca en la UI)
	// =======================================================
	Node* ui_layer = self->get_node_or_null("/root/Main/ContenedorMapa");
	if (ui_layer) {
		Node* menu_existente = ui_layer->get_node_or_null("MenuTactico_Instancia");
		if (menu_existente) {
			// Si haces clic fuera de tu personaje (y el ratón NO estaba en el menú, que ya lo filtramos arriba)
			if (x != current_pos.x || y != current_pos.y) {
				menu_existente->queue_free();
				self->call("RefrescarLuces"); // 💡 RESTAURAMOS LUCES
				return; // Abortamos la caminata
			}
		}
	}
	// =======================================================

	// 2. Distancia y estado de las casillas
	int dist = std::abs(current_pos.x - x) + std::abs(current_pos.y - y);

	bool is_chest = map_data[x][y].is_chest && !map_data[x][y].chest_opened;
	bool puerta_cerrada = map_data[x][y].is_door && !map_data[x][y].door_open;
	bool hay_enemigo = map_data[x][y].has_enemy;
	bool is_manual_event = false;

	if (map_data[x][y].has_event) {
		godot::Ref<godot::Resource> evt = godot::ResourceLoader::get_singleton()->load(map_data[x][y].event_path);
		if (evt.is_valid()) {
			godot::Variant gatillo_var = evt->get("gatillo");
			if ((int)gatillo_var == 0) {
				is_manual_event = true;
			}
		}
	}
	
	// =======================================================
	// 3. ABRIR MENÚ TÁCTICO (Clic en tu propio personaje)
	// =======================================================
	if (x == current_pos.x && y == current_pos.y) {
		self->call("ToggleMenuTactico");
		return; // Detenemos la función para que no intente caminar
	}

	// === INTERACCIONES CERCANAS (Cofres, Puertas, Enemigos) ===
	if (is_chest || is_manual_event || puerta_cerrada || hay_enemigo) {
		
		// RASTREADOR 1: Comprobar qué distancia lee el motor
		godot::UtilityFunctions::print("[Debug] Clic en interacción. Distancia actual: ", dist);
		
		if (dist <= 1) {
			if (hay_enemigo) {
				godot::UtilityFunctions::print("[Dungeon] Enemigo cerca. Usa el menú táctico.");
			} else {
				Node* event_manager = self->get_node_or_null("EventManager");
				if (event_manager) {
					String tipo = "";
					String evt_path = "";
					
					if (is_chest) tipo = "COFRE";
					else if (puerta_cerrada) tipo = "PUERTA";
					else if (is_manual_event) {
						tipo = "EVENTO";
						evt_path = map_data[x][y].event_path;
					}
					
					// =======================================================
					// 📦 EMPAQUETAMOS TODO EN UN ARRAY PARA JENOVA
					// =======================================================
					godot::Array paquete;
					paquete.push_back(x);
					paquete.push_back(y);
					paquete.push_back(tipo);
					paquete.push_back(evt_path);
					paquete.push_back(debug_mode);
					
					godot::UtilityFunctions::print("[Debug] Enviando PAQUETE al EventManager...");
					event_manager->call("Interactuar", paquete);
					// =======================================================
				}
				
				// Llamamos al aggro con el act_char que ya sacamos arriba
				ComprobarAggro(instance, act_char, current_pos.x, current_pos.y);
			}
			return; 
		} else {
			godot::UtilityFunctions::print("[Debug] Demasiado lejos para interactuar. Acércate más.");
		}
	}

	// === CAMINAR (Mandar orden al PathManager) ===
	if (map_data[x][y].is_highlighted) {
		if (self->has_meta("is_moving") && (bool)self->get_meta("is_moving")) return;

		Node* path_manager = self->get_node_or_null("PathManager");
		if (path_manager) {
			// ¡Volvemos a mandar solo 2 parámetros (x, y)!
			path_manager->call("MoverPersonaje", x, y);
		}
	}
}
// ---------------------------------------------------------
// 3. HELPER: APAGAR LUCES (Lo usa el PathManager al empezar a caminar)
// ---------------------------------------------------------
void ApagarLucesRuta(Caller* instance) {
	for (int cx = 0; cx < GRID_SIZE; ++cx) {
		for (int cy = 0; cy < GRID_SIZE; ++cy) {
			if (map_data[cx][cy].is_highlighted) {
				map_data[cx][cy].is_highlighted = false;
				UpdateTileMapCell(instance, cx, cy);
			}
		}
	}
}

// ---------------------------------------------------------
// FUNCIONES SIRVIENTES PARA EL EVENT MANAGER
// ---------------------------------------------------------

void AplicarEstadoCofre(Caller* instance, int x, int y) {
	godot::Node* self_node = GetSelf<godot::Node>(instance);
	map_data[x][y].chest_opened = true;
	
	godot::TileMapLayer* board = godot::Object::cast_to<godot::TileMapLayer>(self_node->get_node_or_null("Tablero"));
	if (board) board->set_cell(Vector2i(x, y), 0, Vector2i(4, 8)); 
	
	Node* chest_marker = self_node->get_node_or_null(NodePath("Marcador_Cofre_" + String::num_int64(x) + "_" + String::num_int64(y)));
	if (chest_marker) chest_marker->queue_free();
}

void AplicarEstadoPuerta(Caller* instance, int x, int y) {
	godot::Node* self_node = GetSelf<godot::Node>(instance);
	map_data[x][y].door_open = true;
	map_data[x][y].atlas_coords = Vector2i(7, 2); 
	UpdateTileMapCell(instance, x, y);

	Node* path_manager = self_node->get_node_or_null("PathManager");
	if (path_manager) {
		// Envolvemos los datos en Variant para que Jenova no crashee
		godot::Variant vx = x;
		godot::Variant vy = y;
		godot::Variant v_solido = false;
		path_manager->call("SetCasillaSolida", vx, vy, v_solido);
	}
}

void AplicarEstadoEvento(Caller* instance, int x, int y) {
	godot::Node* self_node = GetSelf<godot::Node>(instance);
	map_data[x][y].has_event = false;
	map_data[x][y].event_path = "";
	
	Array event_tiles = self_node->get_meta("lista_eventos");
	event_tiles.erase(Vector2i(x, y));
	self_node->set_meta("lista_eventos", event_tiles);
	
	String v_marker = "Marcador_Evento_" + String::num_int64(x) + "_" + String::num_int64(y);
	Node* visual_marker = self_node->get_node_or_null(NodePath(v_marker));
	if (visual_marker) visual_marker->queue_free();
}

void RefrescarLuces(Caller* instance) {
	UpdateHighlightForWarrior(instance);
}

// ==========================================
// DUNGEON CONSTRUCTION START
// ==========================================

void GenerarMapa(Caller* instance) {
	if (!self) return;

	// 1. LIENZO EN NEGRO: Inicializamos toda la matriz a pared
	for (int x = 0; x < GRID_SIZE; x++) {
		for (int y = 0; y < GRID_SIZE; y++) {
			map_data[x][y].grid_pos = Vector2i(x, y);
			map_data[x][y].is_explored = false; 
			map_data[x][y].is_highlighted = false;
			map_data[x][y].is_chest = false;
			map_data[x][y].chest_opened = false;
			map_data[x][y].has_event = false;
			map_data[x][y].event_path = "";
			map_data[x][y].is_wall = true; // Oscuridad total
		}
	}

	m_rooms.clear();
	
	// PARÁMETROS DE ARQUITECTO
	int MAX_ROOMS = 12;
	int MIN_SIZE = 4;
	int MAX_SIZE = 7;
	int MAX_ATTEMPTS = 150;

	// 2. EXCAVAR SALAS
	for (int i = 0; i < MAX_ATTEMPTS; i++) {
		if (m_rooms.size() >= MAX_ROOMS) break;

		int w = godot::UtilityFunctions::randi_range(MIN_SIZE, MAX_SIZE);
		int h = godot::UtilityFunctions::randi_range(MIN_SIZE, MAX_SIZE);
		
		int x = godot::UtilityFunctions::randi_range(1, GRID_SIZE - w - 2);
		int y = godot::UtilityFunctions::randi_range(1, GRID_SIZE - h - 2);

		Room new_room = {x, y, w, h};

		bool colision = false;
		for (const Room& other_room : m_rooms) {
			if (new_room.intersects(other_room)) {
				colision = true;
				break;
			}
		}

		if (!colision) {
			for (int rx = x; rx < x + w; rx++) {
				for (int ry = y; ry < y + h; ry++) {
					map_data[rx][ry].is_wall = false;
				}
			}
			m_rooms.push_back(new_room);
		}
	}
	
	godot::UtilityFunctions::print("[DungeonManager] Fase 1 completada: ", (int)m_rooms.size(), " salas excavadas.");
	
	// ==========================================
	// FASE 2: EXCAVAR PASILLOS (Corridor Carving)
	// ==========================================
	
	// Solo podemos conectar si hay más de 1 sala
	if (m_rooms.size() > 1) {
		for (size_t i = 1; i < m_rooms.size(); i++) {
			Vector2i prev_center = m_rooms[i - 1].get_center();
			Vector2i curr_center = m_rooms[i].get_center();

			int x1 = prev_center.x;
			int y1 = prev_center.y;
			int x2 = curr_center.x;
			int y2 = curr_center.y;

			// Tiramos una moneda para decidir si cavamos primero en horizontal o en vertical
			// Esto hace que los pasillos sean más variados (algunos en "L", otros en "L invertida")
			bool horizontal_first = (godot::UtilityFunctions::randi_range(0, 1) == 0);

			if (horizontal_first) {
				// 1. Túnel horizontal
				for (int x = std::min(x1, x2); x <= std::max(x1, x2); x++) {
					map_data[x][y1].is_wall = false;
				}
				// 2. Túnel vertical
				for (int y = std::min(y1, y2); y <= std::max(y1, y2); y++) {
					map_data[x2][y].is_wall = false;
				}
			} else {
				// 1. Túnel vertical
				for (int y = std::min(y1, y2); y <= std::max(y1, y2); y++) {
					map_data[x1][y].is_wall = false;
				}
				// 2. Túnel horizontal
				for (int x = std::min(x1, x2); x <= std::max(x1, x2); x++) {
					map_data[x][y2].is_wall = false;
				}
			}
		}
		godot::UtilityFunctions::print("[DungeonManager] Fase 2 completada: Pasillos excavados.");
	}
	
	// (Opcional) Si tienes una función visual que dibuje el tilemap al crear el nivel, 
	// deberías llamarla aquí para que los muros se actualicen en pantalla.
	// ActualizarTileMapVisual(instance);
}

void ConstruirLogica(Caller* instance)
{
	Node* parent_node = self->get_parent();
	if (parent_node) {
		m_party_manager = Object::cast_to<Node>(self->get_node_or_null("/root/Main/PartyManager"));
		m_ui_manager = self->get_node_or_null("/root/Main/CanvasLayer");
	}
	
	m_board = Object::cast_to<TileMapLayer>(self->get_node_or_null("Tablero"));
	m_path_line = Object::cast_to<Line2D>(self->get_node_or_null("PathLine"));

	map_data.resize(GRID_SIZE, std::vector<Cell>(GRID_SIZE));
	
	// 1. GENERAR LA GEOMETRÍA DEL MAPA (Muros, Salas y Pasillos)
	GenerarMapa(instance);

	// ==========================================
	// 1.5 GENERAR PUERTAS (ESTRICTO Y ORGÁNICO)
	// ==========================================
	int DISTANCIA_MINIMA = 6; // Subimos a 6 casillas el radio donde no puede haber otra puerta
	int PROBABILIDAD_PUERTA = 40; // Solo un 40% de los sitios válidos tendrán puerta

	for (int x = 1; x < GRID_SIZE - 1; x++) {
		for (int y = 1; y < GRID_SIZE - 1; y++) {
			if (map_data[x][y].is_wall) continue;

			bool pasillo_vertical = (map_data[x-1][y].is_wall && map_data[x+1][y].is_wall && !map_data[x][y-1].is_wall && !map_data[x][y+1].is_wall);
			bool pasillo_horizontal = (map_data[x][y-1].is_wall && map_data[x][y+1].is_wall && !map_data[x-1][y].is_wall && !map_data[x+1][y].is_wall);

			if (pasillo_vertical || pasillo_horizontal) {
				bool hay_puerta_cerca = false;
				
				// Escaneamos un área grande para evitar saturación
				for (int dx = -DISTANCIA_MINIMA; dx <= DISTANCIA_MINIMA; dx++) {
					for (int dy = -DISTANCIA_MINIMA; dy <= DISTANCIA_MINIMA; dy++) {
						int nx = x + dx;
						int ny = y + dy;
						if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE) {
							if (map_data[nx][ny].is_door) {
								hay_puerta_cerca = true;
								break;
							}
						}
					}
					if (hay_puerta_cerca) break;
				}

				// Si no hay puertas cerca, tiramos los dados para ver si la construimos
				if (!hay_puerta_cerca) {
					int tirada = godot::UtilityFunctions::randi() % 100;
					
					if (tirada < PROBABILIDAD_PUERTA) {
						map_data[x][y].is_door = true;
						map_data[x][y].door_open = false; 
					}
				}
			}
		}
	}

	// ==========================================================
	// 2. DELEGAR EL MAPA DE NAVEGACIÓN AL PATHMANAGER (SIN ERRORES)
	// ==========================================================
	Node* path_manager = self->get_node_or_null("PathManager");
	
	if (path_manager) {
		path_manager->call("ConfigurarAStar", GRID_SIZE, 16.0f);
	}

	std::vector<Vector2i> valid_floors;
	godot::Array casillas_libres; // La caja masiva para el PathManager

	for (int x = 0; x < GRID_SIZE; ++x) {
		for (int y = 0; y < GRID_SIZE; ++y) {
			bool es_obstaculo = map_data[x][y].is_wall || (map_data[x][y].is_door && !map_data[x][y].door_open);

			if (!es_obstaculo) {
				// Guardamos las casillas vacías en las dos listas
				valid_floors.push_back(Vector2i(x, y));
				casillas_libres.push_back(Vector2i(x, y));
			}
		}
	}
	
	// Le enviamos todas las casillas libres al GPS de un solo golpe
	if (path_manager) {
		path_manager->call("ActualizarAStarLibres", casillas_libres);
	}
	
	// ==========================================
	// 3. DEFINIR SPAWN Y SALIDA
	// ==========================================
	Vector2i start_pos = Vector2i(1, 1); 
	Vector2i exit_pos = Vector2i(2, 2);

	if (!m_rooms.empty()) {
		start_pos = m_rooms[0].get_center();
		
		// Opcional: Limpiar las puertas que hayan aparecido muy cerca del spawn inicial
		// para que el jugador no empiece encerrado.
		for(int dx = -2; dx <= 2; dx++) {
			for(int dy = -2; dy <= 2; dy++) {
				if(start_pos.x + dx >= 0 && start_pos.x + dx < GRID_SIZE && 
				   start_pos.y + dy >= 0 && start_pos.y + dy < GRID_SIZE) {
					map_data[start_pos.x + dx][start_pos.y + dy].is_door = false;
				}
			}
		}

		if (m_rooms.size() > 1) {
			int max_dist = 0;
			std::vector<int> distances(m_rooms.size(), 0);
			
			for (size_t i = 1; i < m_rooms.size(); i++) {
				Vector2i center = m_rooms[i].get_center();
				int dist = std::abs(center.x - start_pos.x) + std::abs(center.y - start_pos.y);
				distances[i] = dist;
				if (dist > max_dist) max_dist = dist;
			}

			std::vector<int> salas_lejanas;
			for (size_t i = 1; i < m_rooms.size(); i++) {
				if (distances[i] >= (max_dist * 0.65)) {
					salas_lejanas.push_back(i);
				}
			}

			if (!salas_lejanas.empty()) {
				int rand_idx = godot::UtilityFunctions::randi_range(0, salas_lejanas.size() - 1);
				exit_pos = m_rooms[salas_lejanas[rand_idx]].get_center();
			} else {
				exit_pos = m_rooms.back().get_center();
			}
		} else {
			exit_pos = start_pos; 
		}
	}

	self->set_meta("casilla_inicio", start_pos);
	self->set_meta("casilla_salida", exit_pos);

	auto remove_it = std::remove(valid_floors.begin(), valid_floors.end(), start_pos);
	valid_floors.erase(remove_it, valid_floors.end());
	
	remove_it = std::remove(valid_floors.begin(), valid_floors.end(), exit_pos);
	valid_floors.erase(remove_it, valid_floors.end());

	// ==========================================
	// 4. LEER EVENTOS Y COLOCARLOS
	// ==========================================
	Array event_tiles;
	String folder_path = "res://Data/Events/";
	Ref<DirAccess> dir = DirAccess::open(folder_path);
	std::vector<String> available_events;
	
	if (dir.is_valid()) {
		dir->list_dir_begin();
		String file_name = dir->get_next();
		while (!file_name.is_empty()) {
			if (!dir->current_is_dir() && file_name.ends_with(".tres")) {
				available_events.push_back(String(folder_path) + file_name);
			}
			file_name = dir->get_next();
		}
	}

	int events_to_create = m_rooms.size() / 2;
	if (events_to_create < 2) events_to_create = 2; 
	if (events_to_create > 6) events_to_create = 6; 

	while (event_tiles.size() < events_to_create && !valid_floors.empty() && !available_events.empty()) {
		int rand_idx = godot::UtilityFunctions::randi_range(0, valid_floors.size() - 1);
		Vector2i pos = valid_floors[rand_idx];
		
		int rand_evt = godot::UtilityFunctions::randi_range(0, available_events.size() - 1);
		
		map_data[pos.x][pos.y].has_event = true;
		map_data[pos.x][pos.y].event_path = available_events[rand_evt];
		
		event_tiles.push_back(pos);
		valid_floors.erase(valid_floors.begin() + rand_idx);
	}
	
	self->set_meta("lista_eventos", event_tiles);
				
	// 5. COLOCAR COFRES Y ENEMIGOS
	GenerarCofres(instance);
	GenerarEnemigos(instance);
}

void ConstruirVisual(Caller* instance)
{
	m_board = Object::cast_to<TileMapLayer>(self->get_node_or_null("Tablero"));
	m_path_line = Object::cast_to<Line2D>(self->get_node_or_null("PathLine"));

	if (!m_board) {
		godot::UtilityFunctions::print("[ERROR] Aún no encuentro el Tablero en ConstruirVisual");
		return;
	}
	godot::UtilityFunctions::print("Starting visual render on TileMap...");

	String folder_path = "res://Data/Events/";
	Ref<DirAccess> dir = DirAccess::open(folder_path);
	
	event_paths.clear(); 
	if (dir.is_valid()) {
		dir->list_dir_begin();
		String file_name = dir->get_next();
		
		while (!file_name.is_empty()) {
			if (!dir->current_is_dir() && file_name.ends_with(".tres")) {
				event_paths.push_back(String(folder_path) + file_name);
			}
			file_name = dir->get_next();
		}
	}

	// Calculate textures in a simple and direct way
	for (int x = 0; x < GRID_SIZE; ++x) {
		for (int y = 0; y < GRID_SIZE; ++y) {
			
			// === NUEVO: ASIGNAR TEXTURA DE PUERTAS ===
			if (map_data[x][y].is_door) {
				if (map_data[x][y].door_open) {
					map_data[x][y].atlas_coords = Vector2i(6, 0); // Suelo (Puerta abierta)
				} else {
					// ¡CÁMBIALO!: Pon aquí las coordenadas de la Puerta en tu TileSet
					map_data[x][y].atlas_coords = Vector2i(7, 3); // (De momento pongo muro)
				}
			} 
			// === TERRENO NORMAL ===
			else if (map_data[x][y].is_wall) {
				map_data[x][y].atlas_coords = Vector2i(2, 0); 
			} else {
				map_data[x][y].atlas_coords = Vector2i(7, 2); 
			}
		}
	}

	for (int x = 0; x < GRID_SIZE; ++x) {
		for (int y = 0; y < GRID_SIZE; ++y) {
			UpdateTileMapCell(instance, x, y);
		}
	}
	
	UpdateHighlightForWarrior(instance);
	// (Si tienes tu función de MarcadoresEventos, llámala aquí)
	DibujarMarcadoresEventos(instance);
	
	// ==========================================
	// NUEVO: INSTANCIAR LOS ENEMIGOS VISUALMENTE
	// ==========================================
	godot::String ruta_enemigo = "res://Scenes/Enemy.tscn"; 
	
	godot::Ref<godot::PackedScene> escena_enemigo = godot::ResourceLoader::get_singleton()->load(ruta_enemigo);
	
	if (escena_enemigo.is_valid()) {
		if (self->has_meta("lista_enemigos")) {
			godot::Array enemy_tiles = self->get_meta("lista_enemigos");
			
			for (int i = 0; i < enemy_tiles.size(); i++) {
				godot::Vector2i grid_pos = enemy_tiles[i];
				
				// Creamos un clon de la escena
				godot::Node* nuevo_enemigo = escena_enemigo->instantiate();
				godot::Node2D* enemigo_2d = godot::Object::cast_to<godot::Node2D>(nuevo_enemigo);
				
				if (enemigo_2d && m_board) {
					// Calculamos su posición en píxeles basándonos en la cuadrícula
					godot::Vector2 pixel_pos = m_board->map_to_local(grid_pos);
					enemigo_2d->set_position(pixel_pos);
					
					// Le damos un nombre único para encontrarlo fácil después (ej: "Enemy_14_5")
					enemigo_2d->set_name(godot::String("Enemy_") + godot::String::num_int64(grid_pos.x) + "_" + godot::String::num_int64(grid_pos.y));
					
					enemigo_2d->set_meta("in_combat", false);
					
					// Lo añadimos como hijo del tablero para que se dibuje encima
					self->add_child(enemigo_2d);
				}
			}
			godot::UtilityFunctions::print("[DungeonManager] ¡Enemigos instanciados en el mapa!");
		}
	} else {
		godot::UtilityFunctions::printerr("[DungeonManager] ERROR: No se pudo cargar la escena del enemigo en: ", ruta_enemigo);
	}
}

// ==========================================
// BASE LIFECYCLE
// ==========================================

void OnAwake(Caller* instance)
{
	self = GetSelf<Node2D>(instance);
	// CALL THROUGH GODOT TO AVOID JENOVA BUG
	self->call("ConstruirLogica");
}

void OnReady(Caller* instance)
{
	// CALL THROUGH GODOT
	self->call("ConstruirVisual");
}

void OnProcess(Caller* instance, double _delta) 
{
	
}

void OnDestroy(Caller* instance)
{
	self = nullptr;
	map_data.clear(); 
	m_party_manager = nullptr;
	m_ui_manager = nullptr;
}

void SubirMapa(Caller* instance) {
	if (!self) return;
	
	Node* root_node = self->get_node_or_null(NodePath("/root"));
	if (!root_node) return;

	Node* contenedor = root_node->find_child("ContenedorMapa", true, false);
	if (contenedor) {
		Ref<Tween> tween = contenedor->create_tween();
		tween->tween_property(contenedor, "position", Vector2(650, -700), 0.5)
			 ->set_trans(Tween::TRANS_CUBIC)->set_ease(Tween::EASE_IN_OUT);
	} else {
		godot::UtilityFunctions::print("[ERROR] No se encontró ContenedorMapa para subir.");
	}
}

void BajarMapa(Caller* instance) {
	if (!self) return;
	
	Node* root_node = self->get_node_or_null(NodePath("/root"));
	if (!root_node) return;

	Node* contenedor = root_node->find_child("ContenedorMapa", true, false);
	if (contenedor) {
		Ref<Tween> tween = contenedor->create_tween();
		tween->tween_property(contenedor, "position", Vector2(606.0, 53.0), 0.5)
			 ->set_trans(Tween::TRANS_CUBIC)->set_ease(Tween::EASE_OUT);
	} else {
		godot::UtilityFunctions::print("[ERROR] No se encontró ContenedorMapa para bajar.");
	}
}


void AvanzarAlSiguientePiso(Caller* instance) {
	if (!self) return;
	
	// Al principio de la función
	Node* audio = self->get_node_or_null(NodePath("/root/AudioManager"));
	if (audio) audio->call("play_sfx", "stairs");
	
	if (self->has_meta("bloqueo_evento") && (bool)self->get_meta("bloqueo_evento")) {
		UtilityFunctions::print("[DEBUG] Cannot descend floor! You must resolve the active event first.");
		return; // We block floor descent entirely
	}

	UtilityFunctions::print("[DungeonManager] Starting dungeon reconstruction...");
	
	self->set_meta("bloqueo_evento", false);
	if (m_ui_manager) {
		// We ask your GDScript to clean the screen
		m_ui_manager->call("CerrarEventoPorDebug"); 
	}

	// 1. Clean old markers
	TypedArray<Node> children = self->get_children();
	for (int i = 0; i < children.size(); i++) {
		Node* child = Object::cast_to<Node>(children[i]);
		if (child && (String(child->get_name()).begins_with("Marcador_") || child->is_class("ColorRect"))) {
			self->remove_child(child); 
			child->queue_free();
		}
	}

	TileMapLayer* board = Object::cast_to<TileMapLayer>(self->get_node_or_null("Tablero"));
	if (board) board->clear();

	// WE REGENERATE BY INVOKING THE CREATED METHODS (No Jenova errors)
	self->call("ConstruirLogica");
	self->call("ConstruirVisual");

	if (m_party_manager) {
		TypedArray<Node> heroes = m_party_manager->get_children();
		int hero_index = 0; 
		
		for (int i = 0; i < heroes.size(); i++) {
			Node* hero = Object::cast_to<Node>(heroes[i]);
			
			if (hero && hero->get_name() != StringName("DatosEquipo")) {
				
				// THE LOCK FOR THE DEAD!
				bool is_dead = hero->has_meta("esta_muerto") ? (bool)hero->get_meta("esta_muerto") : false;
				
				String name = hero->get_name();
				Node2D* visual_token = Object::cast_to<Node2D>(self->get_node_or_null(NodePath(name)));
				
				if (visual_token && board) {
					// If they are dead, we hide them and skip everything else
					if (is_dead) {
						visual_token->set_visible(false);
						continue; // Move directly to the next hero
					}

					Vector2 offset(0, 0);
					if (hero_index == 0) offset = Vector2(-1, -1); 
					if (hero_index == 1) offset = Vector2(1, -1);  
					if (hero_index == 2) offset = Vector2(1, 1);   
					if (hero_index == 3) offset = Vector2(-1, 1);  
					
					Vector2 center_pos = board->map_to_local(Vector2i(0,0));
					visual_token->set_position(center_pos + offset);
					visual_token->set_visible(true); 
				}
				
				hero->set_meta("grid_pos", Vector2i(0,0));
				hero_index++;
			}
		}
	}
	UtilityFunctions::print("[DungeonManager] NEW FLOOR GENERATED SUCCESSFULLY!");
}

JENOVA_SCRIPT_END
