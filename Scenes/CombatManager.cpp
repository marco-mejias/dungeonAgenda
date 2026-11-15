/* Jenova C++ Node Base Script - CombatManager */

// Combat as an animated sequence: attacker lunge (or a projectile if the weapon is ranged) ->
// impact (hit stop, shake, flash, knockback, number, weapon effects) -> death (burst + loot flies
// to the card) or, if they were adjacent, an animated counterattack -> end of the action. Damage,
// cost, critical and reach come from the equipped weapon (Combate/Reglas.hpp). While it lasts the
// board is locked (is_moving). Screen effects are done by the Feel autoload.

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
#include <Godot/classes/camera3d.hpp>
#include <Godot/variant/utility_functions.hpp>
#include "Scripts/CPP/Combate/Reglas.hpp"
#include "Scripts/CPP/Combate/Animaciones.hpp"

using namespace godot;
using namespace jenova::sdk;
using namespace animacion;

namespace {
	const float T_EMPUJON = 0.11f;
	// Enemies only hit on their turn: attacking them doesn't make them hit back (the counterattack
	// code stays in case some enemy or weapon needs it later)
	const bool HAY_CONTRAATAQUE = false;

	Node* PartyManager(Node* self) { return self->get_node_or_null("/root/Main/PartyManager"); }
	Node* Feel(Node* self) { return self->get_node_or_null("/root/Feel"); }

	// 3D token by node name (heroes go by name, enemies by "Enemy_x_y")
	Node3D* Ficha(Node* dungeon, const String& nombre) {
		return Object::cast_to<Node3D>(dungeon->get_node_or_null(NodePath(nombre)));
	}

	String NombreEnemigo(int x, int y) { return "Enemy_" + String::num_int64(x) + "_" + String::num_int64(y); }

	// The hero whose turn it is (asked to the PartyManager)
	Node* HeroeActivo(Node* self) {
		Node* pm = PartyManager(self);
		return pm ? Object::cast_to<Node>(pm->call("GetActiveCharacter")) : nullptr;
	}

	bool Debug(Node* dungeon) { return dungeon->has_meta("debug_mode") && (bool)dungeon->get_meta("debug_mode"); }

	// An enemy archer can't counterattack whoever hits them point-blank
	bool PuedeContraatacar(Node* enemigo, Node* heroe) {
		return reglas::EnAlcance(enemigo, enemigo->get_meta("grid_pos", Vector2i(-99, -99)), heroe->get_meta("grid_pos", Vector2i(99, 99)));
	}

	// Is the enemy right next to the hero? (melee range)
	bool CuerpoACuerpo(Node* heroe, int x, int y) {
		return reglas::Distancia(heroe->get_meta("grid_pos", Vector2i(-99, -99)), Vector2i(x, y)) <= 1;
	}

	// An enemy that attacks becomes "disobedient" (the flip-flop is after them) and loses the
	// distraction and one turn of blindness
	void DespuesDeAtacar(Node* e) {
		if (!e) return;
		e->set_meta("desobediente", true);
		if (e->has_meta("distraido")) e->remove_meta("distraido");
		if (e->has_meta("cegado")) e->set_meta("cegado", std::max(0, (int)e->get_meta("cegado") - 1));
	}

	// Shows the roll on the small die over the attacker (DadoCombate autoload): just the number and
	// CRITICAL or MISS. Returns how long it takes to show.
	float MostrarTirada(Node* self, const reglas::Tirada& t, Node3D* quien) {
		Node* dado = self->get_node_or_null("/root/DadoCombate");
		if (!dado) return 0.0f;
		String sello = !t.acierta ? String("FALLO") : (t.critico ? String::utf8("\xC2\xA1" "CR\xC3\x8D" "TICO!") : String(""));
		Variant espera = dado->call("tirar", t.d20, t.acierta ? 1 : 0, sello, String(""), quien, t.ventaja);
		return espera.get_type() == Variant::FLOAT ? (float)(double)espera : 1.0f;
	}

