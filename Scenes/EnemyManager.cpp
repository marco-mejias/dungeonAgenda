/* Jenova C++ Node Base Script - EnemyManager */

// El turno de los enemigos. Cuando los héroes agotan el día, cada enemigo despierto
// (in_combat: ha visto a un héroe) actúa por orden según su perfil de IA (EnemigoData):
//   TANQUE            va a por el héroe más cercano y se planta delante
//   CUERPO_A_CUERPO   va a por el héroe más débil que pueda alcanzar
//   LANZADOR          busca una casilla desde la que atacar a distancia, lejos de los héroes
//   COBARDE           ataca mientras está sano y huye con poca vida
// Se mueve hasta 'movimiento' casillas (BFS, IA/Enemigos.hpp) y ataca si su arma llega
// (Combate/Reglas.hpp). Al acabar todos, el PartyManager empieza el día siguiente.
// La cámara echa un vistazo a cada enemigo visible mientras actúa y, al atacar, encuadra al enemigo
// y a su objetivo; el d20 sale en el dado pequeño sobre el enemigo (DadoCombate).

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
#include "Scripts/CPP/Mazmorra/Alturas.hpp"

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
	reglas::Tirada tirada_pendiente; // la del ataque en curso (se tira antes de animar el golpe)
	Ref<Tween> tw_mov;               // el paseo del enemigo que actúa (se corta si cae o queda aturdido)
	Vector2i destino_mov;

	const float T_PASO = 0.22f;
	const float T_EMPUJON = 0.11f;

	Node* Dungeon() { return self ? self->get_parent() : nullptr; }
	Node* PM() { return self ? self->get_node_or_null("/root/Main/PartyManager") : nullptr; }
	Node* Feel() { return self ? self->get_node_or_null("/root/Feel") : nullptr; }
	Camera3D* Camara() { Node* d = Dungeon(); return d ? Object::cast_to<Camera3D>(d->get_node_or_null("Camera3D")) : nullptr; }
	bool MetaBool(Node* n, const char* k) { return n && n->has_meta(k) && (bool)n->get_meta(k); }
	Vector3 GridToWorld(Vector2i c) { return Vector3(c.x + 0.5f, alturas::Y(c.x, c.y), c.y + 0.5f); }

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
		// Con Sigilo.gd: conos de visión (hacia dónde mira cada uno)
		Node* sigilo = self ? self->get_node_or_null("/root/Sigilo") : nullptr;
		if (sigilo) { sigilo->call("despertar_quien_vea"); return; }
		std::vector<Node*> heroes = HeroesEnMapa();
		for (Node3D* e : TodosLosEnemigos()) {
			if (MetaBool(e, "in_combat")) continue;
			// Solo despierta si está a la vista de los héroes (su ficha se ve): visión simétrica
			if (!e->is_visible()) continue;
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

	void Encuadrar(Node3D* a, Node3D* b) {
		Camera3D* cam = Camara();
		if (!cam || !a || !b) return;
		PackedVector3Array puntos;
		puntos.push_back(a->get_position());
		puntos.push_back(b->get_position());
		cam->set_meta("encuadre", puntos);
	}

	void SoltarEncuadre() {
		Camera3D* cam = Camara();
		if (cam && cam->has_meta("encuadre")) cam->remove_meta("encuadre");
	}

	void Continuar(float espera) {
		Ref<Tween> tw = self->create_tween();
		tw->tween_interval(std::max(0.01f, espera));
		tw->tween_callback(Callable(self, StringName("SiguienteEnemigo")));
	}

	// ==========================================
	// DECISIÓN
	// ==========================================

	bool TieneEstado(Node* n, const char* estado) {
		if (!n || !n->has_meta("estados")) return false;
		Dictionary est = n->get_meta("estados");
		return est.has(estado);
	}

	void Actuar(Node3D* e) {
		actor = e->get_instance_id();
		objetivo = 0;
		atacar_al_llegar = false;
		Vector2i pos = e->get_meta("grid_pos");
		std::vector<Node*> heroes = HeroesEnMapa();
		if (heroes.empty()) { Continuar(0.0f); return; }
		// Aturdido: pierde el turno (Estados.gd lo quita al empezar el día)
		if (TieneEstado(e, "aturdido") || TieneEstado(e, "sorprendido")) {
			bool sorpresa = TieneEstado(e, "sorprendido");
			if (e->is_visible()) TextoFlotante(Dungeon(), e->get_position(), sorpresa ? String("Sorprendido") : String("Aturdido"), Color(1.0, 0.9, 0.35), 32);
			Continuar(e->is_visible() ? 0.5f : 0.0f);
			return;
		}

		PackedByteArray transitable = Dungeon()->call("ObtenerTransitables");
		transitable[enemigos::Indice(pos)] = 1; // su propia casilla
		enemigos::Campo desde = enemigos::Bfs(transitable, pos);
		int mov = e->get_meta("movimiento", 3);
		// Derribado: levantarse le cuesta la mitad del movimiento
		if (TieneEstado(e, "derribado")) {
			Node* est = self->get_node_or_null("/root/Estados");
			if (est) est->call("quitar", e, String("derribado"));
			mov = mov / 2;
			if (e->is_visible()) TextoFlotante(Dungeon(), e->get_position(), "Se levanta", Color(0.8, 0.75, 0.65), 28);
		}
		int perfil = e->get_meta("perfil", (int)enemigos::P_CUERPO_A_CUERPO);
		int hp = e->get_meta("hp_actual", 1), hp_max = e->get_meta("hp_max", 1);
		int huir_pct = e->get_meta("huir_con_vida", 0);
		bool huir = huir_pct > 0 && hp * 100 < hp_max * huir_pct;

		// Objetivo: el más cercano (por camino real) o, para el cuerpo a cuerpo, el más débil
		Node* obj = nullptr;
		enemigos::Campo campo_obj;
		int mejor = enemigos::INF;
		for (Node* h : heroes) {
			// Máscara de "No estoy aquí": la mitad de las veces ni lo ven (si hay a quien atacar)
			if (heroes.size() > 1 && inventario::Lleva(h, "mascara_no_estoy") && UtilityFunctions::randi() % 2 == 0) continue;
			enemigos::Campo c = enemigos::Bfs(transitable, h->get_meta("grid_pos"));
			int d = c.en(pos);
			if (d >= enemigos::INF) d = Manhattan(pos, h->get_meta("grid_pos")) + 100;
			int puntos = perfil == enemigos::P_CUERPO_A_CUERPO ? (int)h->get_meta("hp_actual", 0) * 2 + d * 3 : d;
			if (inventario::Lleva(h, "casco_cubo_basura")) puntos += 60; // el olor: prefieren a otro
			if (puntos < mejor) { mejor = puntos; obj = h; campo_obj = c; }
		}
		// Provocado (habilidad del Tanque): va a por quien le provocó, si sigue en el mapa
		if (e->has_meta("provocado_por")) {
			String quien = e->get_meta("provocado_por");
			for (Node* h : heroes) {
				if (String(h->get_name()) != quien) continue;
				obj = h;
				campo_obj = enemigos::Bfs(transitable, h->get_meta("grid_pos"));
				break;
			}
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
		tw_mov = tw;
		destino_mov = destino;
		Vector2i previa = pos;
		for (const Vector2i& paso : camino) {
			tw->tween_property(e, "position", GridToWorld(paso), T_PASO)->set_trans(Tween::TRANS_SINE)->set_ease(Tween::EASE_IN_OUT);
			// Cada paso: ataques de oportunidad de los héroes, sangrado, peligros (Tactica.gd)
			tw->tween_callback(Callable(self, StringName("PasoEnemigo")).bind(previa.x, previa.y, paso.x, paso.y));
			previa = paso;
		}
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
	// Antes que nadie, la gallina invocada lanza su huevo (Especiales.gd)
	float espera_gallina = 0.0f;
	Node* esp = self->get_node_or_null("/root/Especiales");
	if (esp) espera_gallina = (float)(double)esp->call("turno_gallina");
	Continuar(0.9f + espera_gallina); // deja ver el rótulo "TURNO ENEMIGO"
}

void SiguienteEnemigo(Caller* instance) {
	if (!self) return;
	SoltarEncuadre();
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

// Un paso del enemigo que actúa. Si cae o queda aturdido por el camino, se queda en esa casilla.
void PasoEnemigo(Caller* instance, int x0, int y0, int x1, int y1) {
	Node3D* e = Enemigo(actor);
	Node* tac = self ? self->get_node_or_null("/root/Tactica") : nullptr;
	if (!tac || !e) return;
	bool frenar = (bool)tac->call("al_mover", e, Vector2i(x0, y0), Vector2i(x1, y1));
	if (!frenar && Vivo(e)) return;
	if (tw_mov.is_valid()) tw_mov->kill();
	if (Vivo(e)) {
		// El mapa ya lo tenía en el destino: lo devolvemos a donde se ha quedado
		if (destino_mov != Vector2i(x1, y1)) Dungeon()->call("MoverEnemigo", destino_mov.x, destino_mov.y, x1, y1);
		e->set_position(GridToWorld(Vector2i(x1, y1)));
	}
	Continuar(0.35f);
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

	// Primero la tirada: la cámara encuadra a los dos y el dado sale sobre el enemigo
	Node3D* ficha_h = Object::cast_to<Node3D>(d->get_node_or_null(NodePath(String(h->get_name()))));
	tirada_pendiente = reglas::TiradaAtaque(e, h);
	Node* tac = self->get_node_or_null("/root/Tactica");
	if (tac) tac->call("tras_atacar", e);
	float espera_dado = 0.0f;
	Node* dado = self->get_node_or_null("/root/DadoCombate");
	if (e->is_visible() && ficha_h) {
		Encuadrar(e, ficha_h);
		if (dado) {
			const reglas::Tirada& t = tirada_pendiente;
			String sello = !t.acierta ? String("FALLO") : (t.critico ? String::utf8("\xC2\xA1" "CR\xC3\x8D" "TICO!") : String(""));
			Variant v = dado->call("tirar", t.d20, t.acierta ? 1 : 0, sello, String(""), e, t.ventaja);
			espera_dado = v.get_type() == Variant::FLOAT ? (float)(double)v : 1.0f;
		}
	}
	Ref<Tween> tw_dado = self->create_tween();
	tw_dado->tween_interval(std::max(0.01f, espera_dado));
	tw_dado->tween_callback(Callable(self, StringName("GolpeEnemigo")));
}

// Tras la tirada: empujón (pegados) o proyectil (a distancia) y el impacto
void GolpeEnemigo(Caller* instance) {
	Node3D* e = Enemigo(actor);
	Node* h = Heroe(objetivo);
	Node* d = Dungeon();
	if (!Vivo(e) || !d || !h) { Continuar(0.1f); return; }
	Node3D* ficha_h = Object::cast_to<Node3D>(d->get_node_or_null(NodePath(String(h->get_name()))));
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
	// Guardaespaldas: un aliado pegado con esa pasiva recibe el golpe
	Node* tac = self->get_node_or_null("/root/Tactica");
	if (tac) {
		Node* prot = Object::cast_to<Node>(tac->call("protector_de", h));
		if (prot) h = prot;
	}

	// Tirada de ataque (d20) del enemigo: la que ya enseñó el dado
	reglas::Tirada tirada = tirada_pendiente;
	int dano_previo = reglas::DanoBase(e, h); // antes de que se le pase la distracción
	e->set_meta("desobediente", true);
	if (e->has_meta("distraido")) e->remove_meta("distraido");
	if (e->has_meta("cegado")) e->set_meta("cegado", std::max(0, (int)e->get_meta("cegado") - 1));
	Node3D* ficha_h = Object::cast_to<Node3D>(d->get_node_or_null(NodePath(String(h->get_name()))));
	if (!tirada.acierta) { // el FALLO lo enseña el dado
		UtilityFunctions::print("[EnemyManager] ", e->get_meta("nombre_display"), " falla contra ", h->get_name(), " (d20 ", tirada.d20, " + ", tirada.bono, " vs CA ", tirada.ca, ")");
		Continuar(0.45f);
		return;
	}
	int dano = dano_previo;
	bool critico = tirada.critico;
	if (critico) dano = reglas::DanoCritico(e, dano);

	if (ficha_h) {
		// El daño lo enseñan los Indicadores (corazón sobre la cabeza del héroe)
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
	// Pasivas del héroe al recibir un golpe (represalia, piel de piedra...)
	Node* prog = self->get_node_or_null("/root/Progreso");
	if (prog && !MetaBool(h, "esta_muerto")) prog->call("disparar", String("al_recibir_golpe"), h, e, true);
	Continuar(0.45f);
}

JENOVA_SCRIPT_END
