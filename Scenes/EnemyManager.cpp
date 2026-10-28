/* Jenova C++ Node Base Script - EnemyManager */

// El turno de los enemigos. Cuando los héroes agotan el día, cada enemigo despierto
// (in_combat: ha visto a un héroe) actúa por orden según su perfil de IA (EnemigoData):
//   TANQUE            va a por el héroe más cercano y se planta delante
//   CUERPO_A_CUERPO   va a por el héroe más débil que pueda alcanzar
//   LANZADOR          busca una casilla desde la que atacar a distancia, lejos de los héroes
//   COBARDE           ataca mientras está sano y huye con poca vida
// Se mueve hasta 'movimiento' casillas (BFS, IA/Enemigos.hpp) y ataca si su arma llega
// (Combate/Reglas.hpp). Al acabar todos, el PartyManager empieza el día siguiente.
// La cámara echa un vistazo a cada enemigo visible mientras actúa.

#include <Godot/godot.hpp>
#include <Godot/classes/node.hpp>
#include <Godot/classes/node3d.hpp>
#include <Godot/classes/camera3d.hpp>
#include <Godot/classes/tween.hpp>
#include <Godot/classes/property_tweener.hpp>
#include <Godot/classes/callback_tweener.hpp>
#include <Godot/classes/interval_tweener.hpp>
#include <Godot/variant/utility_functions.hpp>
#include "Scripts/CPP/Combate/Reglas.hpp"
#include "Scripts/CPP/Combate/Animaciones.hpp"
#include "Scripts/CPP/IA/Enemigos.hpp"
#include <vector>
#include <algorithm>

using namespace godot;
using namespace jenova::sdk;
using namespace animacion;

namespace {
	Node* self = nullptr;
	std::vector<uint64_t> cola;
	size_t siguiente = 0;
	// Acción en curso
	uint64_t actor = 0;
	uint64_t objetivo = 0;
	bool atacar_al_llegar = false;

	const float T_PASO = 0.22f;
	const float T_EMPUJON = 0.11f;

	Node* Dungeon() { return self ? self->get_parent() : nullptr; }
	Node* PM() { return self ? self->get_node_or_null("/root/Main/PartyManager") : nullptr; }
	Node* Feel() { return self ? self->get_node_or_null("/root/Feel") : nullptr; }
	Camera3D* Camara() { Node* d = Dungeon(); return d ? Object::cast_to<Camera3D>(d->get_node_or_null("Camera3D")) : nullptr; }
	bool MetaBool(Node* n, const char* k) { return n && n->has_meta(k) && (bool)n->get_meta(k); }
	Vector3 GridToWorld(Vector2i c) { return Vector3(c.x + 0.5f, 0.0f, c.y + 0.5f); }

	Node3D* Enemigo(uint64_t id) { return Object::cast_to<Node3D>(UtilityFunctions::instance_from_id(id)); }
	Node* Heroe(uint64_t id) { return Object::cast_to<Node>(UtilityFunctions::instance_from_id(id)); }
	bool Vivo(Node3D* e) { return e && !String(e->get_name()).ends_with("_muerto") && (int)e->get_meta("hp_actual", 1) > 0; }

	std::vector<Node*> HeroesEnMapa() {
		std::vector<Node*> out;
		Node* pm = PM();
		if (!pm) return out;
		TypedArray<Node> hijos = pm->get_children();
		for (int i = 0; i < hijos.size(); i++) {
			Node* h = Object::cast_to<Node>(hijos[i]);
			if (!h || h->get_name() == StringName("DatosEquipo")) continue;
			if (MetaBool(h, "esta_muerto") || MetaBool(h, "ha_escapado") || !h->has_meta("grid_pos")) continue;
			out.push_back(h);
		}
		return out;
	}

	std::vector<Node3D*> TodosLosEnemigos() {
		std::vector<Node3D*> out;
		Node* d = Dungeon();
		if (!d) return out;
		Array lista = d->get_meta("lista_enemigos", Array());
		for (int i = 0; i < lista.size(); i++) {
			Vector2i p = lista[i];
			Node3D* e = Object::cast_to<Node3D>(d->get_node_or_null(NodePath("Enemy_" + String::num_int64(p.x) + "_" + String::num_int64(p.y))));
			if (Vivo(e)) out.push_back(e);
		}
		return out;
	}