	Camera3D* Camara(Node* dungeon) { return dungeon ? Object::cast_to<Camera3D>(dungeon->get_node_or_null("Camera3D")) : nullptr; }

	// The camera frames these characters (CameraManager: meta "encuadre")
	void Encuadrar(Node* dungeon, Node3D* a, Node3D* b = nullptr) {
		Camera3D* cam = Camara(dungeon);
		if (!cam || !a) return;
		PackedVector3Array puntos;
		puntos.push_back(a->get_position());
		if (b) puntos.push_back(b->get_position());
		cam->set_meta("encuadre", puntos);
	}

	// Gives the camera back to the hero once the hit is over
	void SoltarEncuadre(Node* dungeon) {
		Camera3D* cam = Camara(dungeon);
		if (cam && cam->has_meta("encuadre")) cam->remove_meta("encuadre");
	}

	// Floating indicators: show what changed right now (before the node goes away)
	void RevisarIndicadores(Node* self, Node* quien) {
		Node* ind = self->get_node_or_null("/root/Indicadores");
		if (ind && quien) ind->call("revisar", quien);
	}

	// Tactica.gd: after an attack roll, whatever lasts one roll is spent (an ally's help)
	void TrasAtacar(Node* self, Node* atacante) {
		Node* tac = self->get_node_or_null("/root/Tactica");
		if (tac && atacante) tac->call("tras_atacar", atacante);
	}

	// Progreso.gd (XP, levels and passives): reports a hit, critical, miss...
	void Disparar(Node* self, const char* evento, Node* heroe, Node* objetivo, bool puede_matar) {
		Node* prog = self->get_node_or_null("/root/Progreso");
		if (prog && heroe) prog->call("disparar", String(evento), heroe, objetivo, puede_matar);
	}

	// Strip with the ability name (the turn-change one, but shorter)
	void AnunciarHabilidad(Node* self, const String& nombre) {
		Node* banner = self->get_node_or_null("/root/Main/CanvasLayer/Banner");
		if (banner) banner->call("anunciar", nombre);
	}

	// The enemy dies: the hero takes whatever comes out of its loot table (enemy card)
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
// PREVIEW (HUD attack card)
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
	Dictionary mods = reglas::Modificadores(heroe, enemigo);
	d["valido"] = true;
	d["atacante"] = heroe->get_meta("nombre_display", heroe->get_name());
	d["defensor"] = enemigo->get_meta("nombre_display", "Enemigo");
	d["dano"] = dano;
	d["dano_critico"] = dano_critico;
	d["prob_critico"] = reglas::ProbCritico(heroe);
	d["prob_acierto"] = reglas::ProbAcierto(heroe, enemigo);
	d["bono"] = reglas::BonoAtaque(heroe) + (int)mods.get("bono", 0);
	d["ca"] = reglas::ClaseArmadura(enemigo);
	d["hp"] = hp;
	d["hp_max"] = enemigo->get_meta("hp_max", 1);
	// Positioning: advantage/disadvantage and why (flanking, height, cover...)
	d["ventaja"] = mods.get("ventaja", 0);
	d["razones"] = mods.get("razones", PackedStringArray());
	d["mata"] = dano >= hp;
	d["mata_critico"] = dano_critico >= hp;
	// The target stats this hit touches: health and whatever the weapon effects change
	Array stats;
	int dano_extra = 0;
	Dictionary cambios; // stat -> sum
	Array efectos = reglas::EfectosAlGolpear(heroe);
	for (int i = 0; i < efectos.size(); i++) {
		Ref<Resource> ef = efectos[i];
		if (ef.is_null()) continue;
		int tipo = ef->get("tipo"), valor = ef->get("valor");
		if (tipo == inventario::E_DANAR) dano_extra += valor;
		else if (tipo == inventario::E_MODIFICAR_STAT) {
			String s = ef->get("stat");
			cambios[s] = (int)cambios.get(s, 0) + valor;
		}
	}
	Dictionary vida;
	vida["stat"] = "hp_actual";
	vida["actual"] = hp;
	vida["despues"] = std::max(0, hp - dano - dano_extra);
	vida["max"] = enemigo->get_meta("hp_max", 1);
	stats.push_back(vida);
	Array claves = cambios.keys();
	for (int i = 0; i < claves.size(); i++) {
		String s = claves[i];
		Dictionary st;
		st["stat"] = s;
		st["actual"] = enemigo->get_meta(s, 0);
		st["despues"] = (int)enemigo->get_meta(s, 0) + (int)cambios[s];
		stats.push_back(st);
	}
	d["stats"] = stats;
	d["contra"] = HAY_CONTRAATAQUE && cuerpo && PuedeContraatacar(enemigo, heroe) ? reglas::DanoBase(enemigo, heroe) : 0; // No counterattack at range
	d["a_distancia"] = !cuerpo;
	d["en_alcance"] = (bool)dungeon->call("PuedeAtacarA", x, y); // Weapon reach + line of sight
	d["coste"] = Debug(dungeon) ? 0 : reglas::CosteAtaque(heroe);
	d["pa"] = heroe->get_meta("pa_actual", 0);
	return d;
}

