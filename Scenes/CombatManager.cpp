/* Jenova C++ Node Base Script - CombatManager */

// Combate como secuencia animada:
//   empujón del atacante (o proyectil si el arma es a distancia) -> impacto (pausa, sacudida,
//   destello, retroceso, número, efectos del arma) -> muerte (estallido + botín a la ficha) o,
//   si estaban pegados, contraataque animado -> fin de la acción.
// Daño, coste, crítico y alcance salen del arma equipada (Combate/Reglas.hpp).
// Mientras dura, el tablero está bloqueado (is_moving). Los efectos de pantalla los hace el autoload Feel.

#include <Godot/godot.hpp>
#include <Godot/classes/node.hpp>
#include <Godot/classes/node3d.hpp>
#include <Godot/classes/sprite3d.hpp>
#include <Godot/classes/mesh_instance3d.hpp>
#include <Godot/classes/label3d.hpp>
#include <Godot/classes/cpu_particles3d.hpp>
#include <Godot/classes/quad_mesh.hpp>
#include <Godot/classes/standard_material3d.hpp>
#include <Godot/classes/base_material3d.hpp>
#include <Godot/classes/gradient.hpp>
#include <Godot/classes/tween.hpp>
#include <Godot/classes/interval_tweener.hpp>
#include <Godot/classes/property_tweener.hpp>
#include <Godot/classes/callback_tweener.hpp>
#include <Godot/variant/utility_functions.hpp>
#include "Scripts/CPP/Combate/Reglas.hpp"
#include "Scripts/CPP/Combate/Animaciones.hpp"

using namespace godot;
using namespace jenova::sdk;
using namespace animacion;

namespace {
	const float T_EMPUJON = 0.11f;

	Node* PartyManager(Node* self) { return self->get_node_or_null("/root/Main/PartyManager"); }
	Node* Feel(Node* self) { return self->get_node_or_null("/root/Feel"); }

	Node3D* Ficha(Node* dungeon, const String& nombre) {
		return Object::cast_to<Node3D>(dungeon->get_node_or_null(NodePath(nombre)));
	}

	String NombreEnemigo(int x, int y) { return "Enemy_" + String::num_int64(x) + "_" + String::num_int64(y); }

	Node* HeroeActivo(Node* self) {
		Node* pm = PartyManager(self);
		return pm ? Object::cast_to<Node>(pm->call("GetActiveCharacter")) : nullptr;
	}

	bool Debug(Node* dungeon) { return dungeon->has_meta("debug_mode") && (bool)dungeon->get_meta("debug_mode"); }

	// Un arquero enemigo no puede contraatacar a quien le pega a bocajarro
	bool PuedeContraatacar(Node* enemigo, Node* heroe) {
		return reglas::EnAlcance(enemigo, enemigo->get_meta("grid_pos", Vector2i(-99, -99)), heroe->get_meta("grid_pos", Vector2i(99, 99)));
	}

	bool CuerpoACuerpo(Node* heroe, int x, int y) {
		return reglas::Distancia(heroe->get_meta("grid_pos", Vector2i(-99, -99)), Vector2i(x, y)) <= 1;
	}

	// El enemigo muere: el héroe se lleva lo que salga de su tabla de botín (ficha del enemigo)
	void BotinEnemigo(Node* self_node, Node* dungeon, Node* heroe, Vector3 pos, const String& tabla) {
		Node* cat = inventario::Catalogo(self_node);
		Node* pm = PartyManager(self_node);
		int piso = pm ? (int)pm->get_meta("piso_actual", 1) : 1;
		Array botin = cat ? Array(cat->call("tirar_botin", tabla, piso)) : Array();
		Array restos = inventario::EntregarBotin(heroe, botin, pos + Vector3(0, 0.6, 0));
		if (!restos.is_empty()) TextoFlotante(dungeon, pos + Vector3(0, 0.4, 0), "Mochila llena", Color(1, 0.5, 0.3), 32);
	}
}

JENOVA_SCRIPT_BEGIN

void OnAwake(Caller* instance) {}
void OnDestroy(Caller* instance) {}
void OnReady(Caller* instance) {}

