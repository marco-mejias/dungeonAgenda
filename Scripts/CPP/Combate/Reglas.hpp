#pragma once
// Reglas de combate compartidas (CombatManager, DungeonManager para bocadillos y alcance).
// Sin arma se pega con los puños: Fuerza - Defensa, alcance 1, 2 PA.
// Con arma: daño del arma + su stat (fuerza/magia/suerte) - Defensa, con su coste y alcance.

#include "Scripts/CPP/Inventario/Inventario.hpp"

namespace reglas {

using namespace godot;

const int COSTE_SIN_ARMA = 2;

inline int StatAtaque(Node* atacante) {
	Ref<Resource> arma = inventario::Arma(atacante);
	String stat = arma.is_valid() ? String(arma->get("stat_ataque")) : String("fuerza");
	return atacante->get_meta(stat, 1);
}

inline int DanoBase(Node* atacante, Node* defensor) {
	Ref<Resource> arma = inventario::Arma(atacante);
	int bono = arma.is_valid() ? (int)arma->get("dano") : 0;
	return std::max(1, bono + StatAtaque(atacante) - (int)defensor->get_meta("defensa", 0));
}

inline int ProbCritico(Node* atacante) {
	Ref<Resource> arma = inventario::Arma(atacante);
	int extra = arma.is_valid() ? (int)arma->get("critico_extra") : 0;
	return std::min(100, (int)atacante->get_meta("suerte", 1) * 2 + extra);
}

inline int DanoCritico(Node* atacante, int dano) {
	Ref<Resource> arma = inventario::Arma(atacante);
	double mult = arma.is_valid() ? (double)arma->get("mult_critico") : 2.0;
	return (int)std::round(dano * mult);
}

inline int CosteAtaque(Node* atacante) {
	Ref<Resource> arma = inventario::Arma(atacante);
	return arma.is_valid() ? (int)arma->get("coste_pa") : COSTE_SIN_ARMA;
}

inline int AlcanceMin(Node* atacante) {
	Ref<Resource> arma = inventario::Arma(atacante);
	return arma.is_valid() ? (int)arma->get("alcance_min") : 1;
}

inline int AlcanceMax(Node* atacante) {
	Ref<Resource> arma = inventario::Arma(atacante);
	return arma.is_valid() ? (int)arma->get("alcance_max") : 1;
}

inline bool RequiereVision(Node* atacante) {
	Ref<Resource> arma = inventario::Arma(atacante);
	return !arma.is_valid() || (bool)arma->get("requiere_vision");
}

inline int Distancia(Vector2i a, Vector2i b) { return std::abs(a.x - b.x) + std::abs(a.y - b.y); }

// ¿Llega el arma desde 'desde' hasta 'hasta'? (la línea de visión la comprueba quien tenga el mapa)
inline bool EnAlcance(Node* atacante, Vector2i desde, Vector2i hasta) {
	int d = Distancia(desde, hasta);
	return d >= AlcanceMin(atacante) && d <= AlcanceMax(atacante);
}

// Efectos extra al golpear: los del arma y los de sus encantamientos
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
