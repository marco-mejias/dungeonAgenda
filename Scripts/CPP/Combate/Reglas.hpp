#pragma once
// Reglas de combate compartidas (CombatManager, DungeonManager para bocadillos y alcance).
// Sin arma se pega con los puños: Fuerza - Defensa, alcance 1, 2 PA.
// Con arma: daño del arma + su stat (fuerza/magia/suerte) - Defensa, con su coste y alcance.

#include "Scripts/CPP/Inventario/Inventario.hpp"
#include "Scripts/CPP/Mazmorra/Alturas.hpp"
#include <Godot/classes/engine.hpp>
#include <algorithm>
#include <cmath>

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
	// Chancleta de la Inquisición: +3 contra quien te haya atacado ("desobediente")
	if (inventario::Lleva(atacante, "chancleta_inquisicion") && defensor->has_meta("desobediente") && (bool)defensor->get_meta("desobediente")) bono += 3;
	// Espada multiusos: el distraído pega 1 menos en su próximo ataque
	if (atacante->has_meta("distraido") && (int)atacante->get_meta("distraido") > 0) bono -= 1;
	int dano = std::max(1, bono + StatAtaque(atacante) - (int)defensor->get_meta("defensa", 0));
	// Máscara de "No estoy aquí": si te atacan, no te lo esperabas -> daño doble
	if (inventario::Lleva(defensor, "mascara_no_estoy")) dano *= 2;
	return dano;
}

// ==========================================
// TIRADA DE ATAQUE (d20, como en D&D)
// ==========================================
// d20 + bono >= clase de armadura del defensor -> acierta.
//   - 1 natural: siempre falla (pifia).  - 20 natural (o menos con armas de crítico): crítico.
//   - Bono = stat del ataque * (1 + suerte * nivel * 3%) / 2  -> la suerte pesa más con el nivel.
//   - Clase de armadura = 8 + defensa.
// El test fija la tirada con Engine.set_meta("tirada_forzada", n).

struct Tirada {
	int d20 = 10;     // el dado que cuenta
	int d20b = 0;     // el otro dado si hubo ventaja/desventaja (0 = no)
	int ventaja = 0;  // 1 ventaja, -1 desventaja
	int bono = 0;
	int total = 10;
	int ca = 10;
	bool acierta = true;
	bool critico = false;
};

inline int Nivel(Node* n) { return n ? (int)n->get_meta("nivel", 1) : 1; }

inline int BonoAtaque(Node* atacante) {
	double stat = StatAtaque(atacante);
	int suerte = atacante->get_meta("suerte", 1);
	return (int)std::floor(stat * (1.0 + suerte * Nivel(atacante) * 0.03) / 2.0);
}

inline int ClaseArmadura(Node* defensor) { return 8 + (int)defensor->get_meta("defensa", 0); }

// Altura: +2 a la tirada atacando desde arriba a alguien de abajo, -2 al revés
inline int BonoAltura(Node* atacante, Node* defensor) {
	if (!atacante->has_meta("grid_pos") || !defensor->has_meta("grid_pos")) return 0;
	bool arriba_a = alturas::EsAlto(atacante->get_meta("grid_pos")), arriba_d = alturas::EsAlto(defensor->get_meta("grid_pos"));
	return arriba_a == arriba_d ? 0 : (arriba_a ? 2 : -2);
}

// Cara mínima del d20 que es crítico: 20, y una cara menos por cada 5% de crítico extra del arma
inline int CaraCritico(Node* atacante) {
	Ref<Resource> arma = inventario::Arma(atacante);
	int extra = arma.is_valid() ? (int)arma->get("critico_extra") : 0;
	return std::max(15, 20 - extra / 5);
}

inline int TirarD20() {
	Engine* e = Engine::get_singleton();
	if (e && e->has_meta("tirada_forzada")) return std::clamp((int)e->get_meta("tirada_forzada"), 1, 20);
	return 1 + (int)(UtilityFunctions::randi() % 20);
}

