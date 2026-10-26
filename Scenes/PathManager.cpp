/* Jenova C++ Node Base Script (Meteora) - PathManager */

// El "GPS": AStarGrid2D con los muros estáticos cargados UNA sola vez.
// Las entidades dinámicas (héroes, enemigos, cofres) se inyectan como muros
// temporales solo mientras se calcula una ruta, y se retiran después.

#include <Godot/godot.hpp>
#include <Godot/classes/node.hpp>
#include <Godot/classes/node3d.hpp>
#include <Godot/classes/sprite3d.hpp>
#include <Godot/classes/camera3d.hpp>
#include <Godot/classes/a_star_grid2d.hpp>
#include <Godot/classes/tween.hpp>
#include <Godot/variant/utility_functions.hpp>
#include <Godot/classes/property_tweener.hpp>
#include <Godot/classes/callback_tweener.hpp>
#include <vector>

using namespace godot;
using namespace jenova::sdk;

namespace {
	Ref<AStarGrid2D> astar;
	const float TIEMPO_PASO = 0.28f;
	const float ALTURA_SALTO = 0.16f;

	Vector3 GridToWorld(Vector2i c) { return Vector3(c.x + 0.5f, 0.0f, c.y + 0.5f); }

	bool EnRegion(Vector2i p) {
		return astar.is_valid() && astar->is_in_boundsv(p);
	}

	// Inyecta las entidades dinámicas como sólidos. Devuelve las que hemos marcado nosotros.
	std::vector<Vector2i> InyectarDinamicos(Node* dungeon, Vector2i origen) {
		std::vector<Vector2i> marcadas;
		if (!dungeon) return marcadas;
		PackedInt32Array obst = dungeon->call("ObtenerObstaculosDinamicos");
		for (int i = 0; i + 1 < obst.size(); i += 2) {
			Vector2i p(obst[i], obst[i + 1]);
			if (p == origen || !EnRegion(p) || astar->is_point_solid(p)) continue;
			astar->set_point_solid(p, true);
			marcadas.push_back(p);
		}
		return marcadas;
	}

	void RetirarDinamicos(const std::vector<Vector2i>& marcadas) {
		for (const Vector2i& p : marcadas) astar->set_point_solid(p, false);
	}

	Array RutaDirecta(Vector2i origen, Vector2i destino) {
		if (!EnRegion(origen) || !EnRegion(destino) || astar->is_point_solid(destino)) return Array();
		return astar->get_id_path(origen, destino);
	}

	// Ruta al destino; si es sólido o inalcanzable, "Modo Enjambre": la mejor casilla adyacente.
	Array RutaInteligente(Node* dungeon, Vector2i origen, Vector2i destino) {
		if (!astar.is_valid()) return Array();
		std::vector<Vector2i> marcadas = InyectarDinamicos(dungeon, origen);

		Array ruta = RutaDirecta(origen, destino);
		if (ruta.size() == 0 && origen != destino) {
			const Vector2i vecinos[4] = { Vector2i(0, -1), Vector2i(0, 1), Vector2i(-1, 0), Vector2i(1, 0) };
			for (const Vector2i& d : vecinos) {
				Vector2i junto = destino + d;
				if (junto == origen) { ruta = Array(); ruta.push_back(origen); break; }
				Array candidata = RutaDirecta(origen, junto);
				if (candidata.size() > 0 && (ruta.size() == 0 || candidata.size() < ruta.size())) ruta = candidata;
			}
		}

		RetirarDinamicos(marcadas);
		return ruta;
	}
}

JENOVA_SCRIPT_BEGIN

void OnAwake(Caller* instance) {}
void OnReady(Caller* instance) {}
void OnDestroy(Caller* instance) { astar.unref(); }

// 1. INICIALIZAR EL GPS
void ConfigurarAStar(Caller* instance, int grid_size, float cell_size) {
	astar.instantiate();
	astar->set_region(Rect2i(0, 0, grid_size, grid_size));
	astar->set_cell_size(Vector2(cell_size, cell_size));
	astar->set_default_compute_heuristic(AStarGrid2D::HEURISTIC_MANHATTAN);
	astar->set_default_estimate_heuristic(AStarGrid2D::HEURISTIC_MANHATTAN);
	astar->set_diagonal_mode(AStarGrid2D::DIAGONAL_MODE_NEVER);
	astar->update();
}

// 2. CARGA ESTÁTICA: muros y puertas cerradas, una sola vez por piso (pares x,y)
void CargarMurosEstaticos(Caller* instance, PackedInt32Array solidos) {
	if (!astar.is_valid()) return;
	astar->fill_solid_region(astar->get_region(), false);
	for (int i = 0; i + 1 < solidos.size(); i += 2) {
		astar->set_point_solid(Vector2i(solidos[i], solidos[i + 1]), true);
	}
	UtilityFunctions::print("[PathManager] Muros estáticos cargados: ", solidos.size() / 2);
}

