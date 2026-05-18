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
#include <Godot/classes/point_light2d.hpp>
#include <Godot/classes/tile_data.hpp>
#include <Godot/classes/interval_tweener.hpp>
#include <Godot/classes/dir_access.hpp>
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
	Vector2i atlas_coords; // Memory for tile coordinates
	bool is_chest = false;
	bool chest_opened = false;
};

namespace {
	static Node2D* self = nullptr;
	const int GRID_SIZE = 12;
	
	std::vector<std::vector<Cell>> map_data;
	Vector2i last_hovered_cell = Vector2i(-1, -1); // Memory for mouse hover
	
	Ref<AStarGrid2D> astar;

	std::vector<String> event_paths; 
	Node* m_party_manager = nullptr;   // Cache for PartyManager
	Node* m_ui_manager = nullptr;      // Cache for UI
	TileMapLayer* m_board = nullptr;
	Line2D* m_path_line = nullptr;
}

JENOVA_SCRIPT_BEGIN

// ==========================================
// VISUAL DUNGEON FUNCTIONS
// ==========================================

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

void ComprobarCasillaEvento(Caller* instance, int grid_x, int grid_y) {
	if (!self) return;
	
	Vector2i stepped_pos(grid_x, grid_y);

	Array event_tiles;
	if (self->has_meta("lista_eventos")) {
		event_tiles = self->get_meta("lista_eventos");
	}

	if (event_tiles.has(stepped_pos)) {
		UtilityFunctions::print("[DungeonManager]: Event in tile", grid_x, ", ", grid_y);

		if (event_paths.empty()) {
			UtilityFunctions::print("[DungeonManager]: Event folder is empty or not loaded properly.");
			return;
		}

		int random_index = UtilityFunctions::randi_range(0, event_paths.size() - 1);
		String event_path = event_paths[random_index];
		
		Ref<Resource> loaded_event = ResourceLoader::get_singleton()->load(event_path);

		if (loaded_event.is_valid()) {
			Node* real_ui_manager = self->get_parent()->get_node_or_null("CanvasLayer");
			
			if (real_ui_manager) { 
				self->set_meta("bloqueo_evento", true);
				
				// 1. Create text
				Label* event_text = memnew(Label);
				event_text->set_text("EVENTO");
				event_text->add_theme_font_size_override("font_size", 120);
				event_text->add_theme_color_override("font_color", Color(1.0, 0.1, 0.1));
				event_text->set_horizontal_alignment(godot::HORIZONTAL_ALIGNMENT_CENTER);
				event_text->set_vertical_alignment(godot::VERTICAL_ALIGNMENT_CENTER);

				// Put it in the CanvasLayer to draw over the map
				real_ui_manager->add_child(event_text);

				// Adjust size and place it below the screen
				Vector2 screen_size = self->get_viewport_rect().get_size();
				event_text->set_size(Vector2(screen_size.x, 200));
				event_text->set_position(Vector2(0, screen_size.y + 100));

				// Pass the active character so we don't lose it during the event
				Node* act_char = Object::cast_to<Node>(m_party_manager->call("GetActiveCharacter"));
				if (act_char) {
					real_ui_manager->set_meta("heroe_en_peligro", act_char);
				}

				// 2. Tween
				Ref<Tween> tween = self->create_tween();

				tween->tween_property(event_text, "position:y", screen_size.y / 2.0 - 100.0, 0.4)
					 ->set_trans(Tween::TRANS_EXPO)
					 ->set_ease(Tween::EASE_OUT);
				tween->tween_interval(0.6);
				tween->tween_property(event_text, "position:y", -300.0, 0.3)
					 ->set_trans(Tween::TRANS_EXPO)
					 ->set_ease(Tween::EASE_IN);
					
				// Parallel: show the event panel
				Callable show_panel = Callable(real_ui_manager, StringName("MostrarEventoVisual")).bind(loaded_event);
				tween->parallel()->tween_callback(show_panel);

				// Destroy to free memory
				tween->tween_callback(Callable(event_text, StringName("queue_free")));
			}
		}

		// Disable event
		event_tiles.erase(stepped_pos);
		self->set_meta("lista_eventos", event_tiles);
		
		// Remove debug pink square
		String visual_marker_name = "Marcador_Evento_" + String::num_int64(grid_x) + "_" + String::num_int64(grid_y);
		Node* visual_marker = self->get_node_or_null(NodePath(visual_marker_name));
		if (visual_marker) {
			visual_marker->queue_free();
		}
	}
}