// ==========================================
// ATTACK SEQUENCE
// ==========================================
void FinAtaque(Caller* instance) {
	Node* self_node = GetSelf<Node>(instance);
	Node* dungeon = self_node->get_parent();
	Node* pm = PartyManager(self_node);
	if (!dungeon || !pm) return;
	dungeon->set_meta("is_moving", false);
	SoltarEncuadre(dungeon);

	Node* ui = self_node->get_node_or_null("/root/Main/CanvasLayer");
	if (ui) ui->call("UpdateWarriorUI");

	Node* heroe = HeroeActivo(self_node);
	bool vivo = heroe && !(heroe->has_meta("esta_muerto") && (bool)heroe->get_meta("esta_muerto"));
	if (!heroe || !vivo || (int)heroe->get_meta("pa_actual", 0) <= 0) dungeon->call("PedirFinDeTurno");
	dungeon->call("RefrescarLuces");
}

// The enemy's counterattack lands on the hero: damage, flash, knockback and the hero's passives
void ImpactoContra(Caller* instance, int x, int y, int dano) {
	Node* self_node = GetSelf<Node>(instance);
	Node* dungeon = self_node->get_parent();
	Node* pm = PartyManager(self_node);
	Node* heroe = HeroeActivo(self_node);
	if (!dungeon || !pm || !heroe) { FinAtaque(instance); return; }

	Node3D* ficha_h = Ficha(dungeon, heroe->get_name());
	Node3D* ficha_e = Ficha(dungeon, NombreEnemigo(x, y));
	if (ficha_h) {
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

// The counterattack misses its roll: "Miss" text over the hero
void FalloContra(Caller* instance, int x, int y) {
	Node* self_node = GetSelf<Node>(instance);
	Node* dungeon = self_node->get_parent();
	Node* heroe = HeroeActivo(self_node);
	Node3D* ficha_h = (dungeon && heroe) ? Ficha(dungeon, heroe->get_name()) : nullptr;
	if (ficha_h) TextoFlotante(dungeon, ficha_h->get_position(), "Fallo", Color(0.75, 0.75, 0.8), 40);
	Ref<Tween> tw = self_node->create_tween();
	tw->tween_interval(0.35);
	tw->tween_callback(Callable(self_node, StringName("FinAtaque")));
}

// The enemy hits back (only if they were adjacent and its weapon reaches)
void Contraataque(Caller* instance, int x, int y) {
	Node* self_node = GetSelf<Node>(instance);
	Node* dungeon = self_node->get_parent();
	Node* heroe = HeroeActivo(self_node);
	Node3D* ficha_e = dungeon ? Ficha(dungeon, NombreEnemigo(x, y)) : nullptr;
	if (!heroe || !ficha_e) { FinAtaque(instance); return; }

	Node3D* ficha_h = Ficha(dungeon, heroe->get_name());
	Desplazar(ficha_e, Direccion(ficha_e, ficha_h), 0.32f, T_EMPUJON, 0.16f);
	// The counterattack rolls its own d20 too (no die on screen, so combat doesn't slow down)
	reglas::Tirada tirada = reglas::TiradaAtaque(ficha_e, heroe);
	int dano = reglas::DanoBase(ficha_e, heroe);
	if (tirada.critico) dano = reglas::DanoCritico(ficha_e, dano);
	DespuesDeAtacar(ficha_e);

	Ref<Tween> tw = self_node->create_tween();
	tw->tween_interval(T_EMPUJON);
	if (tirada.acierta) tw->tween_callback(Callable(self_node, StringName("ImpactoContra")).bind(x, y, dano));
	else tw->tween_callback(Callable(self_node, StringName("FalloContra")).bind(x, y));
}

// The hero's hit lands: damage (doubled on a critical), weapon effects, passives and maybe death
void ImpactoAtaque(Caller* instance, int x, int y, int dano, int critico) {
	Node* self_node = GetSelf<Node>(instance);
	Node* dungeon = self_node->get_parent();
	Node* heroe = HeroeActivo(self_node);
	Node3D* enemigo = dungeon ? Ficha(dungeon, NombreEnemigo(x, y)) : nullptr;
	if (!heroe || !enemigo) { FinAtaque(instance); return; }

	if (critico < 0) {
		// The roll missed: the hit doesn't land (the die already shows the MISS)
		Disparar(self_node, "al_fallar", heroe, enemigo, false);
		if (inventario::Lleva(heroe, "chancleta_inquisicion")) {
			// The flip-flop bounces back and hits you
			Node3D* ficha_r = Ficha(dungeon, heroe->get_name());
			if (ficha_r) TextoFlotante(dungeon, ficha_r->get_position(), String::utf8("\xC2\xA1" "Rebota!"), Color(1, 0.75, 0.3), 36);
			Node* pm_r = PartyManager(self_node);
			if (pm_r) pm_r->call("RecibirDano", 1, heroe);
		}
		Node* audio_f = self_node->get_node_or_null("/root/AudioManager");
		if (audio_f) audio_f->call("play_ui", "no");
		Ref<Tween> tw_f = self_node->create_tween();
		if (HAY_CONTRAATAQUE && CuerpoACuerpo(heroe, x, y) && PuedeContraatacar(enemigo, heroe)) {
			tw_f->tween_interval(0.32);
			tw_f->tween_callback(Callable(self_node, StringName("Contraataque")).bind(x, y));
		} else {
			tw_f->tween_interval(0.35);
			tw_f->tween_callback(Callable(self_node, StringName("FinAtaque")));
		}
		return;
	}

	int hp = (int)enemigo->get_meta("hp_actual", 1) - dano;
	enemigo->set_meta("hp_actual", hp);
	UtilityFunctions::print("[CombatManager] ", heroe->get_name(), " golpea por ", dano, critico ? " (CRITICO)" : "", ". Le quedan ", UtilityFunctions::maxi(0, hp), " PV.");

	Node3D* ficha_h = Ficha(dungeon, heroe->get_name());
	Vector3 dir = Direccion(ficha_h, enemigo);
	// The number is shown by the Indicadores (heart with the damage over the head)
	Destello(enemigo, Color(4, 4, 4));
	Desplazar(enemigo, dir, 0.18f, 0.05f, 0.2f);

	// Multi-purpose sword: you comb your hair mid-fight and the enemy gets distracted
	if (inventario::Lleva(heroe, "espada_multiusos")) {
		enemigo->set_meta("distraido", 1);
		TextoFlotante(dungeon, enemigo->get_position() + Vector3(0, 0.6, 0), String::utf8("\xC2\xA1" "Distra\xC3\xAD" "do!"), Color(0.6, 0.85, 1.0), 30);
	}

	// Weapon effects on hit (poison, life steal...)
	inventario::AplicarEfectos(reglas::EfectosAlGolpear(heroe), enemigo); // Their numbers: Indicadores
	// Hero passives (extra damage, sparks, PA...): THIS enemy's death is handled here
	if (critico) Disparar(self_node, "al_critico", heroe, enemigo, false);
	Disparar(self_node, "al_golpear", heroe, enemigo, false);
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
		// Death: pixel burst and the loot flies to the card
		RevisarIndicadores(self_node, enemigo); // Let the last hit be seen before removing it
		Node* prog = self_node->get_node_or_null("/root/Progreso");
		if (prog) prog->call("enemigo_muerto", enemigo, heroe); // XP and "on kill" passives
		Estallido(dungeon, enemigo->get_position(), Color(0.85, 0.8, 0.7));
		BotinEnemigo(self_node, dungeon, heroe, enemigo->get_position(), enemigo->get_meta("tabla_botin", "enemigo"));
		dungeon->call("EliminarEnemigo", x, y);
		tw->tween_interval(0.45);
		tw->tween_callback(Callable(self_node, StringName("FinAtaque")));
	} else if (HAY_CONTRAATAQUE && CuerpoACuerpo(heroe, x, y) && PuedeContraatacar(enemigo, heroe)) {
		// Adjacent: the enemy hits back (if its weapon reaches)
		tw->tween_interval(0.32);
		tw->tween_callback(Callable(self_node, StringName("Contraataque")).bind(x, y));
	} else {
		tw->tween_interval(0.35);
		tw->tween_callback(Callable(self_node, StringName("FinAtaque")));
	}
}

// ==========================================
// 1. ATTACK AN ENEMY WITHIN WEAPON REACH
// ==========================================
// Line of sight is checked by the DungeonManager before calling (it owns the map).
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

	// Attack roll (d20): the die shows and, when it stops, the hit comes out
	reglas::Tirada tirada = reglas::TiradaAtaque(heroe, enemigo);
	int dano = reglas::DanoBase(heroe, enemigo);
	if (tirada.critico) dano = reglas::DanoCritico(heroe, dano);
	int critico = !tirada.acierta ? -1 : (tirada.critico ? 1 : 0);
	TrasAtacar(self_node, heroe);
	UtilityFunctions::print("[CombatManager] Tirada: d20 ", tirada.d20, " + ", tirada.bono, " = ", tirada.total, " vs CA ", tirada.ca, tirada.acierta ? " -> acierta" : " -> falla");

	// We lock the board while the sequence lasts
	dungeon->set_meta("is_moving", true);
	dungeon->call("ApagarLucesMapa");
	Node* ui = self_node->get_node_or_null("/root/Main/CanvasLayer");
	if (ui) ui->call("UpdateWarriorUI");

	// The camera frames the hero and the enemy (as close as possible) and the die shows over the
	// hero
	Node3D* ficha_atacante = Ficha(dungeon, heroe->get_name());
	Encuadrar(dungeon, ficha_atacante, enemigo);
	float espera_dado = MostrarTirada(self_node, tirada, ficha_atacante);
	Ref<Tween> tw_dado = self_node->create_tween();
	tw_dado->tween_interval(espera_dado);
	tw_dado->tween_callback(Callable(self_node, StringName("LanzarAtaque")).bind(x, y, dano, critico));
}

