/* Jenova C++ Node Base Script (Meteora) - CameraManager (3D iso) */

// Cámara "pseudo-isométrica": perspectiva con FOV muy cerrado y lejana, para
// conservar el temblor de vértices y la deformación de texturas PS1.
// Va ANCLADA al héroe activo (centrado, también al caminar y al rotar).
// Q/E rotan 90° alrededor del héroe, rueda = zoom (distancia).
// Clic derecho mantenido = mirar libremente; al soltar vuelve al héroe deslizándose.
// El minimapa puede pedir un "vistazo" (meta "vistazo") mientras se mantiene pulsado.
// El combate pide un "encuadre" (meta "encuadre": PackedVector3Array): la cámara se centra en esos
// puntos y se acerca todo lo que puede sin que se salgan (con un tope de zoom). Un solo punto = primer plano.

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
	const float GRID = 40.0f; // = mazmorra::GRID_SIZE

	Vector3 pivote;
	float yaw = 45.0f, yaw_obj = 45.0f;
	float dist = 18.0f, dist_obj = 18.0f;
	bool inicializada = false;
	bool q_prev = false, e_prev = false;
	bool arrastrando = false;
	bool volviendo = false; // tras soltar el arrastre/vistazo: regreso suave al héroe
	Vector2 raton_prev;
	const float VEL_SEGUIR = 14.0f;  // casi pegada al héroe mientras camina
	const float VEL_VOLVER = 5.0f;   // regreso suave al soltar
	// Arrastre con clic derecho "pesado": el ratón mueve un punto objetivo (con menos recorrido por
	// píxel que antes) y la cámara lo persigue con retraso, como si tuviera inercia
	const float ESCALA_ARRASTRE = 0.0016f; // antes 0.0025: la mazmorra se movía demasiado por cada píxel
	const float VEL_ARRASTRE = 6.0f;       // cuánto tarda la cámara en alcanzar al ratón
	Vector3 objetivo_arrastre;

	// Encuadre de combate
	const float DIST_COMBATE_MIN = 7.5f; // tope de zoom al encuadrar (más cerca que la rueda)
	const float MARGEN_ENCUADRE = 1.45f; // aire alrededor de los personajes
	const float ALTO_FICHA = 1.9f;       // cabezas + dado e indicadores encima
	const float VEL_ENCUADRE = 6.0f;
	float peso_enc = 0.0f;   // 0 = distancia del jugador, 1 = distancia del encuadre
	float dist_enc = 18.0f;

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

	// === CLIC DERECHO MANTENIDO: mirar libremente (arrastre sobre el plano del suelo) ===
	Vector2 raton = RatonEnContenedor(cam);
	bool derecho = input->is_mouse_button_pressed(MOUSE_BUTTON_RIGHT);
	if (derecho && !arrastrando) objetivo_arrastre = pivote; // empieza el arrastre
	if (derecho && arrastrando) {
		Vector2 d = raton - raton_prev;
		Basis b = cam->get_global_transform().basis;
		Vector3 right = b.get_column(0); right.y = 0; right = right.normalized();
		Vector3 fwd = -b.get_column(2); fwd.y = 0; fwd = fwd.normalized();
		float escala = dist * ESCALA_ARRASTRE;
		objetivo_arrastre -= right * d.x * escala;
		objetivo_arrastre += fwd * d.y * escala;
		objetivo_arrastre.x = UtilityFunctions::clampf(objetivo_arrastre.x, 0.0f, GRID);
		objetivo_arrastre.z = UtilityFunctions::clampf(objetivo_arrastre.z, 0.0f, GRID);
		pivote = pivote.lerp(objetivo_arrastre, (float)UtilityFunctions::minf(1.0, VEL_ARRASTRE * delta));
	}
	if (!derecho && arrastrando) volviendo = true; // acabamos de soltar
	arrastrando = derecho;
	raton_prev = raton;

	// === VISTAZO DESDE EL MINIMAPA (mientras se mantiene pulsado) ===
	bool vistazo = cam->has_meta("vistazo");
	if (vistazo && !derecho && !cam->has_meta("encuadre")) {
		Vector3 punto = cam->get_meta("vistazo");
		pivote = pivote.lerp(punto, (float)UtilityFunctions::minf(1.0, 6.0 * delta));
		volviendo = true;
	}

	// === ENCUADRE DE COMBATE (manda sobre el vistazo y sobre seguir al héroe) ===
	bool encuadre = cam->has_meta("encuadre") && !derecho;
	if (encuadre) {
		PackedVector3Array puntos = cam->get_meta("encuadre");
		if (puntos.size() > 0) {
			Vector3 centro;
			for (int i = 0; i < puntos.size(); i++) centro += puntos[i];
			centro /= (float)puntos.size();
			// Ejes de la pantalla con la rotación a la que vamos
			float p = PITCH_DEG * (float)Math_PI / 180.0f;
			float y = yaw_obj * (float)Math_PI / 180.0f;
			Vector3 haciaCam(std::sin(y) * std::cos(p), std::sin(p), std::cos(y) * std::cos(p));
			Vector3 frente = -haciaCam;
			Vector3 derecha = frente.cross(Vector3(0, 1, 0)).normalized();
			Vector3 arriba = derecha.cross(frente).normalized();
			Vector2 vp = cam->get_viewport() ? cam->get_viewport()->get_visible_rect().size : Vector2(16, 9);
			float aspecto = vp.y > 0 ? vp.x / vp.y : 1.78f;
			float tv = std::tan(FOV * 0.5f * (float)Math_PI / 180.0f);
			float necesaria = 0.0f;
			for (int i = 0; i < puntos.size(); i++) {
				for (int k = 0; k < 2; k++) {
					Vector3 o = puntos[i] + Vector3(0, k * ALTO_FICHA, 0) - centro;
					necesaria = UtilityFunctions::maxf(necesaria, std::abs(o.dot(derecha)) / (tv * aspecto));
					necesaria = UtilityFunctions::maxf(necesaria, std::abs(o.dot(arriba)) / tv);
				}
			}
			float objetivo_d = UtilityFunctions::clampf(necesaria * MARGEN_ENCUADRE, DIST_COMBATE_MIN, DIST_MAX);
			if (peso_enc < 0.01f) dist_enc = dist; // empieza desde donde está el jugador
			dist_enc += (objetivo_d - dist_enc) * (float)UtilityFunctions::minf(1.0, VEL_ENCUADRE * delta);
			pivote = pivote.lerp(centro, (float)UtilityFunctions::minf(1.0, VEL_ENCUADRE * delta));
			volviendo = true;
		}
	}
	peso_enc += ((encuadre ? 1.0f : 0.0f) - peso_enc) * (float)UtilityFunctions::minf(1.0, (encuadre ? 8.0 : 4.0) * delta);

	// === ANCLADA AL HÉROE ACTIVO ===
	if (cam->has_meta("recentrar")) cam->remove_meta("recentrar"); // ya siempre seguimos al héroe
	if (token && token->is_visible() && !derecho && !vistazo && !encuadre) {
		Vector3 objetivo = token->get_position();
		float vel = volviendo ? VEL_VOLVER : VEL_SEGUIR;
		pivote = pivote.lerp(objetivo, (float)UtilityFunctions::minf(1.0, vel * delta));
		if (pivote.distance_to(objetivo) < 0.05f) volviendo = false;
	}

	pivote.x = UtilityFunctions::clampf(pivote.x, 0.0f, GRID);
	pivote.z = UtilityFunctions::clampf(pivote.z, 0.0f, GRID);
	pivote.y = 0.0f;

	// Golpe de zoom (críticos) que se recupera solo; lo pone el autoload Feel
	float punch = cam->has_meta("punch") ? (float)cam->get_meta("punch") : 0.0f;
	punch = UtilityFunctions::maxf(0.0, punch - 4.0 * delta);
	cam->set_meta("punch", punch);
	float dist_base = dist + (dist_enc - dist) * peso_enc;
	float dist_visual = dist_base * (1.0f - 0.12f * punch);

	cam->set_position(pivote + Direccion() * dist_visual);
	cam->look_at(pivote, Vector3(0, 1, 0));
	// Héroe en el centro exacto: solo aplicamos la sacudida de pantalla que calcula Feel
	Vector2 sacudida = cam->has_meta("sacudida") ? (Vector2)cam->get_meta("sacudida") : Vector2();
	cam->set_h_offset(sacudida.x);
	cam->set_v_offset(sacudida.y);

	// Datos para la UI anclada (menú táctico) y el renderer
	cam->set_meta("pivote", pivote);
	cam->set_meta("zoom_factor", 2.0f * DIST_REF / dist_base);
	cam->set_meta("yaw", yaw);
}

JENOVA_SCRIPT_END