void FinalizarEvento(Caller* instance) {
	if (!self) return;
	self->set_meta("bloqueo_evento", false);
	UtilityFunctions::print("[DungeonManager] Event completed. Map unlocked.");
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

void UpdateTileMapCell(Caller* instance, int x, int y) 
{
	if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE) return;

	TileMapLayer* board = Object::cast_to<TileMapLayer>(self->get_node_or_null("Tablero"));
	if (!board) return;

	Cell& cell = map_data[x][y];
	Vector2i coords = Vector2i(x, y);
	
	if (cell.is_highlighted) {
		board->set_cell(coords, 1, Vector2i(9, 6), 0); // Walkable 
	} 
	else if (!cell.is_explored) {
		board->set_cell(coords, 1, Vector2i(8, 7), 0); // Fog 
	} 
	else {
		// Paint definitive texture
		board->set_cell(coords, 1, cell.atlas_coords, 0); 
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

		// If we hit a wall, we can SEE the wall, but vision stops there
		if (map_data[x0][y0].is_wall) {
			if (x0 == x1 && y0 == y1) return true; 
			return false; 
		}

		if (x0 == x1 && y0 == y1) break;

		e2 = 2 * err;
		int prev_x = x0;
		int prev_y = y0;

		if (e2 >= dy) { err += dy; x0 += sx; }
		if (e2 <= dx) { err += dx; y0 += sy; }

		// Strict corner control
		// If we made a diagonal move (X and Y changed at the same time)
		if (x0 != prev_x && y0 != prev_y) {
			bool wall1 = false;
			bool wall2 = false;
			
			// Check orthogonal tile 1
			if (x0 >= 0 && x0 < GRID_SIZE && prev_y >= 0 && prev_y < GRID_SIZE) {
				wall1 = map_data[x0][prev_y].is_wall;
			}
			// Check orthogonal tile 2
			if (prev_x >= 0 && prev_x < GRID_SIZE && y0 >= 0 && y0 < GRID_SIZE) {
				wall2 = map_data[prev_x][y0].is_wall;
			}

			// If at least one of the side tiles is a wall, vision hits the corner
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
		UtilityFunctions::print("[DungeonManager] Visual scene not found: ", prefab_path);
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
	Vector2i grid_pos = Vector2i(0, 0); 
	
	if (board) {
		Vector2 cell_center_pos = board->map_to_local(grid_pos);
		visual_token->set_scale(Vector2(0.05, 0.05));
		
		// Spawn characters separated: WE SHOULD MAKE IT SO WHEN THEY MOVE THEY ARE ALSO SEPARATED
		Vector2 offset(0, 0);
		if (position_index == 0) offset = Vector2(-1, -1); 
		if (position_index == 1) offset = Vector2(1, -1);  
		if (position_index == 2) offset = Vector2(1, 1);   
		if (position_index == 3) offset = Vector2(-1, 1);  

		visual_token->set_position(cell_center_pos + offset);
	}

	self->add_child(visual_token);
	UtilityFunctions::print("[DungeonManager] ", name, " loaded in the dungeon");
	
	if (m_party_manager) {
		Node* character = m_party_manager->get_node_or_null(NodePath(name));
		if (character) {
			character->set_meta("grid_pos", grid_pos);
		}
	}
}

void DescubrirNiebla(Caller* instance, int center_x, int center_y, int vision_radius) {
	// 1. First we calculate the character's normal vision
	for (int x = 0; x < GRID_SIZE; ++x) {
		for (int y = 0; y < GRID_SIZE; ++y) {
			if (abs(x - center_x) + abs(y - center_y) <= vision_radius) {
				if (CheckLineOfSight(Vector2i(center_x, center_y), Vector2i(x, y))) {
					map_data[x][y].is_explored = true;
				}
			}
		}
	}

	// 2. ROGUELIKE TRICK: If a floor is visible, we illuminate the surrounding walls
	std::vector<Vector2i> walls_to_reveal;
	
	for (int x = 0; x < GRID_SIZE; ++x) {
		for (int y = 0; y < GRID_SIZE; ++y) {
			// If this tile is floor and we are seeing it...
			if (map_data[x][y].is_explored && !map_data[x][y].is_wall) {
				// We check the 8 surrounding tiles
				for (int dx = -1; dx <= 1; dx++) {
					for (int dy = -1; dy <= 1; dy++) {
						int nx = x + dx;
						int ny = y + dy;
						// If we don't go out of the map...
						if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE) {
							// And it's a hidden wall, we mark it to be revealed
							if (map_data[nx][ny].is_wall && !map_data[nx][ny].is_explored) {
								walls_to_reveal.push_back(Vector2i(nx, ny));
							}
						}
					}
				}
			}
		}
	}
	
	// Reveal those outline walls
	for (Vector2i wall : walls_to_reveal) {
		map_data[wall.x][wall.y].is_explored = true;
	}

	// 3. We update the whole board draw at once (prevents graphical flickering)
	for (int x = 0; x < GRID_SIZE; ++x) {
		for (int y = 0; y < GRID_SIZE; ++y) {
			UpdateTileMapCell(instance, x, y);
		}
	}
}

void PasoDado(Caller* instance, int x, int y, int vision) {
	if (!self) return;
	DescubrirNiebla(instance, x, y, vision);
}

// Function to indicate available movement tiles for each character
void HighlightReachableCells(Caller* instance, int start_x, int start_y, int movement_points) 
{
	for (int x = 0; x < GRID_SIZE; ++x) {
		for (int y = 0; y < GRID_SIZE; ++y) {
			map_data[x][y].is_highlighted = false;
			UpdateTileMapCell(instance, x, y); 
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

				if (map_data[next_x][next_y].is_wall) {
					UpdateTileMapCell(instance, next_x, next_y); 
				} 
				else if (!visited[next_x][next_y]) {
					visited[next_x][next_y] = true;
					map_data[next_x][next_y].is_highlighted = true; 
					UpdateTileMapCell(instance, next_x, next_y);
					
					queue.push({dest_pos, current_cost + 1});
				}
			}
		}
	}
	if (astar.is_valid()) {
		for (int x = 0; x < GRID_SIZE; ++x) {
			for (int y = 0; y < GRID_SIZE; ++y) {
				// If it's a stone wall, OR if the tile is NOT highlighted, we block it in A*
				if (map_data[x][y].is_wall || !map_data[x][y].is_highlighted) {
					astar->set_point_solid(Vector2i(x, y), true);
				} else {
					// If it's floor and it is yellow, we give permission to step on it
					astar->set_point_solid(Vector2i(x, y), false);
				}
			}
		}
	}
}

