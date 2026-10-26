/* Jenova C++ Node Base Script - CombatManager */
#include <Godot/godot.hpp>
#include <Godot/classes/node.hpp>
#include <Godot/classes/node2d.hpp>
#include <Godot/classes/tween.hpp>
#include <Godot/classes/interval_tweener.hpp>
#include <Godot/classes/callback_tweener.hpp>
#include <Godot/variant/utility_functions.hpp>

using namespace godot;
using namespace jenova::sdk;

JENOVA_SCRIPT_BEGIN

void OnAwake(Caller* instance) {}
void OnDestroy(Caller* instance) {}
void OnReady(Caller* instance) {}

// ==========================================
// 1. RECEPCIÓN DE BOTONES DEL MENÚ TÁCTICO
// ==========================================
void OnBotonAtacar(Caller* instance) {
	godot::Node* self_node = GetSelf<godot::Node>(instance);
	godot::Node* padre_dungeon = self_node->get_parent();
	if (!padre_dungeon) return;

	godot::UtilityFunctions::print("[CombatManager] ¡Botón ATACAR pulsado! Preparando arma...");
	

	// Aquí le diremos al DungeonManager que pinte las casillas de ataque en ROJO
	// padre_dungeon->call("PintarRangoAtaque");
}

void OnBotonHabilidad(Caller* instance) {
	godot::Node* self_node = GetSelf<godot::Node>(instance);
	godot::Node* padre_dungeon = self_node->get_parent();
	if (!padre_dungeon) return;

	godot::UtilityFunctions::print("[CombatManager] ¡Botón HABILIDAD pulsado!");
	
}

void OnBotonPasar(Caller* instance) {
	godot::Node* self_node = GetSelf<godot::Node>(instance);
	godot::Node* padre_dungeon = self_node->get_parent();
	godot::Node* pm = self_node->get_node_or_null("/root/Main/PartyManager");
	
	if (!padre_dungeon || !pm) return;

	godot::UtilityFunctions::print("[CombatManager] ¡Pasando turno!");
	
	// 0. ESCUDO ANTI-CLIC FANTASMA
	padre_dungeon->set_meta("bloqueo_evento", true);


	// 2. Extraemos al héroe y le vaciamos los PA
	godot::Variant char_var = pm->call("GetActiveCharacter");
	if (char_var.get_type() != godot::Variant::NIL) {
		Node* act_char = godot::Object::cast_to<Node>(char_var.operator godot::Object*());
		if (act_char) {
			act_char->set_meta("pa_actual", 0);
			
			// 3. Limpiamos las luces del mapa
			padre_dungeon->call("ApagarLucesMapa");

			// 4. Simulamos el fin de turno (AtLast)
			Vector2i pos = act_char->has_meta("grid_pos") ? (Vector2i)act_char->get_meta("grid_pos") : Vector2i(0,0);
			padre_dungeon->call("AtLast", pos.x, pos.y, 0);
		}
	}

	// 5. LEVANTAR ESCUDO TRAS 0.25s
	Ref<Tween> tween = self_node->create_tween();
	tween->tween_interval(0.25);
	tween->tween_callback(godot::Callable(padre_dungeon, godot::StringName("set_meta")).bind("bloqueo_evento", false));
}

// ==========================================
// 2. MATEMÁTICAS DE COMBATE (El Árbitro)
// ==========================================
void EjecutarAtaqueFisico(Caller* instance, Node* atacante, Node* defensor) {
	if (!atacante || !defensor) return;

	// 1. Obtenemos estadísticas
	int atk_fuerza = atacante->has_meta("fuerza") ? (int)atacante->get_meta("fuerza") : 1;
	int def_armadura = defensor->has_meta("defensa") ? (int)defensor->get_meta("defensa") : 0;
	godot::String nombre_atk = atacante->get_meta("nombre_display");
	godot::String nombre_def = defensor->get_meta("nombre_display");

	// 2. Fórmula de Daño Básica (Fuerza - Defensa), daño mínimo 1
	int dano_final = atk_fuerza - def_armadura;
	if (dano_final < 1) dano_final = 1;

	// 3. Probabilidad de Crítico (Basado en la Suerte)
	int atk_suerte = atacante->has_meta("suerte") ? (int)atacante->get_meta("suerte") : 1;
	int roll = godot::UtilityFunctions::randi() % 100;
	if (roll < (atk_suerte * 2)) { 
		dano_final *= 2; // ¡Golpe Crítico!
		godot::UtilityFunctions::print("[CombatManager] ¡CRÍTICO de ", nombre_atk, "!");
	}

	// 4. Aplicar el daño mandándolo al PartyManager
	godot::UtilityFunctions::print("[CombatManager] ", nombre_atk, " ataca a ", nombre_def, " por ", dano_final, " pts.");
	
	godot::Node* pm = GetSelf<godot::Node>(instance)->get_node_or_null("/root/Main/PartyManager");
	if (pm) {
		godot::Variant v_dmg = dano_final;
		pm->call("RecibirDano", v_dmg, defensor);
	}
}

JENOVA_SCRIPT_END
