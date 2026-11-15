/* Jenova C++ Node Base Script (Meteora) - PathManager */

// The "GPS": AStarGrid2D with the static walls loaded ONLY once. Dynamic entities (heroes, enemies,
// chests) are injected as temporary walls only while a path is computed, and removed afterwards.

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
#include "Scripts/CPP/Mazmorra/Alturas.hpp"

using namespace godot;
using namespace jenova::sdk;

namespace {
	Ref<AStarGrid2D> astar;
	const float TIEMPO_PASO = 0.28f;
	const float TIEMPO_PASO_EXPLORANDO = 0.15f;
	const float ALTURA_SALTO = 0.16f;

	Vector3 GridToWorld(Vector2i c) { return Vector3(c.x + 0.5f, alturas::Y(c.x, c.y), c.y + 0.5f); }

	// Is the tile inside the AStar grid?
	bool EnRegion(Vector2i p) {
		return astar.is_valid() && astar->is_in_boundsv(p);
	}

	// Injects the dynamic entities as solids. Returns the ones we marked ourselves.
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

	// Is there an ally on that tile who won't step aside? (a path can't end there) While exploring,
	// the rest of the party follows the leader and makes room: only the ones standing still count
	// (Modo.gd). In combat, any ally.
	bool AliadoBloquea(Node* dungeon, Vector2i c) {
		bool explorando = dungeon->has_meta("modo_exploracion") && (bool)dungeon->get_meta("modo_exploracion");
		Node* modo = dungeon->get_node_or_null("/root/Modo");
		if (explorando && modo) return (bool)modo->call("aliado_quieto_en", c.x, c.y);
		return (bool)dungeon->call("HayAliadoEn", c.x, c.y);
	}

	// Undoes InyectarDinamicos: the marked tiles become walkable again
	void RetirarDinamicos(const std::vector<Vector2i>& marcadas) {
		for (const Vector2i& p : marcadas) astar->set_point_solid(p, false);
	}

	// Plain AStar path (empty if the destination is solid or out of the grid)
	Array RutaDirecta(Vector2i origen, Vector2i destino) {
		if (!EnRegion(origen) || !EnRegion(destino) || astar->is_point_solid(destino)) return Array();
		return astar->get_id_path(origen, destino);
	}

