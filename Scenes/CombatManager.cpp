/* Jenova C++ Node Base Script - CombatManager */
#include <Godot/godot.hpp>
#include <Godot/classes/node.hpp>
#include <Godot/classes/node3d.hpp>
#include <Godot/classes/sprite3d.hpp>
#include <Godot/classes/label3d.hpp>
#include <Godot/classes/base_material3d.hpp>
#include <Godot/classes/tween.hpp>
#include <Godot/classes/interval_tweener.hpp>
#include <Godot/classes/property_tweener.hpp>
#include <Godot/classes/callback_tweener.hpp>
#include <Godot/variant/utility_functions.hpp>

using namespace godot;
using namespace jenova::sdk;

namespace {
	const int COSTE_ATAQUE = 2;

	Node* PartyManager(Node* self) { return self->get_node_or_null("/root/Main/PartyManager"); }

	// Número flotante sobre una ficha (daño, "¡CRÍTICO!", etc.)
	void TextoFlotante(Node* dungeon, Vector3 pos, const String& texto, Color color) {
		Label3D* label = memnew(Label3D);
		label->set_text(texto);
		label->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
		label->set_draw_flag(Label3D::FLAG_DISABLE_DEPTH_TEST, true);
		label->set_texture_filter(BaseMaterial3D::TEXTURE_FILTER_NEAREST);
		label->set_modulate(color);
		label->set_outline_modulate(Color(0, 0, 0));
		label->set_font_size(48);
		label->set_outline_size(10);
		label->set_pixel_size(0.008f);
		label->set_position(pos + Vector3(0, 1.0, 0));
		dungeon->add_child(label);

		Ref<Tween> tw = label->create_tween();
		tw->tween_property(label, "position:y", pos.y + 1.6, 0.7)->set_trans(Tween::TRANS_QUAD)->set_ease(Tween::EASE_OUT);
		tw->parallel()->tween_property(label, "modulate:a", 0.0, 0.7)->set_delay(0.3);
		tw->tween_callback(Callable(label, StringName("queue_free")));
	}

	void Parpadeo(Node3D* ficha) {
		if (!ficha) return;
		Sprite3D* cuerpo = Object::cast_to<Sprite3D>(ficha->get_node_or_null("Cuerpo"));
		if (!cuerpo) return;
		Ref<Tween> tw = cuerpo->create_tween();
		tw->tween_property(cuerpo, "modulate", Color(3, 0.4, 0.4), 0.06);
		tw->tween_property(cuerpo, "modulate", Color(1, 1, 1), 0.2);
	}

	// Fórmula básica: (Fuerza - Defensa), mínimo 1; crítico x2 según la Suerte
	int CalcularDano(Node* atacante, Node* defensor, bool& critico) {
		int fuerza = atacante->get_meta("fuerza", 1);
		int defensa = defensor->get_meta("defensa", 0);
		int suerte = atacante->get_meta("suerte", 1);
		int dano = UtilityFunctions::maxi(1, fuerza - defensa);
		critico = (int)(UtilityFunctions::randi() % 100) < suerte * 2;
		if (critico) dano *= 2;
		return dano;
	}
}

JENOVA_SCRIPT_BEGIN

void OnAwake(Caller* instance) {}
void OnDestroy(Caller* instance) {}
void OnReady(Caller* instance) {}

// ==========================================
// 1. ATAQUE A UN ENEMIGO ADYACENTE
// ==========================================
void AtacarEnemigo(Caller* instance, int x, int y) {
	Node* self_node = GetSelf<Node>(instance);
	Node* dungeon = self_node->get_parent();
	Node* pm = PartyManager(self_node);
	if (!dungeon || !pm) return;

	Node* heroe = Object::cast_to<Node>(pm->call("GetActiveCharacter"));
	Node3D* enemigo = Object::cast_to<Node3D>(dungeon->get_node_or_null(NodePath("Enemy_" + String::num_int64(x) + "_" + String::num_int64(y))));
	if (!heroe || !enemigo) return;

	Node* audio = self_node->get_node_or_null("/root/AudioManager");
	bool debug = dungeon->has_meta("debug_mode") && (bool)dungeon->get_meta("debug_mode");
	int coste = debug ? 0 : COSTE_ATAQUE;
	int pa = heroe->get_meta("pa_actual", 0);
	if (pa < coste) {
		UtilityFunctions::print("[CombatManager] Necesitas ", coste, " PA para atacar.");
		if (audio) audio->call("play_ui", "no");
		return;
	}
	heroe->set_meta("pa_actual", pa - coste);
	heroe->set_meta("in_combat", true);
	enemigo->set_meta("in_combat", true);

	bool critico = false;
	int dano = CalcularDano(heroe, enemigo, critico);
	int hp = (int)enemigo->get_meta("hp_actual", 1) - dano;
	enemigo->set_meta("hp_actual", hp);

	UtilityFunctions::print("[CombatManager] ", heroe->get_name(), " golpea al esqueleto por ", dano, critico ? " (¡CRÍTICO!)" : "", ". Le quedan ", UtilityFunctions::maxi(0, hp), " PV.");
	TextoFlotante(dungeon, enemigo->get_position(), (critico ? String::utf8("\xC2\xA1") : String()) + String::num_int64(dano) + (critico ? String("!") : String()), critico ? Color(1, 0.85, 0.2) : Color(1, 1, 1));
	Parpadeo(enemigo);
	if (audio) audio->call("play_sfx", "success");

	if (hp <= 0) {
		dungeon->call("EliminarEnemigo", x, y);
	} else {
		// Contraataque del enemigo
		int fuerza = enemigo->get_meta("fuerza", 3);
		int defensa = heroe->get_meta("defensa", 0);
		int dano_rec = UtilityFunctions::maxi(1, fuerza - defensa);
		Node3D* ficha = Object::cast_to<Node3D>(dungeon->get_node_or_null(NodePath(String(heroe->get_name()))));
		if (ficha) {
			TextoFlotante(dungeon, ficha->get_position(), String::num_int64(dano_rec), Color(1, 0.25, 0.25));
			Parpadeo(ficha);
		}
		pm->call("RecibirDano", dano_rec, heroe);
	}

	Node* ui = self_node->get_node_or_null("/root/Main/CanvasLayer");
	if (ui) ui->call("UpdateWarriorUI");

	bool vivo = !(heroe->has_meta("esta_muerto") && (bool)heroe->get_meta("esta_muerto"));
	if (!vivo || (int)heroe->get_meta("pa_actual", 0) <= 0) pm->call("TerminarTurnoPersonaje");
	dungeon->call("RefrescarLuces");
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

	Node* heroe = Object::cast_to<Node>(pm->call("GetActiveCharacter"));
	if (heroe) {
		heroe->set_meta("pa_actual", 0);
		Vector2i pos = heroe->get_meta("grid_pos", Vector2i(0, 0));
		dungeon->call("AtLast", pos.x, pos.y, 0);
	}
}

// Árbitro genérico (por si un evento quiere enfrentar a dos nodos)
void EjecutarAtaqueFisico(Caller* instance, Node* atacante, Node* defensor) {
	if (!atacante || !defensor) return;
	bool critico = false;
	int dano = CalcularDano(atacante, defensor, critico);
	Node* pm = PartyManager(GetSelf<Node>(instance));
	if (pm) pm->call("RecibirDano", dano, defensor);
}

JENOVA_SCRIPT_END