	bool LineaDeVision(Vector2i a, Vector2i b) { return (bool)Dungeon()->call("HayLineaDeVision", a.x, a.y, b.x, b.y); }
	int Manhattan(Vector2i a, Vector2i b) { return std::abs(a.x - b.x) + std::abs(a.y - b.y); }

	bool PuedeAtacarDesde(Node* e, Vector2i desde, Node* h) {
		Vector2i hp = h->get_meta("grid_pos");
		return reglas::EnAlcance(e, desde, hp) && (!reglas::RequiereVision(e) || LineaDeVision(desde, hp));
	}

	int DistanciaAlMasCercano(Vector2i c, const std::vector<Node*>& heroes) {
		int m = enemigos::INF;
		for (Node* h : heroes) m = std::min(m, Manhattan(c, h->get_meta("grid_pos")));
		return m;
	}

	// Los que ven a algún héroe (radio de detección + línea de visión) se despiertan
	void DespertarQuienVea() {
		std::vector<Node*> heroes = HeroesEnMapa();
		for (Node3D* e : TodosLosEnemigos()) {
			if (MetaBool(e, "in_combat")) continue;
			Vector2i p = e->get_meta("grid_pos", Vector2i(-99, -99));
			int radio = e->get_meta("radio_deteccion", 6);
			for (Node* h : heroes) {
				Vector2i hp = h->get_meta("grid_pos");
				if (Manhattan(p, hp) <= radio && LineaDeVision(p, hp)) {
					e->set_meta("in_combat", true);
					break;
				}
			}
		}
	}

	std::vector<Node3D*> Despiertos() {
		std::vector<Node3D*> out;
		for (Node3D* e : TodosLosEnemigos())
			if (MetaBool(e, "in_combat")) out.push_back(e);
		return out;
	}

	void Mirar(Node3D* e) {
		Camera3D* cam = Camara();
		if (cam && e && e->is_visible()) cam->set_meta("vistazo", e->get_position());
	}

	void Continuar(float espera) {
		Ref<Tween> tw = self->create_tween();
		tw->tween_interval(std::max(0.01f, espera));
		tw->tween_callback(Callable(self, StringName("SiguienteEnemigo")));
	}

	// ==========================================
	// DECISIÓN
	// ==========================================

