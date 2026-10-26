/* Jenova C++ Node Base Script (Meteora) - CameraManager (3D iso) */

// Cámara "pseudo-isométrica": perspectiva con FOV muy cerrado y lejana, para
// conservar el temblor de vértices y la deformación de texturas PS1.
// Q/E rotan 90°, rueda = zoom (distancia), clic derecho = arrastrar.

#include <Godot/godot.hpp>
#include <Godot/classes/node.hpp>
#include <Godot/classes/node3d.hpp>
#include <Godot/classes/camera3d.hpp>
#include <Godot/classes/control.hpp>
#include <Godot/classes/viewport.hpp>
#include <Godot/classes/input.hpp>
#include <Godot/variant/utility_functions.hpp>
#include <cmath>

using namespace godot;
using namespace jenova::sdk;

namespace {
	const float PITCH_DEG = 40.0f;
	const float FOV = 22.0f;
	const float DIST_MIN = 9.0f;
	const float DIST_MAX = 34.0f;
	const float DIST_REF = 18.0f; // distancia de referencia para escalar la UI anclada
	const float GRID = 35.0f;

	Vector3 pivote;
	float yaw = 45.0f, yaw_obj = 45.0f;
	float dist = 18.0f, dist_obj = 18.0f;
	bool inicializada = false;
	bool q_prev = false, e_prev = false;
	bool auto_centrar = true;
	bool arrastrando = false;
	Vector2 raton_prev;
	// OJO: nada de godot::String globales: se construirían al cargar la DLL, antes que GDExtension
	uint64_t last_char_id = 0;
	Vector3 ir_a;
	bool yendo = false;

	Vector3 Direccion() {
		float p = PITCH_DEG * (float)Math_PI / 180.0f;
		float y = yaw * (float)Math_PI / 180.0f;
		return Vector3(std::sin(y) * std::cos(p), std::sin(p), std::cos(y) * std::cos(p));
	}

	Node3D* TokenActivo(Camera3D* cam, Node* dungeon, String& nombre) {
		Node* pm = cam->get_node_or_null("/root/Main/PartyManager");
		if (!pm) return nullptr;
		Node* act = Object::cast_to<Node>(pm->call("GetActiveCharacter"));
		if (!act) return nullptr;
		nombre = act->get_name();
		return Object::cast_to<Node3D>(dungeon->get_node_or_null(NodePath(nombre)));
	}

	Vector2 RatonEnContenedor(Camera3D* cam) {
		Node* sub = cam->get_viewport();
		Control* contenedor = sub ? Object::cast_to<Control>(sub->get_parent()) : nullptr;
		return contenedor ? contenedor->get_local_mouse_position() : Vector2();
	}
}

JENOVA_SCRIPT_BEGIN

// Lo puede llamar el DungeonManager al crear un piso (compatibilidad con la versión 2D)
void ConfigurarLimitesMapa(Caller* instance, int grid_size) {}

void OnAwake(Caller* instance) {}
void OnDestroy(Caller* instance) { inicializada = false; }

void OnReady(Caller* instance) {
	Camera3D* cam = GetSelf<Camera3D>(instance);
	if (!cam) return;
	cam->set_fov(FOV);
	cam->set_near(1.0f);
	cam->set_far(120.0f);
	cam->set_current(true);
}

