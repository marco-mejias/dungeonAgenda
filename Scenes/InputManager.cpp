/* Jenova C++ Node Base Script (Meteora) - InputManager */

// El vigía: convierte el ratón en una casilla de la cuadrícula lanzando un rayo
// desde la Camera3D contra el plano del suelo (y = 0). No necesita físicas.

#include <Godot/godot.hpp>
#include <Godot/classes/node.hpp>
#include <Godot/classes/node3d.hpp>
#include <Godot/classes/camera3d.hpp>
#include <Godot/classes/control.hpp>
#include <Godot/classes/input.hpp>
#include <Godot/classes/viewport.hpp>
#include <Godot/variant/utility_functions.hpp>
#include <cmath>

using namespace godot;
using namespace jenova::sdk;

namespace {
	Vector2i last_hovered_cell = Vector2i(-1000, -1000);
	bool debug_key_pressed = false;

	// Posición del ratón en píxeles del SubViewport (el contenedor reduce la resolución con stretch_shrink)
	bool RatonEnViewport(Camera3D* cam, Vector2& out) {
		Node* sub = cam->get_viewport();
		Control* contenedor = sub ? Object::cast_to<Control>(sub->get_parent()) : nullptr;
		if (!contenedor) return false;
		Vector2 local = contenedor->get_local_mouse_position();
		Vector2 size = contenedor->get_size();
		if (local.x < 0 || local.y < 0 || local.x > size.x || local.y > size.y) return false;
		int shrink = (int)contenedor->get("stretch_shrink");
		out = local / (float)std::max(1, shrink);
		return true;
	}

	Vector2i CasillaBajoRaton(Node* dungeon) {
		Camera3D* cam = Object::cast_to<Camera3D>(dungeon->get_node_or_null("Camera3D"));
		Vector2 mp;
		if (!cam || !RatonEnViewport(cam, mp)) return Vector2i(-1, -1);

		Vector3 origen = cam->project_ray_origin(mp);
		Vector3 dir = cam->project_ray_normal(mp);
		if (std::abs(dir.y) < 0.0001f) return Vector2i(-1, -1);
		float t = -origen.y / dir.y;
		if (t < 0) return Vector2i(-1, -1);
		Vector3 hit = origen + dir * t;
		return Vector2i((int)std::floor(hit.x), (int)std::floor(hit.z));
	}
}

JENOVA_SCRIPT_BEGIN

void OnAwake(Caller* instance) {}
void OnDestroy(Caller* instance) {}
void OnReady(Caller* instance) {}

// === 1. EL ONPROCESS (EL VIGÍA) ===
void OnProcess(Caller* instance, double _delta) {
	Node* self_node = GetSelf<Node>(instance);
	Node* dungeon = self_node->get_parent();
	if (!dungeon) return;

	Vector2i hover = CasillaBajoRaton(dungeon);
	if (hover != last_hovered_cell) {
		last_hovered_cell = hover;
		dungeon->call("ActualizarHover", hover.x, hover.y);
	}

	// === F9: MODO DEBUG ===
	Input* input = Input::get_singleton();
	bool f9 = input->is_physical_key_pressed(KEY_F9);
	if (f9 && !debug_key_pressed) dungeon->call("ToggleModoDebug");
	debug_key_pressed = f9;
}

// Fuerza a recalcular el hover (p.ej. después de moverse)
void RefrescarHover(Caller* instance) {
	last_hovered_cell = Vector2i(-1000, -1000);
}

// === 2. GESTIÓN DEL CLIC IZQUIERDO (lo llama main.gd solo si el ratón no está sobre la UI) ===
void HandleMapClick(Caller* instance, double mouse_x, double mouse_y) {
	Node* self_node = GetSelf<Node>(instance);
	Node* dungeon = self_node->get_parent();
	if (!dungeon) return;
	if (dungeon->has_meta("bloqueo_evento") && (bool)dungeon->get_meta("bloqueo_evento")) return;

	Vector2i celda = CasillaBajoRaton(dungeon);
	last_hovered_cell = Vector2i(-1000, -1000);

	bool debug = dungeon->has_meta("debug_mode") && (bool)dungeon->get_meta("debug_mode");
	if (debug) {
		dungeon->call("TeletransporteDebug", celda.x, celda.y);
		return;
	}

	// El InputManager no tiene map_data: el DungeonManager decide si es caminar,
	// interactuar, abrir el menú táctico o cerrar un menú abierto.
	dungeon->call("ProcesarClicEnCasilla", celda.x, celda.y);
}

JENOVA_SCRIPT_END
