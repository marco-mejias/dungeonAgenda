/* Jenova C++ Node Base Script (Meteora) - InputManager */

// Godot SDK
#include <Godot/godot.hpp>
#include <Godot/classes/node.hpp>
#include <Godot/classes/tile_map_layer.hpp>
#include <Godot/classes/line2d.hpp>
#include <Godot/classes/input.hpp>
#include <Godot/classes/control.hpp>
#include <Godot/variant/utility_functions.hpp>

// Namespaces
using namespace godot;
using namespace jenova::sdk;

// Self Instance
namespace {
	Vector2i last_hovered_cell = Vector2i(-1, -1);
	bool debug_key_pressed = false;
	bool debug_mode = false; // El InputManager debe recordar si el debug está activo
}

JENOVA_SCRIPT_BEGIN

void OnAwake(Caller* instance) {}
void OnDestroy(Caller* instance) {}
void OnReady(Caller* instance) {}

// === 1. EL ONPROCESS (EL VIGÍA) ===
void OnProcess(Caller* instance, double _delta) 
{
	godot::Node* self_node = GetSelf<godot::Node>(instance);
	godot::Node* padre_dungeon = self_node->get_parent();
	if (!padre_dungeon) return;

	// === DETECTAR HOVER DEL RATÓN ===
	if (!padre_dungeon->get_node_or_null("MenuTactico_Instancia")) {
		godot::TileMapLayer* board = godot::Object::cast_to<godot::TileMapLayer>(padre_dungeon->get_node_or_null("Tablero"));
		
		if (board) {
			Vector2 mouse_global = board->get_global_mouse_position();
			Vector2 local_pos = board->to_local(mouse_global);
			Vector2i current_hover = board->local_to_map(local_pos);

			if (current_hover != last_hovered_cell) {
				last_hovered_cell = current_hover;
				padre_dungeon->call("ActualizarHover", current_hover.x, current_hover.y);
			}
		}
	}

	// === DETECTAR TECLADO (F9 DEBUG) ===
	godot::Input* input = godot::Input::get_singleton();
	if (input) {
		bool is_f9_pressed = input->is_physical_key_pressed(godot::Key::KEY_F9);
		
		if (is_f9_pressed && !debug_key_pressed) {
			debug_key_pressed = true;
			debug_mode = !debug_mode; // Cambiamos nuestro estado interno
			padre_dungeon->call("ToggleModoDebug");
		} 
		else if (!is_f9_pressed) {
			debug_key_pressed = false; 
		}
	}
}

// === 2. GESTIÓN DEL CLIC IZQUIERDO ===
void HandleMapClick(Caller* instance, double mouse_x, double mouse_y)
{
	godot::Node* self_node = GetSelf<godot::Node>(instance);
	godot::Node* padre_dungeon = self_node->get_parent();
	if (!padre_dungeon) return;

	if (padre_dungeon->has_meta("bloqueo_evento") && (bool)padre_dungeon->get_meta("bloqueo_evento")) return;

	TileMapLayer* board = Object::cast_to<TileMapLayer>(padre_dungeon->get_node_or_null("Tablero"));
	if (!board) return;

	// 1. OBTENER COORDENADAS
	Vector2 global_mouse = board->get_global_mouse_position();
	Vector2 local_pos = board->to_local(global_mouse);
	Vector2i grid_pos = board->local_to_map(local_pos); 

	int x = grid_pos.x;
	int y = grid_pos.y;

	// 2. LIMPIAR LA LÍNEA DE RUTA
	Line2D* path_line = Object::cast_to<Line2D>(padre_dungeon->get_node_or_null("PathLine"));
	if (path_line) path_line->clear_points();

	// 3. MODO DEBUG: TELETRANSPORTE
	if (debug_mode) {
		padre_dungeon->call("TeletransporteDebug", x, y);
		return;
	}

	// 4. GESTIÓN DEL MENÚ TÁCTICO (PROTEGER INTERFAZ)
	Node* menu_activo = padre_dungeon->get_node_or_null("MenuTactico_Instancia");
	if (menu_activo) {
		godot::Control* menu_ctrl = godot::Object::cast_to<godot::Control>(menu_activo);
		if (menu_ctrl) {
			godot::Vector2 raton_local = menu_ctrl->get_local_mouse_position();
			godot::Rect2 area_menu = godot::Rect2(godot::Vector2(0, 0), menu_ctrl->get_size());
			
			if (area_menu.has_point(raton_local)) {
				return; // El clic fue dentro del menú UI, ignoramos el mapa
			}
		}
		
		// Clic fuera del menú: lo cerramos y abortamos la caminata
		menu_activo->queue_free();
		return; 
	}

	// 5. DELEGAR EL ANÁLISIS DE LA CASILLA AL DUNGEON MANAGER
	// Como el InputManager no tiene map_data, le pasa las coordenadas al DungeonManager
	// para que él decida si es un movimiento normal, abrir un cofre, o abrir el menú táctico.
	padre_dungeon->call("ProcesarClicEnCasilla", x, y);
}

JENOVA_SCRIPT_END