	void Actuar(Node3D* e) {
		actor = e->get_instance_id();
		objetivo = 0;
		atacar_al_llegar = false;
		Vector2i pos = e->get_meta("grid_pos");
		std::vector<Node*> heroes = HeroesEnMapa();
		if (heroes.empty()) { Continuar(0.0f); return; }

		PackedByteArray transitable = Dungeon()->call("ObtenerTransitables");
		transitable[enemigos::Indice(pos)] = 1; // su propia casilla
		enemigos::Campo desde = enemigos::Bfs(transitable, pos);
		int mov = e->get_meta("movimiento", 3);
		int perfil = e->get_meta("perfil", (int)enemigos::P_CUERPO_A_CUERPO);
		int hp = e->get_meta("hp_actual", 1), hp_max = e->get_meta("hp_max", 1);
		int huir_pct = e->get_meta("huir_con_vida", 0);
		bool huir = huir_pct > 0 && hp * 100 < hp_max * huir_pct;

		// Objetivo: el más cercano (por camino real) o, para el cuerpo a cuerpo, el más débil
		Node* obj = nullptr;
		enemigos::Campo campo_obj;
		int mejor = enemigos::INF;
		for (Node* h : heroes) {
			enemigos::Campo c = enemigos::Bfs(transitable, h->get_meta("grid_pos"));
			int d = c.en(pos);
			if (d >= enemigos::INF) d = Manhattan(pos, h->get_meta("grid_pos")) + 100;
			int puntos = perfil == enemigos::P_CUERPO_A_CUERPO ? (int)h->get_meta("hp_actual", 0) * 2 + d * 3 : d;
			if (puntos < mejor) { mejor = puntos; obj = h; campo_obj = c; }
		}
		if (!obj) { Continuar(0.0f); return; }
		objetivo = obj->get_instance_id();

		Vector2i destino = pos;
		bool decidido = false;
		auto pasos = [&](int i) { return desde.dist[i]; };

		if (huir) {
			// La casilla alcanzable más lejos del héroe más cercano
			int mejor_seg = -1, mejor_pasos = 0;
			for (int i = 0; i < (int)desde.dist.size(); i++) {
				if (pasos(i) > mov) continue;
				int seg = DistanciaAlMasCercano(enemigos::Celda(i), heroes);
				if (seg > mejor_seg || (seg == mejor_seg && pasos(i) < mejor_pasos)) { mejor_seg = seg; mejor_pasos = pasos(i); destino = enemigos::Celda(i); }
			}
			decidido = true;
		} else if (perfil == enemigos::P_LANZADOR) {
			// Desde dónde puede disparar, lo más lejos posible de los héroes
			int mejor_puntos = -enemigos::INF;
			for (int i = 0; i < (int)desde.dist.size(); i++) {
				if (pasos(i) > mov) continue;
				Vector2i c = enemigos::Celda(i);
				if (!PuedeAtacarDesde(e, c, obj)) continue;
				int puntos = DistanciaAlMasCercano(c, heroes) * 10 - pasos(i);
				if (puntos > mejor_puntos) { mejor_puntos = puntos; destino = c; }
			}
			if (mejor_puntos > -enemigos::INF) { decidido = true; atacar_al_llegar = true; }
		}

		if (!decidido) {
			if (PuedeAtacarDesde(e, pos, obj)) {
				atacar_al_llegar = true; // ya llega: no se mueve
			} else {
				// Acercarse: la casilla alcanzable más cerca del objetivo (mejor si desde ella ya ataca)
				int mejor_d = enemigos::INF, mejor_pasos = 0;
				bool mejor_ataca = false;
				for (int i = 0; i < (int)desde.dist.size(); i++) {
					if (pasos(i) > mov) continue;
					Vector2i c = enemigos::Celda(i);
					int d = campo_obj.en(c);
					bool ataca = d <= 6 && PuedeAtacarDesde(e, c, obj);
					bool mejora = ataca != mejor_ataca ? ataca : (d < mejor_d || (d == mejor_d && pasos(i) < mejor_pasos));
					if (mejora) { mejor_d = d; mejor_pasos = pasos(i); mejor_ataca = ataca; destino = c; }
				}
				if (!mejor_ataca && mejor_d >= campo_obj.en(pos)) destino = pos; // no mejora: se queda
				atacar_al_llegar = mejor_ataca;
			}
		}

		// Movimiento
		std::vector<Vector2i> camino = enemigos::Camino(desde, destino);
		if (camino.empty()) {
			self->call("TrasMoverEnemigo");
			return;
		}
		bool se_veia = e->is_visible();
		Dungeon()->call("MoverEnemigo", pos.x, pos.y, destino.x, destino.y);
		if (!se_veia && !e->is_visible()) {
			// En la niebla no hace falta animarlo
			e->set_position(GridToWorld(destino));
			self->call("TrasMoverEnemigo");
			return;
		}
		e->set_visible(true);
		Mirar(e);
		Ref<Tween> tw = self->create_tween();
		for (const Vector2i& paso : camino)
			tw->tween_property(e, "position", GridToWorld(paso), T_PASO)->set_trans(Tween::TRANS_SINE)->set_ease(Tween::EASE_IN_OUT);
		tw->tween_callback(Callable(self, StringName("TrasMoverEnemigo")));
	}
}

JENOVA_SCRIPT_BEGIN

void OnAwake(Caller* instance) { self = GetSelf<Node>(instance); }
void OnReady(Caller* instance) {}
void OnDestroy(Caller* instance) {
	self = nullptr;
	cola.clear();
}

// ¿Hay alguien que vaya a actuar? (despierta antes a quien vea a un héroe)
Variant HayEnemigosQueActuan(Caller* instance) {
	if (!Dungeon() || HeroesEnMapa().empty()) return false;
	DespertarQuienVea();
	return !Despiertos().empty();
}

void IniciarTurnoEnemigos(Caller* instance) {
	Node* d = Dungeon();
	if (!d) return;
	DespertarQuienVea();
	std::vector<Node3D*> despiertos = Despiertos();
	std::vector<Node*> heroes = HeroesEnMapa();
	// Actúan primero los más cercanos a los héroes
	std::sort(despiertos.begin(), despiertos.end(), [&](Node3D* a, Node3D* b) {
		return DistanciaAlMasCercano(a->get_meta("grid_pos"), heroes) < DistanciaAlMasCercano(b->get_meta("grid_pos"), heroes);
	});
	cola.clear();
	for (Node3D* e : despiertos) cola.push_back(e->get_instance_id());
	siguiente = 0;

	UtilityFunctions::print("[EnemyManager] Turno enemigo: ", (int)cola.size(), " enemigos despiertos.");
	d->set_meta("turno_enemigo", true);
	d->set_meta("is_moving", true);
	d->call("ApagarLucesMapa");
	Continuar(0.9f); // deja ver el rótulo "TURNO ENEMIGO"
}