// After the roll: lunge (or projectile) and the impact
void LanzarAtaque(Caller* instance, int x, int y, int dano, int critico) {
	Node* self_node = GetSelf<Node>(instance);
	Node* dungeon = self_node->get_parent();
	Node* heroe = HeroeActivo(self_node);
	Node3D* enemigo = dungeon ? Ficha(dungeon, NombreEnemigo(x, y)) : nullptr;
	if (!heroe || !enemigo) { FinAtaque(instance); return; }
	Vector2i pos_heroe = heroe->get_meta("grid_pos", Vector2i(-99, -99));
	bool cuerpo = reglas::Distancia(pos_heroe, Vector2i(x, y)) == 1;

	Node3D* ficha_h = Ficha(dungeon, heroe->get_name());
	float espera = T_EMPUJON;
	if (cuerpo) {
		Desplazar(ficha_h, Direccion(ficha_h, enemigo), 0.34f, T_EMPUJON, 0.16f);
	} else {
		// Ranged: small recoil and the projectile travels to the enemy
		Desplazar(ficha_h, -Direccion(ficha_h, enemigo), 0.1f, 0.06f, 0.14f);
		Ref<Resource> arma = inventario::Arma(heroe);
		bool magia = arma.is_valid() && (int)arma->get("animacion") == 4; // SPELL
		if (ficha_h) espera = Proyectil(dungeon, ficha_h->get_position(), enemigo->get_position(), magia ? Color(0.5, 0.8, 1.0) : Color(0.95, 0.9, 0.75));
	}

	Ref<Tween> tw = self_node->create_tween();
	tw->tween_interval(espera);
	tw->tween_callback(Callable(self_node, StringName("ImpactoAtaque")).bind(x, y, dano, critico)); // critico: 1 critical, 0 normal, -1 miss
}

