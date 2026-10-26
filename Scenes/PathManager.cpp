/* Jenova C++ Node Base Script (Meteora) - PathManager */

#include <Godot/godot.hpp>
#include <Godot/classes/node.hpp>
#include <Godot/classes/node2d.hpp>
#include <Godot/classes/a_star_grid2d.hpp>
#include <Godot/classes/tween.hpp>
#include <Godot/classes/tile_map_layer.hpp>
#include <Godot/variant/utility_functions.hpp>
#include <Godot/classes/property_tweener.hpp> 
#include <Godot/classes/callback_tweener.hpp>

using namespace godot;
using namespace jenova::sdk;

namespace {
	Ref<AStarGrid2D> astar;
}

JENOVA_SCRIPT_BEGIN

void OnAwake(Caller* instance) {}
void OnDestroy(Caller* instance) {}
void OnReady(Caller* instance) {}

// 1. INICIALIZAR EL GPS
void ConfigurarAStar(Caller* instance, int grid_size, float cell_size) {
	astar.instantiate();
	astar->set_region(Rect2i(0, 0, grid_size, grid_size));
	astar->set_cell_size(Vector2(cell_size, cell_size));
	astar->set_default_compute_heuristic(AStarGrid2D::HEURISTIC_MANHATTAN);
	astar->set_diagonal_mode(AStarGrid2D::DIAGONAL_MODE_NEVER);
	astar->update();
}

// 2. BLOQUEAR/DESBLOQUEAR RUTAS
void SetCasillaSolida(Caller* instance, int x, int y, bool es_solido) {
	if (astar.is_valid()) {
		astar->set_point_solid(Vector2i(x, y), es_solido);
	}
}

// 5. RECIBIR LA LISTA MASIVA DEL DUNGEON MANAGER
void ActualizarAStarLibres(Caller* instance, godot::Array casillas_libres) {
	if (!astar.is_valid()) return;

	// 1. Bloqueamos TODO el mapa rapidísimo (C++ puro, sin lag)
	Vector2i grid_size = astar->get_region().size;
	for (int x = 0; x < grid_size.x; ++x) {
		for (int y = 0; y < grid_size.y; ++y) {
			astar->set_point_solid(Vector2i(x, y), true);
		}
	}

	// 2. Desbloqueamos SOLO las que el DungeonManager nos ha dicho
	for (int i = 0; i < casillas_libres.size(); i++) {
		Vector2i pos = casillas_libres[i];
		astar->set_point_solid(pos, false);
	}
}

// 3. DEVOLVER LA RUTA PARA LA LÍNEA AMARILLA (AHORA RECIBE INTs)
godot::Array CalcularRuta(Caller* instance, int orig_x, int orig_y, int dest_x, int dest_y) {
	Vector2i origen(orig_x, orig_y);
	Vector2i destino(dest_x, dest_y);
	
	if (astar.is_valid()) {
		bool era_solido = astar->is_point_solid(destino);
		if (era_solido) astar->set_point_solid(destino, false);

		TypedArray<Vector2i> path = astar->get_id_path(origen, destino);

		if (era_solido) astar->set_point_solid(destino, true);
		return path;
	}
	return godot::Array();
}

// 4. MOVER FÍSICAMENTE AL PERSONAJE CON TWEENS
void MoverPersonaje(Caller* instance, int dest_x, int dest_y) {
	godot::Node* self_node = GetSelf<godot::Node>(instance);
	godot::Node* padre_dungeon = self_node->get_parent();
	if (!padre_dungeon) return;

	Node* pm = self_node->get_node_or_null("/root/Main/PartyManager");
	if (!pm) return;

	// PROTECCIÓN VARIANT: Cast 100% seguro del personaje activo
	godot::Variant char_var = pm->call("GetActiveCharacter");
	Node* act_char = godot::Object::cast_to<Node>((godot::Object*)char_var);
	if (!act_char) {
		godot::UtilityFunctions::print("[PathManager] Error: No se encontró al personaje activo.");
		return;
	}

	Vector2i current_pos = (Vector2i)pm->call("GetActiveGridPos");
	
	// Calculamos la ruta
	godot::Array path = CalcularRuta(instance, current_pos.x, current_pos.y, dest_x, dest_y);

	// =======================================================
	// ✂️ EL CORTADOR DE RUTAS AUTÓNOMO (La magia del GPS)
	// =======================================================
	// Si la casilla destino estaba marcada como "sólida" en el GPS 
	// (es decir, es una puerta cerrada o un cofre), recortamos el último paso solos.
	if (astar.is_valid() && astar->is_point_solid(Vector2i(dest_x, dest_y))) {
		if (path.size() > 1) {
			path.pop_back(); 
		}
	}
	// =======================================================

	int pa_disponibles = act_char->has_meta("pa_actual") ? (int)act_char->get_meta("pa_actual") : 0;
	
	if (pa_disponibles <= 0 || path.size() <= 1) {
		godot::UtilityFunctions::print("[PathManager] Movimiento abortado. PA: ", pa_disponibles, " | Pasos de ruta: ", path.size());
		return;
	}

	// Recortar ruta por PA
	if ((path.size() - 1) > pa_disponibles) {
		path.resize(pa_disponibles + 1);
	}

	Vector2i actual_end = path[path.size() - 1];

	padre_dungeon->call("ApagarLucesRuta");
	padre_dungeon->set_meta("is_moving", true);

	String node_name = act_char->get_name();
	Node2D* token = godot::Object::cast_to<Node2D>(padre_dungeon->get_node_or_null(NodePath(node_name)));
	TileMapLayer* board = godot::Object::cast_to<TileMapLayer>(padre_dungeon->get_node_or_null("Tablero"));

	if (token && board) {
		int dex = pa_disponibles;
		Ref<Tween> tween = self_node->create_tween();
		padre_dungeon->set_meta("current_move_tween", tween);
		
		float time_per_step = 0.4;

		for (int i = 1; i < path.size(); i++) {
			Vector2i step = path[i];
			Vector2 pixel_target = board->map_to_local(step);
			
			tween->tween_property(token, "position", pixel_target, time_per_step)->set_trans(Tween::TRANS_LINEAR);
			
			float tilt = (i % 2 == 0) ? 0.2 : -0.2;
			tween->parallel()->tween_property(token, "rotation", tilt, time_per_step)->set_trans(Tween::TRANS_SINE)->set_ease(Tween::EASE_IN_OUT);
			
			Callable step_fn = Callable(padre_dungeon, StringName("PasoDado")).bind(step.x, step.y, dex);
			tween->tween_callback(step_fn);
		}

		tween->tween_property(token, "rotation", 0.0, 0.15);

		int steps_taken = path.size() - 1;
		Callable last_move = Callable(padre_dungeon, StringName("AtLast")).bind(actual_end.x, actual_end.y, steps_taken);
		tween->connect("finished", last_move);
	} else {
		padre_dungeon->set_meta("is_moving", false);
	}
}

JENOVA_SCRIPT_END
