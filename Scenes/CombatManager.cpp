/* Jenova C++ Node Base Script - CombatManager */

// Combate cuerpo a cuerpo como secuencia animada:
//   empujón del atacante -> impacto (pausa, sacudida, destello, retroceso, número)
//   -> muerte (estallido + oro a la ficha) o contraataque animado -> fin de la acción.
// Mientras dura, el tablero está bloqueado (is_moving). Los efectos de pantalla los hace el autoload Feel.

#include <Godot/godot.hpp>
#include <Godot/classes/node.hpp>
#include <Godot/classes/node3d.hpp>
#include <Godot/classes/sprite3d.hpp>
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

using namespace godot;
using namespace jenova::sdk;

namespace {
	const int COSTE_ATAQUE = 2;
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

	// Fórmula básica: (Fuerza - Defensa), mínimo 1; crítico x2 según la Suerte
	int DanoBase(Node* atacante, Node* defensor) {
		return UtilityFunctions::maxi(1, (int)atacante->get_meta("fuerza", 1) - (int)defensor->get_meta("defensa", 0));
	}
	int ProbCritico(Node* atacante) { return UtilityFunctions::mini(100, (int)atacante->get_meta("suerte", 1) * 2); }
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

	int dano = DanoBase(heroe, enemigo);
	int hp = enemigo->get_meta("hp_actual", 1);
	d["valido"] = true;
	d["atacante"] = heroe->get_meta("nombre_display", heroe->get_name());
	d["defensor"] = enemigo->get_meta("nombre_display", "Enemigo");
	d["dano"] = dano;
	d["dano_critico"] = dano * 2;
	d["prob_critico"] = ProbCritico(heroe);
	d["hp"] = hp;
	d["hp_max"] = enemigo->get_meta("hp_max", 1);
	d["mata"] = dano >= hp;
	d["mata_critico"] = dano * 2 >= hp;
	d["contra"] = DanoBase(enemigo, heroe);
	d["coste"] = Debug(dungeon) ? 0 : COSTE_ATAQUE;
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
	int dano = DanoBase(ficha_e, heroe);

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
		// Muerte: estallido de píxeles y un puñado de oro que vuela a la ficha
		Estallido(dungeon, enemigo->get_position(), Color(0.85, 0.8, 0.7));
		int oro = UtilityFunctions::randi_range(3, 8);
		heroe->set_meta("oro", (int)heroe->get_meta("oro", 0) + oro);
		if (feel) feel->call("volar_a_ficha", enemigo->get_position() + Vector3(0, 0.6, 0), String(heroe->get_name()), Rect2(96, 128, 16, 16), "+" + String::num_int64(oro));
		dungeon->call("EliminarEnemigo", x, y);
		tw->tween_interval(0.45);
		tw->tween_callback(Callable(self_node, StringName("FinAtaque")));
	} else {
		tw->tween_interval(0.32);
		tw->tween_callback(Callable(self_node, StringName("Contraataque")).bind(x, y));
	}
}

// ==========================================
// 1. ATAQUE A UN ENEMIGO ADYACENTE
// ==========================================
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
	int coste = Debug(dungeon) ? 0 : COSTE_ATAQUE;
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

	int dano = DanoBase(heroe, enemigo);
	bool critico = (int)(UtilityFunctions::randi() % 100) < ProbCritico(heroe);
	if (critico) dano *= 2;

	// Bloqueamos el tablero mientras dura la secuencia
	dungeon->set_meta("is_moving", true);
	dungeon->call("ApagarLucesMapa");
	Node* ui = self_node->get_node_or_null("/root/Main/CanvasLayer");
	if (ui) ui->call("UpdateWarriorUI");

	Node3D* ficha_h = Ficha(dungeon, heroe->get_name());
	Desplazar(ficha_h, Direccion(ficha_h, enemigo), 0.34f, T_EMPUJON, 0.16f);

	Ref<Tween> tw = self_node->create_tween();
	tw->tween_interval(T_EMPUJON);
	tw->tween_callback(Callable(self_node, StringName("ImpactoAtaque")).bind(x, y, dano, critico ? 1 : 0));
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
		UtilityFunctions::print("[CombatManager] No hay ningún enemigo a tu lado.");
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
	int dano = DanoBase(atacante, defensor);
	if ((int)(UtilityFunctions::randi() % 100) < ProbCritico(atacante)) dano *= 2;
	Node* pm = PartyManager(GetSelf<Node>(instance));
	if (pm) pm->call("RecibirDano", dano, defensor);
}

JENOVA_SCRIPT_END