void UpdateHighlightForWarrior(Caller* instance) 
{
	if (!m_party_manager) return;

	Node* act_char = Object::cast_to<Node>(m_party_manager->call("GetActiveCharacter"));
	
	if (act_char) {
		int dex = m_party_manager->call("GetCurrentStat","velocidad");
		Vector2i pos = m_party_manager->call("GetActiveGridPos");

		DescubrirNiebla(instance, pos.x, pos.y, dex);
		HighlightReachableCells(instance, pos.x, pos.y, dex);
	} else {
		UtilityFunctions::print("[Dungeon] No active character triggered.");
	}
}

// Function before finishing the character's turn
void AtLast(Caller* instance, int x, int y) {
	if (!self) return;
	
	self->set_meta("is_moving", false);

	if (m_party_manager) {
		m_party_manager->call("SetActiveGridPos", x, y);
	}
	
	// 1. ESCAPE LOGIC
	if (self->has_meta("casilla_salida")) {
		Vector2i exit_pos = self->get_meta("casilla_salida");
		
		if (x == exit_pos.x && y == exit_pos.y) {
			if (m_party_manager) {
				Node* act_char = Object::cast_to<Node>(m_party_manager->call("GetActiveCharacter"));
				if (act_char) {
					// We tell the manager that this hero has escaped
					m_party_manager->call("PersonajeEscapa", act_char);
					
					// We hide their visual token on the map to make it look like they went down
					String node_name = act_char->get_name(); 
					Node2D* token = Object::cast_to<Node2D>(self->get_node_or_null(NodePath(node_name)));
					if (token) {
						token->set_visible(false);
					}
				}
			}
		}
	}

	// 2. CHEST LOGIC
	if (map_data[x][y].is_chest && !map_data[x][y].chest_opened) {
		Node* hero = nullptr;
		if (m_party_manager) hero = Object::cast_to<Node>(m_party_manager->call("GetActiveCharacter"));
		
		if (hero) {
			int roll = rand() % 3; // 0: Gold, 1: Food, 2: Item
			
			if (roll == 0) {
				int gold_gained = (rand() % 20) + 10;
				int current = hero->has_meta("oro") ? (int)hero->get_meta("oro") : 0;
				hero->set_meta("oro", current + gold_gained);
				UtilityFunctions::print("[Botin] ", hero->get_name(), " found ", gold_gained, " gold!");
			} 
			else if (roll == 1) {
				int food_gained = (rand() % 2) + 1;
				int current = hero->has_meta("comida_bolsa") ? (int)hero->get_meta("comida_bolsa") : 0;
				hero->set_meta("comida_bolsa", current + food_gained);
				UtilityFunctions::print("[Botin] ", hero->get_name(), " found food!");
			}
			else {
				// We get the current inventory
				TypedArray<Resource> inv = hero->has_meta("items_lista") ? (TypedArray<Resource>)hero->get_meta("items_lista") : TypedArray<Resource>();
				int max_s = hero->has_meta("max_slots") ? (int)hero->get_meta("max_slots") : 3;
				
				if (inv.size() < max_s) {
					// 1. LOAD THE ITEM FROM FILES
					// Make sure the path matches where you saved the .tres in Godot
					Ref<Resource> new_item = godot::ResourceLoader::get_singleton()->load("res://Data/Items/Espada.tres");
					
					if (new_item.is_valid()) {
						// 2. PUT IT IN THE BACKPACK
						inv.push_back(new_item);
						hero->set_meta("items_lista", inv); // Save the updated backpack
						
						UtilityFunctions::print("[Botin] ", hero->get_name(), " found and stored a sword!");
					} else {
						UtilityFunctions::print("[Botin] Error: Item file not found.");
					}
				} else {
					UtilityFunctions::print("[Botin] Inventory full. Item lost.");
				}
			}
		}

		map_data[x][y].chest_opened = true;
		TileMapLayer* board = Object::cast_to<TileMapLayer>(self->get_node_or_null("Tablero"));
		if (board) {
			board->set_cell(Vector2i(x, y), 0, Vector2i(4, 8)); //Chest sprite tilemap
		}

		String chest_marker_name = "Marcador_Cofre_" + String::num_int64(x) + "_" + String::num_int64(y);
		Node* chest_marker = self->get_node_or_null(NodePath(chest_marker_name));
		if (chest_marker) {
			chest_marker->queue_free(); // Remove it from the screen
		}
	}

	// 3. EVENT AND TURN LOGIC
	ComprobarCasillaEvento(instance, x, y);

	if (m_party_manager) {
		m_party_manager->call("TerminarTurnoPersonaje");
	}

	if (m_ui_manager) {
		m_ui_manager->call("UpdateWarriorUI");
		UtilityFunctions::print("[DungeonManager] Turn change. UI updated.");
	} else {
		UtilityFunctions::print("[DungeonManager] CanvasLayer node not found to update UI.");
	}

	UpdateHighlightForWarrior(instance);
}