// ==========================================
// PREVISIÓN (tarjeta de ataque del HUD)
// ==========================================
Dictionary PrevisionAtaque(Caller* instance, int x, int y) {
	Dictionary d;
	d["valido"] = false;
	Node* self_node = GetSelf<Node>(instance);
	Node* dungeon = self_node->get_parent();
	Node* heroe = HeroeActivo(self_node);
	Node* enemigo = dungeon ? dungeon->get_node_or_null(NodePath(NombreEnemigo(x, y))) : nullptr;
	if (!heroe || !enemigo) return d;

	int dano = reglas::DanoBase(heroe, enemigo);
	int dano_critico = reglas::DanoCritico(heroe, dano);
	int hp = enemigo->get_meta("hp_actual", 1);
	bool cuerpo = CuerpoACuerpo(heroe, x, y);
	d["valido"] = true;
	d["atacante"] = heroe->get_meta("nombre_display", heroe->get_name());
	d["defensor"] = enemigo->get_meta("nombre_display", "Enemigo");
	d["dano"] = dano;
	d["dano_critico"] = dano_critico;
	d["prob_critico"] = reglas::ProbCritico(heroe);
	d["hp"] = hp;
	d["hp_max"] = enemigo->get_meta("hp_max", 1);
	d["mata"] = dano >= hp;
	d["mata_critico"] = dano_critico >= hp;
	d["contra"] = cuerpo && PuedeContraatacar(enemigo, heroe) ? reglas::DanoBase(enemigo, heroe) : 0; // a distancia no hay contraataque
	d["a_distancia"] = !cuerpo;
	d["en_alcance"] = (bool)dungeon->call("PuedeAtacarA", x, y); // alcance del arma + línea de visión
	d["coste"] = Debug(dungeon) ? 0 : reglas::CosteAtaque(heroe);
	d["pa"] = heroe->get_meta("pa_actual", 0);
	return d;
}

// ==========================================
// SECUENCIA DE ATAQUE
// ==========================================
void FinAtaque(Caller* instance) {
	Node* self_node = GetSelf<Node>(instance);
	Node* dungeon = self_node->get_parent();
	Node* pm = PartyManager(self_node);
	if (!dungeon || !pm) return;
	dungeon->set_meta("is_moving", false);

	Node* ui = self_node->get_node_or_null("/root/Main/CanvasLayer");
	if (ui) ui->call("UpdateWarriorUI");

	Node* heroe = HeroeActivo(self_node);
	bool vivo = heroe && !(heroe->has_meta("esta_muerto") && (bool)heroe->get_meta("esta_muerto"));
	if (!heroe || !vivo || (int)heroe->get_meta("pa_actual", 0) <= 0) pm->call("TerminarTurnoPersonaje");
	dungeon->call("RefrescarLuces");
}

void ImpactoContra(Caller* instance, int x, int y, int dano) {
	Node* self_node = GetSelf<Node>(instance);
	Node* dungeon = self_node->get_parent();
	Node* pm = PartyManager(self_node);
	Node* heroe = HeroeActivo(self_node);
	if (!dungeon || !pm || !heroe) { FinAtaque(instance); return; }

	Node3D* ficha_h = Ficha(dungeon, heroe->get_name());
	Node3D* ficha_e = Ficha(dungeon, NombreEnemigo(x, y));
	if (ficha_h) {
		TextoFlotante(dungeon, ficha_h->get_position(), String::num_int64(dano), Color(1, 0.3, 0.25));
		Destello(ficha_h, Color(3.0, 0.6, 0.6));
		Desplazar(ficha_h, Direccion(ficha_e, ficha_h), 0.14f, 0.05f, 0.15f);
	}
	Node* feel = Feel(self_node);
	if (feel) { feel->call("sacudir", 0.22); feel->call("pausa_impacto", 0.04); }
	Node* audio = self_node->get_node_or_null("/root/AudioManager");
	if (audio) audio->call("play_sfx", "fail");

	pm->call("RecibirDano", dano, heroe);

	Ref<Tween> tw = self_node->create_tween();
	tw->tween_interval(0.35);
	tw->tween_callback(Callable(self_node, StringName("FinAtaque")));
}

void Contraataque(Caller* instance, int x, int y) {
	Node* self_node = GetSelf<Node>(instance);
	Node* dungeon = self_node->get_parent();
	Node* heroe = HeroeActivo(self_node);
	Node3D* ficha_e = dungeon ? Ficha(dungeon, NombreEnemigo(x, y)) : nullptr;
	if (!heroe || !ficha_e) { FinAtaque(instance); return; }

	Node3D* ficha_h = Ficha(dungeon, heroe->get_name());
	Desplazar(ficha_e, Direccion(ficha_e, ficha_h), 0.32f, T_EMPUJON, 0.16f);
	int dano = reglas::DanoBase(ficha_e, heroe);

	Ref<Tween> tw = self_node->create_tween();
	tw->tween_interval(T_EMPUJON);
	tw->tween_callback(Callable(self_node, StringName("ImpactoContra")).bind(x, y, dano));
}