	// Path to the destination; if it is solid or unreachable, "Swarm Mode": the best adjacent tile.
	Array RutaInteligente(Node* dungeon, Vector2i origen, Vector2i destino) {
		if (!astar.is_valid()) return Array();
		std::vector<Vector2i> marcadas = InyectarDinamicos(dungeon, origen);

		Array ruta = RutaDirecta(origen, destino);
		if (ruta.size() == 0 && origen != destino) {
			const Vector2i vecinos[4] = { Vector2i(0, -1), Vector2i(0, 1), Vector2i(-1, 0), Vector2i(1, 0) };
			for (const Vector2i& d : vecinos) {
				Vector2i junto = destino + d;
				if (junto == origen) { ruta = Array(); ruta.push_back(origen); break; }
				if (EnRegion(junto) && AliadoBloquea(dungeon, junto)) continue; // Can't end there
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

// 1. INIT THE GPS
void ConfigurarAStar(Caller* instance, int grid_size, float cell_size) {
	astar.instantiate();
	astar->set_region(Rect2i(0, 0, grid_size, grid_size));
	astar->set_cell_size(Vector2(cell_size, cell_size));
	astar->set_default_compute_heuristic(AStarGrid2D::HEURISTIC_MANHATTAN);
	astar->set_default_estimate_heuristic(AStarGrid2D::HEURISTIC_MANHATTAN);
	astar->set_diagonal_mode(AStarGrid2D::DIAGONAL_MODE_NEVER);
	astar->update();
}

// 2. STATIC LOAD: walls and closed doors, only once per floor (x,y pairs)
void CargarMurosEstaticos(Caller* instance, PackedInt32Array solidos) {
	if (!astar.is_valid()) return;
	astar->fill_solid_region(astar->get_region(), false);
	for (int i = 0; i + 1 < solidos.size(); i += 2) {
		astar->set_point_solid(Vector2i(solidos[i], solidos[i + 1]), true);
	}
	UtilityFunctions::print(String::utf8("[PathManager] Muros est\xC3\xA1ticos cargados: "), solidos.size() / 2);
}

// 3. ONE-OFF CHANGES (opening a door...)
void SetCasillaSolida(Caller* instance, int x, int y, int es_solido) {
	if (EnRegion(Vector2i(x, y))) astar->set_point_solid(Vector2i(x, y), es_solido != 0);
}

// 4. PATH FOR THE PREVIEW LINE (includes Swarm Mode)
Array CalcularRuta(Caller* instance, int orig_x, int orig_y, int dest_x, int dest_y) {
	Node* dungeon = GetSelf<Node>(instance)->get_parent();
	return RutaInteligente(dungeon, Vector2i(orig_x, orig_y), Vector2i(dest_x, dest_y));
}

// 5. PHYSICALLY MOVE THE CHARACTER WITH TWEENS
void MoverPersonaje(Caller* instance, int dest_x, int dest_y) {
	Node* self_node = GetSelf<Node>(instance);
	Node* dungeon = self_node->get_parent();
	if (!dungeon) return;

	Node* pm = self_node->get_node_or_null("/root/Main/PartyManager");
	if (!pm) return;

	Node* act_char = Object::cast_to<Node>(pm->call("GetActiveCharacter"));
	if (!act_char) {
		UtilityFunctions::print(String::utf8("[PathManager] Error: No se encontr\xC3\xB3 al personaje activo."));
		return;
	}

	Vector2i origen = pm->call("GetActiveGridPos");
	Array path = RutaInteligente(dungeon, origen, Vector2i(dest_x, dest_y));

	bool debug = dungeon->has_meta("debug_mode") && (bool)dungeon->get_meta("debug_mode");
	bool explorando = dungeon->has_meta("modo_exploracion") && (bool)dungeon->get_meta("modo_exploracion");
	int pa = act_char->has_meta("pa_actual") ? (int)act_char->get_meta("pa_actual") : 0;
	if (debug || explorando) pa = 999;
	// Hidden (Sigilo.gd): every tile costs 2 PA
	else if (act_char->has_meta("estados") && Dictionary(act_char->get_meta("estados")).has("oculto")) pa /= 2;

	if (pa <= 0 || path.size() <= 1) {
		UtilityFunctions::print("[PathManager] Movimiento abortado. PA: ", pa, " | Pasos de ruta: ", path.size());
		Node* feel = self_node->get_node_or_null("/root/Feel");
		if (feel && pa <= 0) feel->call("sacudir_ficha", String(act_char->get_name()));
		return;
	}

	// Cut the path by PA
	if (path.size() - 1 > pa) path.resize(pa + 1);
	// You can walk through a teammate, but not end on their tile: step back to the last free one
	while (path.size() > 1) {
		Vector2i fin = path[path.size() - 1];
		if (!AliadoBloquea(dungeon, fin)) break;
		path.resize(path.size() - 1);
	}
	if (path.size() <= 1) return;

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
	// While exploring we walk faster (there's nothing to think about between steps)
	float paso_t = explorando ? TIEMPO_PASO_EXPLORANDO : TIEMPO_PASO;
	Vector2i anterior = origen;
	for (int i = 1; i < path.size(); i++) {
		Vector2i paso = path[i];
		Vector3 destino = GridToWorld(paso);

		if (cuerpo) {
			// The sprite faces where it walks (on screen)
			Vector3 dir((float)(paso.x - anterior.x), 0.0f, (float)(paso.y - anterior.y));
			bool mira_izq = dir.dot(derecha_cam) < -0.01f;
			bool mira_der = dir.dot(derecha_cam) > 0.01f;
			if (mira_izq || mira_der) tween->tween_callback(Callable(cuerpo, StringName("set_flip_h")).bind(mira_izq));
		}

		tween->tween_property(token, "position", destino, paso_t)->set_trans(Tween::TRANS_LINEAR);
		if (cuerpo) {
			tween->parallel()->tween_property(cuerpo, "position:y", y_base + ALTURA_SALTO, paso_t * 0.5f)->set_trans(Tween::TRANS_SINE)->set_ease(Tween::EASE_OUT);
			tween->parallel()->tween_property(cuerpo, "position:y", y_base, paso_t * 0.5f)->set_delay(paso_t * 0.5f)->set_trans(Tween::TRANS_SINE)->set_ease(Tween::EASE_IN);
		}

		tween->tween_callback(Callable(dungeon, StringName("PasoDado")).bind(paso.x, paso.y, i));
		anterior = paso;
	}

	Vector2i fin = path[path.size() - 1];
	tween->tween_callback(Callable(dungeon, StringName("AtLast")).bind(fin.x, fin.y, (int)path.size() - 1));
	// While exploring, the rest of the party follows whoever walks (Modo.gd)
	Node* modo = self_node->get_node_or_null("/root/Modo");
	if (modo && explorando) modo->call("lider_se_mueve", path, (double)paso_t);
}

JENOVA_SCRIPT_END