void OnProcess(Caller* instance, double delta) {
	Camera3D* cam = GetSelf<Camera3D>(instance);
	Input* input = Input::get_singleton();
	if (!cam || !input) return;
	Node* dungeon = cam->get_parent();
	if (!dungeon) return;

	String nombre;
	Node3D* token = TokenActivo(cam, dungeon, nombre);

	if (!inicializada) {
		pivote = token ? token->get_position() : Vector3(GRID * 0.5f, 0, GRID * 0.5f);
		inicializada = true;
	}

	// === ROTACIÓN Q / E (pasos de 90°) ===
	bool q = input->is_physical_key_pressed(KEY_Q);
	bool e = input->is_physical_key_pressed(KEY_E);
	if (q && !q_prev) yaw_obj -= 90.0f;
	if (e && !e_prev) yaw_obj += 90.0f;
	q_prev = q;
	e_prev = e;

	// === ZOOM (rueda) ===
	if (input->is_action_just_pressed("zoom_in")) dist_obj = UtilityFunctions::clampf(dist_obj - 2.0f, DIST_MIN, DIST_MAX);
	if (input->is_action_just_pressed("zoom_out")) dist_obj = UtilityFunctions::clampf(dist_obj + 2.0f, DIST_MIN, DIST_MAX);

	float k = (float)UtilityFunctions::minf(1.0, 10.0 * delta);
	yaw += (yaw_obj - yaw) * k;
	dist += (dist_obj - dist) * k;

	// === ARRASTRE CON CLIC DERECHO (sobre el plano del suelo) ===
	Vector2 raton = RatonEnContenedor(cam);
	bool derecho = input->is_mouse_button_pressed(MOUSE_BUTTON_RIGHT);
	if (derecho && arrastrando) {
		Vector2 d = raton - raton_prev;
		Basis b = cam->get_global_transform().basis;
		Vector3 right = b.get_column(0); right.y = 0; right = right.normalized();
		Vector3 fwd = -b.get_column(2); fwd.y = 0; fwd = fwd.normalized();
		float escala = dist * 0.0025f;
		pivote -= right * d.x * escala;
		pivote += fwd * d.y * escala;
		auto_centrar = false;
	}
	arrastrando = derecho;
	raton_prev = raton;

	// === SEGUIMIENTO DEL HÉROE ACTIVO ===
	uint64_t char_id = token ? token->get_instance_id() : 0;
	if (char_id != 0 && char_id != last_char_id) {
		auto_centrar = true;
		last_char_id = char_id;
	}
	// El DungeonManager puede pedir recentrar (teletransporte, nuevo piso...)
	if (cam->has_meta("recentrar") && (bool)cam->get_meta("recentrar")) {
		auto_centrar = true;
		cam->set_meta("recentrar", false);
	}
	// El minimapa puede pedir ir a un punto concreto
	if (cam->has_meta("ir_a")) {
		ir_a = cam->get_meta("ir_a");
		yendo = true;
		auto_centrar = false;
		cam->remove_meta("ir_a");
	}
	if (yendo) {
		pivote = pivote.lerp(ir_a, (float)UtilityFunctions::minf(1.0, 6.0 * delta));
		if (pivote.distance_to(ir_a) < 0.05f || derecho) yendo = false;
	}
	bool moviendo = dungeon->has_meta("is_moving") && (bool)dungeon->get_meta("is_moving");
	if (token && !derecho && (moviendo || auto_centrar)) {
		Vector3 objetivo = token->get_position();
		pivote = pivote.lerp(objetivo, (float)UtilityFunctions::minf(1.0, 6.0 * delta));
		if (pivote.distance_to(objetivo) < 0.05f) auto_centrar = false;
	}

	pivote.x = UtilityFunctions::clampf(pivote.x, 0.0f, GRID);
	pivote.z = UtilityFunctions::clampf(pivote.z, 0.0f, GRID);
	pivote.y = 0.0f;

	cam->set_position(pivote + Direccion() * dist);
	cam->look_at(pivote, Vector3(0, 1, 0));
	// Desplazamos el encuadre para que el héroe quede a la derecha de las fichas del HUD
	cam->set_h_offset(-dist * 0.14f);

	// Datos para la UI anclada (menú táctico) y el renderer
	cam->set_meta("pivote", pivote);
	cam->set_meta("zoom_factor", 2.0f * DIST_REF / dist);
	cam->set_meta("yaw", yaw);
}

JENOVA_SCRIPT_END