// 3. CAMBIOS PUNTUALES (abrir una puerta...)
void SetCasillaSolida(Caller* instance, int x, int y, int es_solido) {
	if (EnRegion(Vector2i(x, y))) astar->set_point_solid(Vector2i(x, y), es_solido != 0);
}

// 4. RUTA PARA LA LÍNEA DE PREVISUALIZACIÓN (incluye Modo Enjambre)
Array CalcularRuta(Caller* instance, int orig_x, int orig_y, int dest_x, int dest_y) {
	Node* dungeon = GetSelf<Node>(instance)->get_parent();
	return RutaInteligente(dungeon, Vector2i(orig_x, orig_y), Vector2i(dest_x, dest_y));
}

// 5. MOVER FÍSICAMENTE AL PERSONAJE CON TWEENS
void MoverPersonaje(Caller* instance, int dest_x, int dest_y) {
	Node* self_node = GetSelf<Node>(instance);
	Node* dungeon = self_node->get_parent();
	if (!dungeon) return;

	Node* pm = self_node->get_node_or_null("/root/Main/PartyManager");
	if (!pm) return;

	Node* act_char = Object::cast_to<Node>(pm->call("GetActiveCharacter"));
	if (!act_char) {
		UtilityFunctions::print("[PathManager] Error: No se encontró al personaje activo.");
		return;
	}

	Vector2i origen = pm->call("GetActiveGridPos");
	Array path = RutaInteligente(dungeon, origen, Vector2i(dest_x, dest_y));

	bool debug = dungeon->has_meta("debug_mode") && (bool)dungeon->get_meta("debug_mode");
	int pa = act_char->has_meta("pa_actual") ? (int)act_char->get_meta("pa_actual") : 0;
	if (debug) pa = 999;

	if (pa <= 0 || path.size() <= 1) {
		UtilityFunctions::print("[PathManager] Movimiento abortado. PA: ", pa, " | Pasos de ruta: ", path.size());
		return;
	}

	// Recortar ruta por PA
	if (path.size() - 1 > pa) path.resize(pa + 1);

	Node3D* token = Object::cast_to<Node3D>(dungeon->get_node_or_null(NodePath(String(act_char->get_name()))));
	if (!token) return;
	Sprite3D* cuerpo = Object::cast_to<Sprite3D>(token->get_node_or_null("Cuerpo"));
	Camera3D* cam = Object::cast_to<Camera3D>(dungeon->get_node_or_null("Camera3D"));
	Vector3 derecha_cam = cam ? cam->get_global_transform().basis.get_column(0) : Vector3(1, 0, 0);

	dungeon->call("ApagarLucesRuta");
	dungeon->set_meta("is_moving", true);

	Ref<Tween> tween = self_node->create_tween();
	dungeon->set_meta("current_move_tween", tween);

	float y_base = cuerpo ? (float)cuerpo->get_position().y : 0.0f;
	Vector2i anterior = origen;
	for (int i = 1; i < path.size(); i++) {
		Vector2i paso = path[i];
		Vector3 destino = GridToWorld(paso);

		if (cuerpo) {
			// El sprite mira hacia donde camina (en pantalla)
			Vector3 dir((float)(paso.x - anterior.x), 0.0f, (float)(paso.y - anterior.y));
			bool mira_izq = dir.dot(derecha_cam) < -0.01f;
			bool mira_der = dir.dot(derecha_cam) > 0.01f;
			if (mira_izq || mira_der) tween->tween_callback(Callable(cuerpo, StringName("set_flip_h")).bind(mira_izq));
		}

		tween->tween_property(token, "position", destino, TIEMPO_PASO)->set_trans(Tween::TRANS_LINEAR);
		if (cuerpo) {
			tween->parallel()->tween_property(cuerpo, "position:y", y_base + ALTURA_SALTO, TIEMPO_PASO * 0.5f)->set_trans(Tween::TRANS_SINE)->set_ease(Tween::EASE_OUT);
			tween->parallel()->tween_property(cuerpo, "position:y", y_base, TIEMPO_PASO * 0.5f)->set_delay(TIEMPO_PASO * 0.5f)->set_trans(Tween::TRANS_SINE)->set_ease(Tween::EASE_IN);
		}

		tween->tween_callback(Callable(dungeon, StringName("PasoDado")).bind(paso.x, paso.y, i));
		anterior = paso;
	}

	Vector2i fin = path[path.size() - 1];
	tween->tween_callback(Callable(dungeon, StringName("AtLast")).bind(fin.x, fin.y, (int)path.size() - 1));
}

JENOVA_SCRIPT_END
