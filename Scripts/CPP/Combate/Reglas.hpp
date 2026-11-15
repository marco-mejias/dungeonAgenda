#pragma once
// Shared combat rules (CombatManager, DungeonManager for bubbles and reach). Without a weapon you
// hit with your fists: Strength - Defense, reach 1, 2 PA. With a weapon: weapon damage + its stat
// (strength/magic/luck) - Defense, with its own cost and reach.

#include "Scripts/CPP/Inventario/Inventario.hpp"
#include "Scripts/CPP/Mazmorra/Alturas.hpp"
#include <Godot/classes/engine.hpp>
#include <algorithm>
#include <cmath>

namespace reglas {

using namespace godot;

const int COSTE_SIN_ARMA = 2;

// Value of the stat the weapon attacks with (strength, magic or luck; strength with no weapon)
inline int StatAtaque(Node* atacante) {
	Ref<Resource> arma = inventario::Arma(atacante);
	String stat = arma.is_valid() ? String(arma->get("stat_ataque")) : String("fuerza");
	return atacante->get_meta(stat, 1);
}

// Damage before defense: attack stat + weapon damage + joke item bonuses
inline int DanoBase(Node* atacante, Node* defensor) {
	Ref<Resource> arma = inventario::Arma(atacante);
	int bono = arma.is_valid() ? (int)arma->get("dano") : 0;
	// Flip-flop of the Inquisition: +3 against whoever attacked you ("disobedient")
	if (inventario::Lleva(atacante, "chancleta_inquisicion") && defensor->has_meta("desobediente") && (bool)defensor->get_meta("desobediente")) bono += 3;
	// Multi-purpose sword: the distracted one hits for 1 less on its next attack
	if (atacante->has_meta("distraido") && (int)atacante->get_meta("distraido") > 0) bono -= 1;
	int dano = std::max(1, bono + StatAtaque(atacante) - (int)defensor->get_meta("defensa", 0));
	// "I'm not here" mask: if they attack you, you didn't see it coming -> double damage
	if (inventario::Lleva(defensor, "mascara_no_estoy")) dano *= 2;
	return dano;
}

// ==========================================
// ATTACK ROLL (d20, like D&D)
// ==========================================
// d20 + bonus >= defender's armour class -> hit.
//   - Natural 1: always misses (fumble).  - Natural 20 (or lower with critical weapons): critical.
//   - Bonus = attack stat * (1 + luck * level * 3%) / 2  -> luck weighs more with level.
//   - Armour class = 8 + defense.
// The test fixes the roll with Engine.set_meta("tirada_forzada", n).

struct Tirada {
	int d20 = 10;     // The die that counts
	int d20b = 0;     // The other die if there was advantage/disadvantage (0 = none)
	int ventaja = 0;  // 1 advantage, -1 disadvantage
	int bono = 0;
	int total = 10;
	int ca = 10;
	bool acierta = true;
	bool critico = false;
};

inline int Nivel(Node* n) { return n ? (int)n->get_meta("nivel", 1) : 1; }

// Attack roll bonus (see the formula above)
inline int BonoAtaque(Node* atacante) {
	double stat = StatAtaque(atacante);
	int suerte = atacante->get_meta("suerte", 1);
	return (int)std::floor(stat * (1.0 + suerte * Nivel(atacante) * 0.03) / 2.0);
}

inline int ClaseArmadura(Node* defensor) { return 8 + (int)defensor->get_meta("defensa", 0); }

// Height: +2 to the roll attacking from above someone below, -2 the other way round
inline int BonoAltura(Node* atacante, Node* defensor) {
	if (!atacante->has_meta("grid_pos") || !defensor->has_meta("grid_pos")) return 0;
	bool arriba_a = alturas::EsAlto(atacante->get_meta("grid_pos")), arriba_d = alturas::EsAlto(defensor->get_meta("grid_pos"));
	return arriba_a == arriba_d ? 0 : (arriba_a ? 2 : -2);
}

// Lowest d20 face that is a critical: 20, and one face less for every extra 5% critical of the
// weapon
inline int CaraCritico(Node* atacante) {
	Ref<Resource> arma = inventario::Arma(atacante);
	int extra = arma.is_valid() ? (int)arma->get("critico_extra") : 0;
	return std::max(15, 20 - extra / 5);
}

// One d20 (or the forced one the test sets)
inline int TirarD20() {
	Engine* e = Engine::get_singleton();
	if (e && e->has_meta("tirada_forzada")) return std::clamp((int)e->get_meta("tirada_forzada"), 1, 20);
	return 1 + (int)(UtilityFunctions::randi() % 20);
}

// Natural 1 always misses, the critical face always hits, otherwise d20 + bonus vs AC
inline bool AciertaCon(int d20, int bono, int ca, int cara_critico) {
	if (d20 == 1) return false;
	if (d20 >= cara_critico) return true;
	return d20 + bono >= ca;
}

// Positioning (Tactica.gd): advantage/disadvantage (flanking, height, cover, states) and bonuses
// (ally help, banner). Without the autoload, nothing.
inline Dictionary Modificadores(Node* atacante, Node* defensor) {
	Node* tac = atacante ? atacante->get_node_or_null("/root/Tactica") : nullptr;
	if (!tac || !defensor) return Dictionary();
	Variant v = tac->call("modificadores", atacante, defensor);
	return v.get_type() == Variant::DICTIONARY ? Dictionary(v) : Dictionary();
}

// The full attack roll: advantage, bonuses, critical face and whether it hits
inline Tirada TiradaAtaque(Node* atacante, Node* defensor) {
	Tirada t;
	Dictionary m = Modificadores(atacante, defensor);
	t.ventaja = (int)m.get("ventaja", 0);
	t.d20 = TirarD20();
	if (t.ventaja != 0) {
		// Advantage: two d20 and the best one; disadvantage: the worst one
		t.d20b = TirarD20();
		int mejor = std::max(t.d20, t.d20b), peor = std::min(t.d20, t.d20b);
		int otro = t.ventaja > 0 ? peor : mejor;
		t.d20 = t.ventaja > 0 ? mejor : peor;
		t.d20b = otro;
	}
	t.bono = BonoAtaque(atacante) + (int)m.get("bono", 0);
	// Egg-throwing chicken: whoever is blinded by an egg aims worse
	if (atacante->has_meta("cegado") && (int)atacante->get_meta("cegado") > 0) t.bono -= 3;
	t.total = t.d20 + t.bono;
	t.ca = ClaseArmadura(defensor);
	t.critico = t.d20 >= CaraCritico(atacante);
	t.acierta = AciertaCon(t.d20, t.bono, t.ca, CaraCritico(atacante));
	// Backstab (Tactica/Sigilo: from behind someone who hasn't seen you): guaranteed critical
	if ((bool)m.get("critico", false)) { t.critico = true; t.acierta = true; }
	return t;
}

// % of d20 faces that hit (for the preview card)
inline int ProbAcierto(Node* atacante, Node* defensor) {
	Dictionary m = Modificadores(atacante, defensor);
	if ((bool)m.get("critico", false)) return 100; // Backstab
	int ventaja = (int)m.get("ventaja", 0);
	int bono = BonoAtaque(atacante) + (int)m.get("bono", 0), ca = ClaseArmadura(defensor), cc = CaraCritico(atacante), caras = 0;
	if (atacante->has_meta("cegado") && (int)atacante->get_meta("cegado") > 0) bono -= 3;
	for (int d = 1; d <= 20; d++) if (AciertaCon(d, bono, ca, cc)) caras++;
	double p = caras / 20.0;
	if (ventaja > 0) p = 1.0 - (1.0 - p) * (1.0 - p); // Two dice, the best one
	else if (ventaja < 0) p = p * p;                  // Two dice, the worst one
	return (int)std::ceil(p * 100.0 - 1e-9);
}

inline int ProbCritico(Node* atacante) { return (21 - CaraCritico(atacante)) * 5; }

// Critical damage (the weapon's multiplier, x2 by default)
inline int DanoCritico(Node* atacante, int dano) {
	Ref<Resource> arma = inventario::Arma(atacante);
	double mult = arma.is_valid() ? (double)arma->get("mult_critico") : 2.0;
	return (int)std::round(dano * mult);
}

// PA it costs to attack with the equipped weapon
inline int CosteAtaque(Node* atacante) {
	Ref<Resource> arma = inventario::Arma(atacante);
	return arma.is_valid() ? (int)arma->get("coste_pa") : COSTE_SIN_ARMA;
}

// Closest tile the weapon can hit (a bow can't shoot point-blank)
inline int AlcanceMin(Node* atacante) {
	Ref<Resource> arma = inventario::Arma(atacante);
	return arma.is_valid() ? (int)arma->get("alcance_min") : 1;
}

// Furthest tile the weapon can hit
inline int AlcanceMax(Node* atacante) {
	Ref<Resource> arma = inventario::Arma(atacante);
	return arma.is_valid() ? (int)arma->get("alcance_max") : 1;
}

// Does the weapon need line of sight? (fists and most weapons do)
inline bool RequiereVision(Node* atacante) {
	Ref<Resource> arma = inventario::Arma(atacante);
	return !arma.is_valid() || (bool)arma->get("requiere_vision");
}

inline int Distancia(Vector2i a, Vector2i b) { return std::abs(a.x - b.x) + std::abs(a.y - b.y); }

// Does the weapon reach from 'desde' to 'hasta'? (line of sight is checked by whoever owns the map)
// From a high room against someone below, ranged weapons reach 1 tile further
inline bool EnAlcance(Node* atacante, Vector2i desde, Vector2i hasta) {
	int d = Distancia(desde, hasta);
	int maximo = AlcanceMax(atacante);
	if (maximo > 1 && alturas::EsAlto(desde) && !alturas::EsAlto(hasta)) maximo += 1;
	return d >= AlcanceMin(atacante) && d <= maximo;
}

// Extra effects on hit: the weapon's and its enchantments'
inline Array EfectosAlGolpear(Node* atacante) {
	Array out;
	Ref<Resource> arma = inventario::Arma(atacante);
	if (arma.is_null()) return out;
	out.append_array(arma->get("efectos_al_golpear"));
	Array encantamientos = arma->get("encantamientos");
	for (int i = 0; i < encantamientos.size(); i++) {
		Ref<Resource> enc = encantamientos[i];
		if (enc.is_valid()) out.append_array(enc->get("efectos_al_golpear"));
	}
	return out;
}

} // namespace reglas