void ImpactoAtaque(Caller* instance, int x, int y, int dano, int critico) {
	Node* self_node = GetSelf<Node>(instance);
	Node* dungeon = self_node->get_parent();
	Node* heroe = HeroeActivo(self_node);
	Node3D* enemigo = dungeon ? Ficha(dungeon, NombreEnemigo(x, y)) : nullptr;
	if (!heroe || !enemigo) { FinAtaque(instance); return; }

	int hp = (int)enemigo->get_meta("hp_actual", 1) - dano;
	enemigo->set_meta("hp_actual", hp);
	UtilityFunctions::print("[CombatManager] ", heroe->get_name(), " golpea por ", dano, critico ? " (CRITICO)" : "", ". Le quedan ", UtilityFunctions::maxi(0, hp), " PV.");

	Node3D* ficha_h = Ficha(dungeon, heroe->get_name());
	Vector3 dir = Direccion(ficha_h, enemigo);
	TextoFlotante(dungeon, enemigo->get_position(), String::num_int64(dano), critico ? Color(1, 0.85, 0.2) : Color(1, 1, 1), critico ? 64 : 48);
	if (critico) TextoFlotante(dungeon, enemigo->get_position() + Vector3(0, 0.35, 0), String::utf8("\xC2\xA1" "CR\xC3\x8DTICO!"), Color(1, 0.6, 0.15), 36);
	Destello(enemigo, Color(4, 4, 4));
	Desplazar(enemigo, dir, 0.18f, 0.05f, 0.2f);

	// Efectos del arma al golpear (veneno, robo de vida...)
	String extra = inventario::AplicarEfectos(reglas::EfectosAlGolpear(heroe), enemigo);
	if (!extra.is_empty()) TextoFlotante(dungeon, enemigo->get_position() + Vector3(0, -0.25, 0), extra, Color(0.75, 0.55, 1.0), 32);
	hp = enemigo->get_meta("hp_actual", 1);

	Node* feel = Feel(self_node);
	if (feel) {
		feel->call("sacudir", critico ? 0.55 : 0.28);
		feel->call("pausa_impacto", critico ? 0.1 : 0.06);
		if (critico) feel->call("golpe_camara", 1.0);
	}
	Node* audio = self_node->get_node_or_null("/root/AudioManager");
	if (audio) audio->call("play_sfx", "success");

	Ref<Tween> tw = self_node->create_tween();
	if (hp <= 0) {
		// Muerte: estallido de píxeles y el botín vuela a la ficha
		Estallido(dungeon, enemigo->get_position(), Color(0.85, 0.8, 0.7));
		BotinEnemigo(self_node, dungeon, heroe, enemigo->get_position(), enemigo->get_meta("tabla_botin", "enemigo"));
		dungeon->call("EliminarEnemigo", x, y);
		tw->tween_interval(0.45);
		tw->tween_callback(Callable(self_node, StringName("FinAtaque")));
	} else if (CuerpoACuerpo(heroe, x, y) && PuedeContraatacar(enemigo, heroe)) {
		// Pegados: el enemigo devuelve el golpe (si su arma llega)
		tw->tween_interval(0.32);
		tw->tween_callback(Callable(self_node, StringName("Contraataque")).bind(x, y));
	} else {
		tw->tween_interval(0.35);
		tw->tween_callback(Callable(self_node, StringName("FinAtaque")));
	}
}