// ==========================================
// 2. TACTICAL MENU BUTTONS
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

// Ability: summon the egg-throwing chicken (handled by the Especiales autoload)
void OnBotonHabilidad(Caller* instance) {
	Node* self_node = GetSelf<Node>(instance);
	Node* dungeon = self_node->get_parent();
	Node* esp = self_node->get_node_or_null("/root/Especiales");
	Node* heroe = HeroeActivo(self_node);
	if (esp && heroe) {
		String error = esp->call("invocar_gallina", heroe);
		Node* audio = self_node->get_node_or_null("/root/AudioManager");
		if (!error.is_empty()) {
			UtilityFunctions::print("[CombatManager] ", error);
			if (audio) audio->call("play_ui", "no");
		} else if (dungeon) {
			// The ability is announced and the camera moves close to the caster
			AnunciarHabilidad(self_node, "Invocar gallina");
			Encuadrar(dungeon, Ficha(dungeon, heroe->get_name()));
			Ref<Tween> tw = self_node->create_tween();
			tw->tween_interval(1.3);
			tw->tween_callback(Callable(self_node, StringName("TerminarPrimerPlano")));
		}
		Node* ui = self_node->get_node_or_null("/root/Main/CanvasLayer");
		if (ui) ui->call("UpdateWarriorUI");
	}
	if (dungeon) dungeon->call("RefrescarLuces");
}

