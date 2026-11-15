#pragma once
// Enemigos: configurar un nodo de enemigo a partir de su EnemigoData (Data/Enemigos) y las
// utilidades de búsqueda que usa la IA (campos de distancia BFS sobre la cuadrícula).
//
// Metas de cada enemigo: id_enemigo, nombre_display, frame, hp_max/hp_actual, fuerza, defensa,
// magia, suerte, movimiento, perfil, radio_deteccion, huir_con_vida, tabla_botin, grid_pos,
// equipo {mano_principal: id del arma} e in_combat (= despierto, actúa en el turno enemigo).

#include <Godot/godot.hpp>
#include <Godot/classes/node3d.hpp>
#include <Godot/classes/sprite3d.hpp>
#include <Godot/classes/resource.hpp>
#include <vector>
#include <deque>
#include <algorithm>

namespace enemigos {

using namespace godot;

const int LADO = 40; // = mazmorra::GRID_SIZE

// Mismo orden que EnemigoData.Perfil
enum Perfil { P_TANQUE, P_CUERPO_A_CUERPO, P_LANZADOR, P_COBARDE };

inline void Configurar(Node3D* e, const Ref<Resource>& def, int piso, Vector2i celda) {
	e->set_meta("grid_pos", celda);
	e->set_meta("in_combat", false);
	Sprite3D* cuerpo = Object::cast_to<Sprite3D>(e->get_node_or_null("Cuerpo"));

	if (def.is_null()) {
		// Sin fichas en Data/Enemigos: el esqueleto de siempre
		e->set_meta("nombre_display", "Esqueleto");
		e->set_meta("frame", 11);
		e->set_meta("hp_max", 12 + 3 * piso);
		e->set_meta("hp_actual", 12 + 3 * piso);
		e->set_meta("fuerza", 5 + piso);
		e->set_meta("defensa", 2);
		e->set_meta("movimiento", 3);
		e->set_meta("perfil", (int)P_CUERPO_A_CUERPO);
		e->set_meta("radio_deteccion", 6);
		e->set_meta("huir_con_vida", 0);
		e->set_meta("tabla_botin", "enemigo");
		if (cuerpo) cuerpo->set_frame(11);
		return;
	}

	int p = std::max(0, piso - 1);
	int vida = (int)def->get("vida") + (int)def->get("vida_por_piso") * p;
	e->set_meta("id_enemigo", def->call("obtener_id"));
	e->set_meta("nombre_display", def->get("nombre"));
	e->set_meta("frame", def->get("frame"));
	e->set_meta("hp_max", vida);
	e->set_meta("hp_actual", vida);
	e->set_meta("fuerza", (int)def->get("fuerza") + (int)def->get("fuerza_por_piso") * p);
	e->set_meta("defensa", (int)def->get("defensa") + (int)def->get("defensa_por_piso") * p);
	e->set_meta("magia", def->get("magia"));
	e->set_meta("suerte", def->get("suerte"));
	e->set_meta("movimiento", def->get("movimiento"));
	e->set_meta("perfil", def->get("perfil"));
	e->set_meta("radio_deteccion", def->get("radio_deteccion"));
	e->set_meta("huir_con_vida", def->get("huir_con_vida"));
	e->set_meta("tabla_botin", def->call("id_botin"));
	String arma = def->call("id_arma");
	Dictionary equipo;
	if (!arma.is_empty()) equipo["mano_principal"] = arma;
	e->set_meta("equipo", equipo);
	if (cuerpo) {
		cuerpo->set_frame(def->get("frame"));
		cuerpo->set_modulate(def->get("tinte"));
	}
}

// ==========================================
// CAMPOS DE DISTANCIA (BFS 4 direcciones)
// ==========================================

inline bool Dentro(Vector2i p) { return p.x >= 0 && p.x < LADO && p.y >= 0 && p.y < LADO; }
inline int Indice(Vector2i p) { return p.x * LADO + p.y; }
inline Vector2i Celda(int i) { return Vector2i(i / LADO, i % LADO); }

const int INF = 1 << 28;

struct Campo {
	std::vector<int> dist;
	std::vector<int> padre;
	int en(Vector2i p) const { return Dentro(p) ? dist[Indice(p)] : INF; }
};

// Distancias desde 'inicio' pisando solo casillas transitables (el inicio puede estar ocupado:
// es el propio enemigo o el héroe del que medimos)
inline Campo Bfs(const PackedByteArray& transitable, Vector2i inicio) {
	Campo c;
	c.dist.assign(LADO * LADO, INF);
	c.padre.assign(LADO * LADO, -1);
	if (!Dentro(inicio)) return c;
	std::deque<Vector2i> cola;
	c.dist[Indice(inicio)] = 0;
	cola.push_back(inicio);
	const Vector2i dirs[4] = { Vector2i(0, -1), Vector2i(0, 1), Vector2i(-1, 0), Vector2i(1, 0) };
	while (!cola.empty()) {
		Vector2i p = cola.front();
		cola.pop_front();
		for (const Vector2i& d : dirs) {
			Vector2i n = p + d;
			if (!Dentro(n) || transitable[Indice(n)] == 0 || c.dist[Indice(n)] != INF) continue;
			c.dist[Indice(n)] = c.dist[Indice(p)] + 1;
			c.padre[Indice(n)] = Indice(p);
			cola.push_back(n);
		}
	}
	return c;
}

// Camino (sin la casilla de salida) desde el inicio del campo hasta 'destino'
inline std::vector<Vector2i> Camino(const Campo& c, Vector2i destino) {
	std::vector<Vector2i> out;
	if (c.en(destino) >= INF) return out;
	for (int i = Indice(destino); i >= 0 && c.dist[i] > 0; i = c.padre[i]) out.push_back(Celda(i));
	std::reverse(out.begin(), out.end());
	return out;
}

// Distancia desde un héroe hasta cualquier casilla pegada a él (la del héroe no es transitable)
inline Campo CampoDesdeHeroe(const PackedByteArray& transitable, Vector2i heroe) { return Bfs(transitable, heroe); }

} // namespace enemigos