void SiguienteEnemigo(Caller* instance) {
	if (!self) return;
	while (siguiente < cola.size() && !HeroesEnMapa().empty()) {
		Node3D* e = Enemigo(cola[siguiente++]);
		if (!Vivo(e)) continue;
		Actuar(e);
		return;
	}

	// Fin del turno enemigo: vuelve la cámara y empieza un nuevo día
	cola.clear();
	Camera3D* cam = Camara();
	if (cam && cam->has_meta("vistazo")) cam->remove_meta("vistazo");
	Node* d = Dungeon();
	if (d) {
		d->set_meta("turno_enemigo", false);
		d->set_meta("is_moving", false);
		d->call("CalcularMapaDeLuces");
	}
	Node* pm = PM();
	if (pm) pm->call("IniciarNuevoDia");
}

void TrasMoverEnemigo(Caller* instance) {
	Node3D* e = Enemigo(actor);
	Node* h = Heroe(objetivo);
	Node* d = Dungeon();
	bool vivo_h = h && !MetaBool(h, "esta_muerto") && !MetaBool(h, "ha_escapado");
	if (!Vivo(e) || !d || !vivo_h || !atacar_al_llegar || !PuedeAtacarDesde(e, e->get_meta("grid_pos"), h)) {
		Continuar(e && e->is_visible() ? 0.2f : 0.0f);
		return;
	}

	// Ataque: empujón (pegados) o proyectil (a distancia)
	Node3D* ficha_h = Object::cast_to<Node3D>(d->get_node_or_null(NodePath(String(h->get_name()))));
	Mirar(e);
	float espera = T_EMPUJON;
	if (reglas::Distancia(e->get_meta("grid_pos"), h->get_meta("grid_pos")) <= 1) {
		Desplazar(e, Direccion(e, ficha_h), 0.32f, T_EMPUJON, 0.16f);
	} else if (ficha_h) {
		Ref<Resource> arma = inventario::Arma(e);
		bool magia = arma.is_valid() && (int)arma->get("animacion") == 4; // CONJURO
		espera = Proyectil(d, e->get_position(), ficha_h->get_position(), magia ? Color(0.5, 0.8, 1.0) : Color(0.95, 0.9, 0.75));
	}
	Ref<Tween> tw = self->create_tween();
	tw->tween_interval(espera);
	tw->tween_callback(Callable(self, StringName("ImpactoEnemigo")));
}

void ImpactoEnemigo(Caller* instance) {
	Node3D* e = Enemigo(actor);
	Node* h = Heroe(objetivo);
	Node* d = Dungeon();
	Node* pm = PM();
	if (!Vivo(e) || !h || !d || !pm) { Continuar(0.1f); return; }

	int dano = reglas::DanoBase(e, h);
	bool critico = (int)(UtilityFunctions::randi() % 100) < reglas::ProbCritico(e);
	if (critico) dano = reglas::DanoCritico(e, dano);

	Node3D* ficha_h = Object::cast_to<Node3D>(d->get_node_or_null(NodePath(String(h->get_name()))));
	if (ficha_h) {
		TextoFlotante(d, ficha_h->get_position(), String::num_int64(dano), critico ? Color(1, 0.6, 0.15) : Color(1, 0.3, 0.25), critico ? 60 : 48);
		Destello(ficha_h, Color(3.0, 0.6, 0.6));
		Desplazar(ficha_h, Direccion(e, ficha_h), 0.14f, 0.05f, 0.15f);
	}
	Node* feel = Feel();
	if (feel) { feel->call("sacudir", critico ? 0.4 : 0.22); feel->call("pausa_impacto", 0.04); }
	Node* audio = self->get_node_or_null("/root/AudioManager");
	if (audio) audio->call("play_sfx", "fail");
	UtilityFunctions::print("[EnemyManager] ", e->get_meta("nombre_display"), " golpea a ", h->get_name(), " por ", dano, critico ? " (CRITICO)" : "");

	inventario::AplicarEfectos(reglas::EfectosAlGolpear(e), h);
	pm->call("RecibirDano", dano, h);
	Continuar(0.45f);
}

JENOVA_SCRIPT_END