// An enemy falls to an ability or a passive (Progreso.gd): burst, loot for 'heroe' and gone
void MatarEnemigoPor(Caller* instance, String heroe, int x, int y) {
	Node* self_node = GetSelf<Node>(instance);
	Node* dungeon = self_node->get_parent();
	Node* pm = PartyManager(self_node);
	Node3D* enemigo = dungeon ? Ficha(dungeon, NombreEnemigo(x, y)) : nullptr;
	if (!dungeon || !enemigo) return;
	Node* h = (pm && !heroe.is_empty()) ? pm->get_node_or_null(NodePath(heroe)) : nullptr;
	if (!h) h = HeroeActivo(self_node);
	Estallido(dungeon, enemigo->get_position(), Color(0.85, 0.8, 0.7));
	if (h) BotinEnemigo(self_node, dungeon, h, enemigo->get_position(), enemigo->get_meta("tabla_botin", "enemigo"));
	dungeon->call("EliminarEnemigo", x, y);
}

// A free hit out of turn (attack of opportunity, and whatever comes later): roll with advantages,
// weapon damage, and if it dies, XP/loot. 'distancia' = how many tiles away it is (the weapon has
// to reach). Works both ways: hero -> enemy and enemy -> hero. Returns whether it hit.
Variant AtaqueLibre(Caller* instance, Node* atacante, Node* defensor, int distancia) {
	Node* self_node = GetSelf<Node>(instance);
	Node* dungeon = self_node->get_parent();
	Node* pm = PartyManager(self_node);
	if (!dungeon || !pm || !atacante || !defensor) return false;
	if (distancia < reglas::AlcanceMin(atacante) || distancia > reglas::AlcanceMax(atacante)) return false;
	bool defiende_heroe = !String(defensor->get_name()).begins_with("Enemy_");
	bool ataca_heroe = !String(atacante->get_name()).begins_with("Enemy_");
	Node3D* ficha_d = defiende_heroe ? Ficha(dungeon, defensor->get_name()) : Object::cast_to<Node3D>(defensor);
	Node3D* ficha_a = ataca_heroe ? Ficha(dungeon, atacante->get_name()) : Object::cast_to<Node3D>(atacante);

	reglas::Tirada t = reglas::TiradaAtaque(atacante, defensor);
	TrasAtacar(self_node, atacante);
	if (ficha_a && ficha_d) Desplazar(ficha_a, Direccion(ficha_a, ficha_d), 0.28f, T_EMPUJON, 0.14f);
	if (!t.acierta) {
		if (ficha_d) TextoFlotante(dungeon, ficha_d->get_position(), "Fallo", Color(0.75, 0.75, 0.8), 34);
		return false;
	}
	int dano = reglas::DanoBase(atacante, defensor);
	if (t.critico) dano = reglas::DanoCritico(atacante, dano);
	if (ficha_d) Destello(ficha_d, Color(3.0, 1.2, 0.8));
	Node* feel = Feel(self_node);
	if (feel) feel->call("sacudir", 0.2);
	if (defiende_heroe) {
		pm->call("RecibirDano", dano, defensor);
	} else {
		int hp = (int)defensor->get_meta("hp_actual", 1) - dano;
		defensor->set_meta("hp_actual", hp);
		if (hp <= 0) {
			Node* prog = self_node->get_node_or_null("/root/Progreso");
			if (prog) prog->call("enemigo_muerto", defensor, ataca_heroe ? atacante : nullptr);
			Vector2i p = defensor->get_meta("grid_pos");
			self_node->call("MatarEnemigoPor", ataca_heroe ? String(atacante->get_name()) : String(""), p.x, p.y);
		}
	}
	return true;
}