inline bool AciertaCon(int d20, int bono, int ca, int cara_critico) {
	if (d20 == 1) return false;
	if (d20 >= cara_critico) return true;
	return d20 + bono >= ca;
}

// Posicionamiento (Tactica.gd): ventaja/desventaja (flanqueo, altura, cobertura, estados) y bonos
// (ayuda de un aliado, estandarte). Si no está el autoload, nada.
inline Dictionary Modificadores(Node* atacante, Node* defensor) {
	Node* tac = atacante ? atacante->get_node_or_null("/root/Tactica") : nullptr;
	if (!tac || !defensor) return Dictionary();
	Variant v = tac->call("modificadores", atacante, defensor);
	return v.get_type() == Variant::DICTIONARY ? Dictionary(v) : Dictionary();
}

inline Tirada TiradaAtaque(Node* atacante, Node* defensor) {
	Tirada t;
	Dictionary m = Modificadores(atacante, defensor);
	t.ventaja = (int)m.get("ventaja", 0);
	t.d20 = TirarD20();
	if (t.ventaja != 0) {
		// Ventaja: dos d20 y el mejor; desventaja: el peor
		t.d20b = TirarD20();
		int mejor = std::max(t.d20, t.d20b), peor = std::min(t.d20, t.d20b);
		int otro = t.ventaja > 0 ? peor : mejor;
		t.d20 = t.ventaja > 0 ? mejor : peor;
		t.d20b = otro;
	}
	t.bono = BonoAtaque(atacante) + (int)m.get("bono", 0);
	// Gallina lanza-huevos: el cegado por un huevo apunta peor
	if (atacante->has_meta("cegado") && (int)atacante->get_meta("cegado") > 0) t.bono -= 3;
	t.total = t.d20 + t.bono;
	t.ca = ClaseArmadura(defensor);
	t.critico = t.d20 >= CaraCritico(atacante);
	t.acierta = AciertaCon(t.d20, t.bono, t.ca, CaraCritico(atacante));
	// Puñalada (Tactica/Sigilo: por la espalda de quien no os ha visto): crítico seguro
	if ((bool)m.get("critico", false)) { t.critico = true; t.acierta = true; }
	return t;
}

// % de caras del d20 que aciertan (para la tarjeta de previsión)
inline int ProbAcierto(Node* atacante, Node* defensor) {
	Dictionary m = Modificadores(atacante, defensor);
	if ((bool)m.get("critico", false)) return 100; // puñalada
	int ventaja = (int)m.get("ventaja", 0);
	int bono = BonoAtaque(atacante) + (int)m.get("bono", 0), ca = ClaseArmadura(defensor), cc = CaraCritico(atacante), caras = 0;
	if (atacante->has_meta("cegado") && (int)atacante->get_meta("cegado") > 0) bono -= 3;
	for (int d = 1; d <= 20; d++) if (AciertaCon(d, bono, ca, cc)) caras++;
	double p = caras / 20.0;
	if (ventaja > 0) p = 1.0 - (1.0 - p) * (1.0 - p); // dos dados, el mejor
	else if (ventaja < 0) p = p * p;                  // dos dados, el peor
	return (int)std::ceil(p * 100.0 - 1e-9);
}

inline int ProbCritico(Node* atacante) { return (21 - CaraCritico(atacante)) * 5; }

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
// Desde una sala alta contra alguien de abajo, las armas a distancia llegan 1 casilla más
inline bool EnAlcance(Node* atacante, Vector2i desde, Vector2i hasta) {
	int d = Distancia(desde, hasta);
	int maximo = AlcanceMax(atacante);
	if (maximo > 1 && alturas::EsAlto(desde) && !alturas::EsAlto(hasta)) maximo += 1;
	return d >= AlcanceMin(atacante) && d <= maximo;
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