// ==========================================
// 1. ATAQUE A UN ENEMIGO AL ALCANCE DEL ARMA
// ==========================================
// La línea de visión la comprueba el DungeonManager antes de llamar (es quien tiene el mapa).
void AtacarEnemigo(Caller* instance, int x, int y) {
	Node* self_node = GetSelf<Node>(instance);
	Node* dungeon = self_node->get_parent();
	Node* pm = PartyManager(self_node);
	if (!dungeon || !pm) return;

	Node* heroe = HeroeActivo(self_node);
	Node3D* enemigo = Ficha(dungeon, NombreEnemigo(x, y));
	if (!heroe || !enemigo) return;
	if (dungeon->has_meta("is_moving") && (bool)dungeon->get_meta("is_moving")) return;

	Node* audio = self_node->get_node_or_null("/root/AudioManager");
	Vector2i pos_heroe = heroe->get_meta("grid_pos", Vector2i(-99, -99));
	if (!reglas::EnAlcance(heroe, pos_heroe, Vector2i(x, y))) {
		UtilityFunctions::print("[CombatManager] El enemigo no est\xC3\xA1 al alcance del arma.");
		if (audio) audio->call("play_ui", "no");
		return;
	}
	int coste = Debug(dungeon) ? 0 : reglas::CosteAtaque(heroe);
	int pa = heroe->get_meta("pa_actual", 0);
	if (pa < coste) {
		UtilityFunctions::print("[CombatManager] Necesitas ", coste, " PA para atacar.");
		if (audio) audio->call("play_ui", "no");
		Node* feel = Feel(self_node);
		if (feel) feel->call("sacudir_ficha", String(heroe->get_name()));
		return;
	}
	heroe->set_meta("pa_actual", pa - coste);
	heroe->set_meta("in_combat", true);
	enemigo->set_meta("in_combat", true);

	int dano = reglas::DanoBase(heroe, enemigo);
	bool critico = (int)(UtilityFunctions::randi() % 100) < reglas::ProbCritico(heroe);
	if (critico) dano = reglas::DanoCritico(heroe, dano);
	bool cuerpo = reglas::Distancia(pos_heroe, Vector2i(x, y)) == 1;

	// Bloqueamos el tablero mientras dura la secuencia
	dungeon->set_meta("is_moving", true);
	dungeon->call("ApagarLucesMapa");
	Node* ui = self_node->get_node_or_null("/root/Main/CanvasLayer");
	if (ui) ui->call("UpdateWarriorUI");

	Node3D* ficha_h = Ficha(dungeon, heroe->get_name());
	float espera = T_EMPUJON;
	if (cuerpo) {
		Desplazar(ficha_h, Direccion(ficha_h, enemigo), 0.34f, T_EMPUJON, 0.16f);
	} else {
		// A distancia: pequeño retroceso y el proyectil viaja hasta el enemigo
		Desplazar(ficha_h, -Direccion(ficha_h, enemigo), 0.1f, 0.06f, 0.14f);
		Ref<Resource> arma = inventario::Arma(heroe);
		bool magia = arma.is_valid() && (int)arma->get("animacion") == 4; // CONJURO
		if (ficha_h) espera = Proyectil(dungeon, ficha_h->get_position(), enemigo->get_position(), magia ? Color(0.5, 0.8, 1.0) : Color(0.95, 0.9, 0.75));
	}

	Ref<Tween> tw = self_node->create_tween();
	tw->tween_interval(espera);
	tw->tween_callback(Callable(self_node, StringName("ImpactoAtaque")).bind(x, y, dano, critico ? 1 : 0)); // si es cuerpo a cuerpo lo calcula ImpactoAtaque
}

// ==========================================
// 2. RECEPCIÓN DE BOTONES DEL MENÚ TÁCTICO
// ==========================================
void OnBotonAtacar(Caller* instance) {
	Node* self_node = GetSelf<Node>(instance);
	Node* dungeon = self_node->get_parent();
	if (!dungeon) return;

	Vector2i objetivo = dungeon->call("BuscarEnemigoAdyacente");
	if (objetivo.x < 0) {
		UtilityFunctions::print("[CombatManager] No hay ning\xC3\xBAn enemigo al alcance.");
		Node* audio = self_node->get_node_or_null("/root/AudioManager");
		if (audio) audio->call("play_ui", "no");
		dungeon->call("RefrescarLuces");
		return;
	}
	AtacarEnemigo(instance, objetivo.x, objetivo.y);
}

void OnBotonHabilidad(Caller* instance) {
	UtilityFunctions::print("[CombatManager] Habilidades: todavía no implementadas.");
	Node* dungeon = GetSelf<Node>(instance)->get_parent();
	if (dungeon) dungeon->call("RefrescarLuces");
}

void OnBotonPasar(Caller* instance) {
	Node* self_node = GetSelf<Node>(instance);
	Node* dungeon = self_node->get_parent();
	Node* pm = PartyManager(self_node);
	if (!dungeon || !pm) return;

	UtilityFunctions::print("[CombatManager] ¡Pasando turno!");

	Node* heroe = HeroeActivo(self_node);
	if (heroe) {
		heroe->set_meta("pa_actual", 0);
		Vector2i pos = heroe->get_meta("grid_pos", Vector2i(0, 0));
		dungeon->call("AtLast", pos.x, pos.y, 0);
	}
}

// Árbitro genérico (por si un evento quiere enfrentar a dos nodos)
void EjecutarAtaqueFisico(Caller* instance, Node* atacante, Node* defensor) {
	if (!atacante || !defensor) return;
	int dano = reglas::DanoBase(atacante, defensor);
	if ((int)(UtilityFunctions::randi() % 100) < reglas::ProbCritico(atacante)) dano = reglas::DanoCritico(atacante, dano);
	Node* pm = PartyManager(GetSelf<Node>(instance));
	if (pm) pm->call("RecibirDano", dano, defensor);
}

JENOVA_SCRIPT_END
