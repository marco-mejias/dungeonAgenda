/* Jenova C++ Node Base Script (Meteora) - CameraManager */

// Godot SDK
#include <Godot/godot.hpp>
#include <Godot/classes/node.hpp>
#include <Godot/classes/node2d.hpp>
#include <Godot/classes/camera2d.hpp>
#include <Godot/classes/input.hpp>
#include <Godot/classes/window.hpp>
#include <Godot/classes/tile_map_layer.hpp>
#include <Godot/variant/utility_functions.hpp>

// Namespaces
using namespace godot;
using namespace jenova::sdk;

// Usamos un namespace anónimo para evitar LNK2005
namespace {
	int map_grid_size = 0; // Guardaremos aquí el tamaño del mapa
}

JENOVA_SCRIPT_BEGIN

// =======================================================
// 1. CONFIGURACIÓN INICIAL (Lo llama el DungeonManager)
// =======================================================
void ConfigurarLimitesMapa(Caller* instance, int grid_size_del_juego) {
	map_grid_size = grid_size_del_juego;
	
	Camera2D* cam = GetSelf<Camera2D>(instance);
	if (cam) {
		float tamano_baldosa = 16.0f;
		float ancho_mapa = map_grid_size * tamano_baldosa;
		float alto_mapa = map_grid_size * tamano_baldosa;
		int margen = 100;
		
		cam->set_limit(godot::SIDE_LEFT, -margen);
		cam->set_limit(godot::SIDE_TOP, -margen);
		cam->set_limit(godot::SIDE_RIGHT, ancho_mapa + margen);
		cam->set_limit(godot::SIDE_BOTTOM, alto_mapa + margen);
		
		godot::UtilityFunctions::print("[CameraManager] Límites configurados para mapa de tamaño: ", map_grid_size);
	}
}

void OnAwake(Caller* instance) {}
void OnDestroy(Caller* instance) {}
void OnReady(Caller* instance) {}

// =======================================================
// 2. EL CEREBRO EN TIEMPO REAL (Tu código original pulido)
// =======================================================
void OnProcess(Caller* instance, double _delta) 
{
	Camera2D* cam = GetSelf<Camera2D>(instance);
	godot::Input* input = godot::Input::get_singleton();
	if (!cam || !input) return;

	Node* padre_dungeon = cam->get_parent();
	if (!padre_dungeon) return;

	// === MATEMÁTICAS DINÁMICAS AUTÓNOMAS (Límites y Zoom) ===
	godot::TileMapLayer* board = godot::Object::cast_to<godot::TileMapLayer>(padre_dungeon->get_node_or_null("Tablero"));
	if (!board) return;

	// Leemos el tamaño real de los tiles dibujados en lugar de una variable manual
	godot::Rect2i limites = board->get_used_rect();
	float tamano_baldosa = 16.0f;
	float ancho_mapa = limites.size.width * tamano_baldosa;
	float alto_mapa = limites.size.height * tamano_baldosa;
	int margen = 100;

	Vector2 tamano_pantalla = cam->get_viewport()->get_visible_rect().size;
	
	float zoom_minimo = 0.5f;
	if (ancho_mapa > 0 && alto_mapa > 0) {
		float zoom_fit_x = tamano_pantalla.x / (ancho_mapa + (margen * 2));
		float zoom_fit_y = tamano_pantalla.y / (alto_mapa + (margen * 2));
		
		// Usar std::max garantiza que la cámara NUNCA muestre el vacío fuera del mapa
		zoom_minimo = std::max(zoom_fit_x, zoom_fit_y);
	}
	
	float zoom_maximo = 5.0f; 
	if (zoom_minimo >= zoom_maximo) zoom_minimo = zoom_maximo - 0.5f;

	float factor_zoom = cam->get_zoom().x;
	float velocidad_zoom = 0.15f;
	
	if (input->is_action_just_pressed("zoom_in")) factor_zoom = godot::UtilityFunctions::clamp(factor_zoom + velocidad_zoom, zoom_minimo, zoom_maximo);
	if (input->is_action_just_pressed("zoom_out")) factor_zoom = godot::UtilityFunctions::clamp(factor_zoom - velocidad_zoom, zoom_minimo, zoom_maximo);
	
	cam->set_zoom(Vector2(factor_zoom, factor_zoom));

	// === BUSCAR AL PROTAGONISTA AUTÓNOMAMENTE ===
	String current_char_name = "";
	Node2D* active_token = nullptr;
	
	// Buscamos el PartyManager asumiendo que está en la raíz de tu escena Main
	Node* pm = cam->get_node_or_null("/root/Main/PartyManager");
	if (pm) {
		Node* act_char = godot::Object::cast_to<Node>(pm->call("GetActiveCharacter"));
		if (act_char) {
			current_char_name = act_char->get_name();
			// El sprite físico es hijo del DungeonManager (nuestro padre)
			active_token = godot::Object::cast_to<Node2D>(padre_dungeon->get_node_or_null(NodePath(current_char_name)));
		}
	}

	// === LÓGICA DE SEGUIMIENTO (Tuya 100%) ===
	Vector2 target_pos = cam->get_position();
	bool is_dragging = input->is_mouse_button_pressed(godot::MOUSE_BUTTON_RIGHT);

	if (is_dragging) {
		cam->set_meta("auto_centrar", false);
	}

	String last_char = cam->has_meta("last_char") ? (String)cam->get_meta("last_char") : String("");
	if (current_char_name != "" && current_char_name != last_char) {
		cam->set_meta("auto_centrar", true);
		cam->set_meta("last_char", current_char_name);
	}

	bool is_moving = padre_dungeon->has_meta("is_moving") && (bool)padre_dungeon->get_meta("is_moving");

	if (factor_zoom <= zoom_minimo + 0.02f && map_grid_size > 0) {
		Vector2 centro_mapa(ancho_mapa / 2.0f, alto_mapa / 2.0f);
		target_pos = target_pos.lerp(centro_mapa, 10.0f * _delta);
	} 
	else if (is_dragging) {
		Vector2 raton_actual = cam->get_viewport()->get_mouse_position();
		if (cam->has_meta("raton_anterior")) {
			Vector2 raton_anterior = (Vector2)cam->get_meta("raton_anterior"); // Cast seguro para C++
			Vector2 delta = raton_anterior - raton_actual;
			target_pos = target_pos + (delta / factor_zoom);
		}
	}
	else if (active_token && (is_moving || (cam->has_meta("auto_centrar") && (bool)cam->get_meta("auto_centrar")))) {
		// Aquí compensamos el espacio de tu UI izquierda como querías (offset)
		float offset_pantalla_x = 0.0f; // Ajusta este número para dejar espacio a los retratos UI
		Vector2 offset_mundo = Vector2(offset_pantalla_x / factor_zoom, 0.0f);
		
		Vector2 char_pos = active_token->get_position() + offset_mundo;
		target_pos = target_pos.lerp(char_pos, 8.0f * _delta);
		
		if (target_pos.distance_to(char_pos) < 1.0f) {
			cam->set_meta("auto_centrar", false);
		}
	}

	cam->set_meta("raton_anterior", cam->get_viewport()->get_mouse_position());
	cam->set_position(target_pos);
}

JENOVA_SCRIPT_END