void HandleMapClick(Caller* instance, double mouse_x, double mouse_y)
{
	if (self->has_meta("bloqueo_evento") && (bool)self->get_meta("bloqueo_evento")) {
		return;
	}
	TileMapLayer* board = Object::cast_to<TileMapLayer>(self->get_node_or_null("Tablero"));
	if (!board) return;

	Vector2 global_mouse = Vector2(mouse_x, mouse_y);
	Vector2 local_pos = board->to_local(global_mouse);
	
	Vector2i grid_pos = board->local_to_map(local_pos); 

	Line2D* path_line = Object::cast_to<Line2D>(self->get_node_or_null("PathLine"));
	if (path_line) {
		path_line->clear_points();
	}

	int x = grid_pos.x;
	int y = grid_pos.y;

	if (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE) {
		if (map_data[x][y].is_highlighted) {
			
			if (self->has_meta("is_moving") && (bool)self->get_meta("is_moving")) return;
			
			self->set_meta("is_moving", true);
			
			if (!m_party_manager) return;

			Node* act_char = Object::cast_to<Node>(m_party_manager->call("GetActiveCharacter"));
			if (!act_char) return;

			String node_name = act_char->get_name(); 
			Node2D* token = Object::cast_to<Node2D>(self->get_node_or_null(NodePath(node_name)));
			
			if (token) {
				Vector2i current_pos = m_party_manager->call("GetActiveGridPos");
				Vector2i dest_pos = Vector2i(x, y);
				TypedArray<Vector2i> path = astar->get_id_path(current_pos, dest_pos);
				
				for (int cx = 0; cx < GRID_SIZE; ++cx) {
					for (int cy = 0; cy < GRID_SIZE; ++cy) {
						if (map_data[cx][cy].is_highlighted) {
							map_data[cx][cy].is_highlighted = false;
							UpdateTileMapCell(instance, cx, cy);
						}
					}
				}
				int dex = m_party_manager->call("GetCurrentStat", "velocidad");
				Ref<Tween> tween = self->create_tween();
				
				// Walking animation
				float time_per_step = 0.4; 

				for (int i = 1; i < path.size(); i++) {
					Vector2i step = path[i];
					Vector2 pixel_target = board->map_to_local(step);
					
					tween->tween_property(token, "position", pixel_target, time_per_step)
						 ->set_trans(Tween::TRANS_LINEAR);

					float tilt = (i % 2 == 0) ? 0.2 : -0.2; 
					
					tween->parallel()->tween_property(token, "rotation", tilt, time_per_step)
						 ->set_trans(Tween::TRANS_SINE)
						 ->set_ease(Tween::EASE_IN_OUT);
					
					Callable step_fn = Callable(self, StringName("PasoDado")).bind(step.x, step.y, dex);
					tween->tween_callback(step_fn);
				}

				tween->tween_property(token, "rotation", 0.0, 0.15);

				Callable last_move = Callable(self, StringName("AtLast")).bind(x, y);
				tween->connect("finished", last_move);
			}
		}
	}
}