// The ability close-up ends (unless another framing sequence has started)
void TerminarPrimerPlano(Caller* instance) {
	Node* self_node = GetSelf<Node>(instance);
	Node* dungeon = self_node->get_parent();
	if (dungeon && !(dungeon->has_meta("is_moving") && (bool)dungeon->get_meta("is_moving"))) SoltarEncuadre(dungeon);
}

// "End turn" button: the active hero loses their remaining PA and the turn moves on
void OnBotonPasar(Caller* instance) {
	Node* self_node = GetSelf<Node>(instance);
	Node* dungeon = self_node->get_parent();
	Node* pm = PartyManager(self_node);
	if (!dungeon || !pm) return;

	UtilityFunctions::print(String::utf8("[CombatManager] \xC2\xA1Pasando turno!"));

	Node* heroe = HeroeActivo(self_node);
	if (heroe) {
		heroe->set_meta("pa_actual", 0);
		Vector2i pos = heroe->get_meta("grid_pos", Vector2i(0, 0));
		dungeon->call("AtLast", pos.x, pos.y, 0);
	}
}

// Generic referee (in case an event wants to pit two nodes against each other)
void EjecutarAtaqueFisico(Caller* instance, Node* atacante, Node* defensor) {
	if (!atacante || !defensor) return;
	reglas::Tirada tirada = reglas::TiradaAtaque(atacante, defensor);
	if (!tirada.acierta) return;
	int dano = reglas::DanoBase(atacante, defensor);
	if (tirada.critico) dano = reglas::DanoCritico(atacante, dano);
	Node* pm = PartyManager(GetSelf<Node>(instance));
	if (pm) pm->call("RecibirDano", dano, defensor);
}

JENOVA_SCRIPT_END
