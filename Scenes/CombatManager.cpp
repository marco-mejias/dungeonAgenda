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

using namespace godot;
using namespace jenova::sdk;

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

	// Número flotante sobre una ficha (daño, "¡CRÍTICO!", etc.) con un pequeño rebote
	void TextoFlotante(Node* dungeon, Vector3 pos, const String& texto, Color color, int tamano = 48) {
		Label3D* label = memnew(Label3D);
		label->set_text(texto);
		label->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
		label->set_draw_flag(Label3D::FLAG_DISABLE_DEPTH_TEST, true);
		label->set_texture_filter(BaseMaterial3D::TEXTURE_FILTER_NEAREST);
		label->set_modulate(color);
		label->set_outline_modulate(Color(0, 0, 0));
		label->set_font_size(tamano);
		label->set_outline_size(10);
		label->set_pixel_size(0.008f);
		label->set_position(pos + Vector3(0, 1.0, 0));
		label->set_scale(Vector3(0.4, 0.4, 0.4));
		dungeon->add_child(label);

		Ref<Tween> tw = label->create_tween();
		tw->tween_property(label, "scale", Vector3(1.3, 1.3, 1.3), 0.08)->set_trans(Tween::TRANS_BACK)->set_ease(Tween::EASE_OUT);
		tw->tween_property(label, "scale", Vector3(1, 1, 1), 0.1);
		tw->parallel()->tween_property(label, "position:y", pos.y + 1.6, 0.7)->set_trans(Tween::TRANS_QUAD)->set_ease(Tween::EASE_OUT);
		tw->parallel()->tween_property(label, "modulate:a", 0.0, 0.5)->set_delay(0.35);
		tw->tween_callback(Callable(label, StringName("queue_free")));
	}

	// Destello blanco (o del color dado) del sprite
	void Destello(Node3D* ficha, Color color) {
		if (!ficha) return;
		Sprite3D* cuerpo = Object::cast_to<Sprite3D>(ficha->get_node_or_null("Cuerpo"));
		if (!cuerpo) return;
		if (!cuerpo->has_meta("mod_base")) cuerpo->set_meta("mod_base", cuerpo->get_modulate());
		Color base = cuerpo->get_meta("mod_base");
		Ref<Tween> tw = cuerpo->create_tween();
		tw->tween_property(cuerpo, "modulate", color, 0.04);
		tw->tween_property(cuerpo, "modulate", base, 0.22);
	}

	// El cuerpo se desplaza hacia 'dir' y vuelve (empujón del atacante o retroceso del golpeado)
	void Desplazar(Node3D* ficha, Vector3 dir, float distancia, float t_ida, float t_vuelta) {
		if (!ficha) return;
		Node3D* cuerpo = Object::cast_to<Node3D>(ficha->get_node_or_null("Cuerpo"));
		if (!cuerpo) return;
		float y_base = cuerpo->has_meta("y_base") ? (float)cuerpo->get_meta("y_base") : 0.44f;
		Vector3 reposo(0, y_base, 0);
		Ref<Tween> tw = cuerpo->create_tween();
		tw->tween_property(cuerpo, "position", reposo + dir * distancia, t_ida)->set_trans(Tween::TRANS_QUAD)->set_ease(Tween::EASE_OUT);
		tw->tween_property(cuerpo, "position", reposo, t_vuelta)->set_trans(Tween::TRANS_QUAD)->set_ease(Tween::EASE_IN_OUT);
	}

	Vector3 Direccion(Node3D* desde, Node3D* hacia) {
		if (!desde || !hacia) return Vector3();
		Vector3 d = hacia->get_position() - desde->get_position();
		d.y = 0;
		return d.length() > 0.001f ? d.normalized() : Vector3();
	}

	// Estallido de píxeles al morir un enemigo
	void Estallido(Node* dungeon, Vector3 pos, Color color) {
		CPUParticles3D* p = memnew(CPUParticles3D);
		Ref<QuadMesh> quad;
		quad.instantiate();
		quad->set_size(Vector2(0.07, 0.07));
		Ref<StandardMaterial3D> mat;
		mat.instantiate();
		mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		mat->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
		mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
		quad->set_material(mat);
		p->set_mesh(quad);
		p->set_amount(36);
		p->set_lifetime(0.7);
		p->set_one_shot(true);
		p->set_explosiveness_ratio(1.0);
		p->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_SPHERE);
		p->set_emission_sphere_radius(0.2);
		p->set_direction(Vector3(0, 1, 0));
		p->set_spread(70.0);
		p->set_param_min(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 1.2);
		p->set_param_max(CPUParticles3D::PARAM_INITIAL_LINEAR_VELOCITY, 2.6);
		p->set_gravity(Vector3(0, -6.0, 0));
		Ref<Gradient> rampa;
		rampa.instantiate();
		rampa->set_color(0, color);
		rampa->set_color(1, Color(color.r * 0.4f, color.g * 0.4f, color.b * 0.4f, 0.0));
		p->set_color_ramp(rampa);
		p->set_position(pos + Vector3(0, 0.45, 0));
		dungeon->add_child(p);
		p->set_emitting(true);
		Ref<Tween> tw = p->create_tween();
		tw->tween_interval(1.2);
		tw->tween_callback(Callable(p, StringName("queue_free")));
	}

	bool CuerpoACuerpo(Node* heroe, int x, int y) {
		return reglas::Distancia(heroe->get_meta("grid_pos", Vector2i(-99, -99)), Vector2i(x, y)) <= 1;
	}

	// Flecha o chispa que viaja del héroe al enemigo (armas a distancia)
	float Proyectil(Node* dungeon, Vector3 desde, Vector3 hasta, Color color) {
		MeshInstance3D* bala = memnew(MeshInstance3D);
		Ref<QuadMesh> quad;
		quad.instantiate();
		quad->set_size(Vector2(0.14, 0.14));
		Ref<StandardMaterial3D> mat;
		mat.instantiate();
		mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		mat->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
		mat->set_albedo(color);
		mat->set_flag(BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);
		quad->set_material(mat);
		bala->set_mesh(quad);
		Vector3 alto(0, 0.55, 0);
		bala->set_position(desde + alto);
		dungeon->add_child(bala);
		float t = 0.08f + 0.05f * desde.distance_to(hasta);
		Ref<Tween> tw = bala->create_tween();
		tw->tween_property(bala, "position", hasta + alto, t)->set_trans(Tween::TRANS_SINE)->set_ease(Tween::EASE_IN);
		tw->tween_callback(Callable(bala, StringName("queue_free")));
		return t;
	}

	// El enemigo muere: el héroe se lleva lo que salga de la tabla de botín "enemigo"
	void BotinEnemigo(Node* self_node, Node* dungeon, Node* heroe, Vector3 pos) {
		Node* cat = inventario::Catalogo(self_node);
		Node* pm = PartyManager(self_node);
		int piso = pm ? (int)pm->get_meta("piso_actual", 1) : 1;
		Array botin = cat ? Array(cat->call("tirar_botin", String("enemigo"), piso)) : Array();
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
	d["contra"] = cuerpo ? reglas::DanoBase(enemigo, heroe) : 0; // a distancia no hay contraataque
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

void ImpactoAtaque(Caller* instance, int x, int y, int dano, int critico, int cuerpo_a_cuerpo) {
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
		BotinEnemigo(self_node, dungeon, heroe, enemigo->get_position());
		dungeon->call("EliminarEnemigo", x, y);
		tw->tween_interval(0.45);
		tw->tween_callback(Callable(self_node, StringName("FinAtaque")));
	} else if (cuerpo_a_cuerpo) {
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
	tw->tween_callback(Callable(self_node, StringName("ImpactoAtaque")).bind(x, y, dano, critico ? 1 : 0, cuerpo ? 1 : 0));
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