// ==========================================
// DUNGEON CONSTRUCTION START
// ==========================================

void ConstruirLogica(Caller* instance)
{
	Node* parent_node = self->get_parent();
	if (parent_node) {
		m_party_manager = parent_node->get_node_or_null("PartyManager");
		m_ui_manager = parent_node->get_node_or_null("CanvasLayer");
	}
	
	m_board = Object::cast_to<TileMapLayer>(self->get_node_or_null("Tablero"));
	m_path_line = Object::cast_to<Line2D>(self->get_node_or_null("PathLine"));

	map_data.resize(GRID_SIZE, std::vector<Cell>(GRID_SIZE));
	
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(1, 100);

	bool is_map_valid = false;
	int attempts = 0; 
	
	while (!is_map_valid && attempts < 1000) {
		attempts++;
		int total_floor_cells = 0;

		for (int x = 0; x < GRID_SIZE; ++x) {
			for (int y = 0; y < GRID_SIZE; ++y) {
				map_data[x][y].grid_pos = Vector2i(x, y);
				map_data[x][y].is_explored = false; 
				map_data[x][y].is_highlighted = false;
				
				map_data[x][y].is_chest = false;
				map_data[x][y].chest_opened = false;
				
				if (dis(gen) <= 35 && !(x == 0 && y == 0)) { 
					map_data[x][y].is_wall = true;
				} else {
					map_data[x][y].is_wall = false;
					total_floor_cells++; 
				}
			}
		}

		std::vector<std::vector<bool>> visited(GRID_SIZE, std::vector<bool>(GRID_SIZE, false));
		std::queue<Vector2i> cells_to_check;
		
		cells_to_check.push(Vector2i(0, 0));
		visited[0][0] = true;
		int reachable_floors = 0;

		std::vector<Vector2i> directions = {
			Vector2i(0, -1), Vector2i(0, 1), Vector2i(-1, 0), Vector2i(1, 0)
		};

		while (!cells_to_check.empty()) {
			Vector2i current = cells_to_check.front();
			cells_to_check.pop();
			reachable_floors++; 

			for (const Vector2i& dir : directions) {
				int next_x = current.x + dir.x;
				int next_y = current.y + dir.y;

				if (next_x >= 0 && next_x < GRID_SIZE && next_y >= 0 && next_y < GRID_SIZE) {
					if (!map_data[next_x][next_y].is_wall && !visited[next_x][next_y]) {
						visited[next_x][next_y] = true;
						cells_to_check.push(Vector2i(next_x, next_y));
					}
				}
			}
		}

		if (reachable_floors == total_floor_cells) {
			is_map_valid = true;
			UtilityFunctions::print("Valid map generated on attempt number: ", attempts);
			
			astar.instantiate(); 
			astar->set_region(Rect2i(0, 0, GRID_SIZE, GRID_SIZE)); 
			astar->set_cell_size(Vector2(16, 16)); 
			astar->set_default_compute_heuristic(AStarGrid2D::HEURISTIC_MANHATTAN); 
			astar->set_diagonal_mode(AStarGrid2D::DIAGONAL_MODE_NEVER); 
			astar->update(); 

			for (int x = 0; x < GRID_SIZE; ++x) {
				for (int y = 0; y < GRID_SIZE; ++y) {
					if (map_data[x][y].is_wall) {
						astar->set_point_solid(Vector2i(x, y), true);
					}
				}
			}
			
			Array event_tiles;
			std::vector<Vector2i> valid_floors;

			for (int x = 0; x < GRID_SIZE; ++x) {
				for (int y = 0; y < GRID_SIZE; ++y) {
					if (!map_data[x][y].is_wall && !(x == 0 && y == 0)) {
						valid_floors.push_back(Vector2i(x, y));
					}
				}
			}
			
			int events_to_create = 20;
			while (event_tiles.size() < events_to_create && !valid_floors.empty()) {
				int rand_idx = UtilityFunctions::randi_range(0, valid_floors.size() - 1);
				event_tiles.push_back(valid_floors[rand_idx]);
				valid_floors.erase(valid_floors.begin() + rand_idx);
			}

			self->set_meta("lista_eventos", event_tiles);
						
			bool exit_placed = false;
			Vector2i exit_pos;

			while (!exit_placed) {
				int rx = UtilityFunctions::randi_range(GRID_SIZE / 2, GRID_SIZE - 1);
				int ry = UtilityFunctions::randi_range(GRID_SIZE / 2, GRID_SIZE - 1);
				Vector2i try_pos(rx, ry);

				if (!map_data[rx][ry].is_wall && !event_tiles.has(try_pos)) {
					exit_pos = try_pos;
					exit_placed = true;
				}
			}

			self->set_meta("casilla_salida", exit_pos);
		}
	}
	GenerarCofres(instance);
}

void ConstruirVisual(Caller* instance)
{
	UtilityFunctions::print("Starting visual render on TileMap...");

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
			if (map_data[x][y].is_wall) {
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
	DibujarMarcadoresEventos(instance);
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
	if (!m_board || !m_path_line) return;

	Vector2 mouse_pos = self->get_local_mouse_position();
	Vector2i current_hover = m_board->local_to_map(mouse_pos);

	if (current_hover == last_hovered_cell) return;

	UpdateTileMapCell(instance, last_hovered_cell.x, last_hovered_cell.y);
	last_hovered_cell = current_hover;

	m_path_line->clear_points(); 

	if (current_hover.x < 0 || current_hover.x >= GRID_SIZE || current_hover.y < 0 || current_hover.y >= GRID_SIZE) return;

	if (map_data[current_hover.x][current_hover.y].is_highlighted && astar.is_valid()) {
		if (m_party_manager) {
			Node* act_char = Object::cast_to<Node>(m_party_manager->call("GetActiveCharacter"));
			if (act_char) {
				Vector2i current_pos = m_party_manager->call("GetActiveGridPos");
				TypedArray<Vector2i> path = astar->get_id_path(current_pos, current_hover);
				
				for (int i = 0; i < path.size(); i++) {
					Vector2i step = path[i];
					Vector2 pixel_pos = m_board->map_to_local(step); 
					m_path_line->add_point(pixel_pos);
				}
			}
		}
	}
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
	Ref<Tween> tween = self->create_tween();
	tween->tween_property(self, "position", Vector2(650, -700), 0.5)
		 ->set_trans(Tween::TRANS_CUBIC)
		 ->set_ease(Tween::EASE_IN_OUT);
}

void BajarMapa(Caller* instance) {
	if (!self) return;
	Ref<Tween> tween = self->create_tween();
	tween->tween_property(self, "position", Vector2(650, 100.0), 0.5)
		 ->set_trans(Tween::TRANS_CUBIC)
		 ->set_ease(Tween::EASE_OUT);
}

void AvanzarAlSiguientePiso(Caller* instance) {
	if (!self) return;
	
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
