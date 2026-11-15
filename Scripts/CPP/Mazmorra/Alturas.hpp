#pragma once
// Alturas del piso: cada casilla tiene un nivel (0 = suelo de abajo, 1 = escalón, 2 = sala alta).
// Mazmorra las publica al generar el piso en un meta del Engine ("alturas"), para que cualquier
// script (render, movimiento, IA, ratón, combate) sepa a qué altura está una casilla sin tener
// acceso a la Mazmorra.

#include <Godot/godot.hpp>
#include <Godot/classes/engine.hpp>
#include <Godot/variant/packed_byte_array.hpp>

namespace alturas {

using namespace godot;

const float PASO = 0.2f; // metros por nivel: escalón 0.2, sala alta 0.4
const int ESCALON = 1;
const int ALTO = 2;

inline void Publicar(const PackedByteArray& niveles, int lado) {
	Engine* e = Engine::get_singleton();
	if (!e) return;
	e->set_meta("alturas", niveles);
	e->set_meta("alturas_lado", lado);
}

inline int Nivel(int x, int y) {
	Engine* e = Engine::get_singleton();
	if (!e || !e->has_meta("alturas")) return 0;
	int lado = e->get_meta("alturas_lado", 0);
	if (x < 0 || y < 0 || x >= lado || y >= lado) return 0;
	PackedByteArray a = e->get_meta("alturas");
	int i = x * lado + y;
	return i < a.size() ? (int)a[i] : 0;
}

inline int Nivel(Vector2i c) { return Nivel(c.x, c.y); }

// Altura en el mundo del suelo de una casilla
inline float Y(int x, int y) { return Nivel(x, y) * PASO; }

inline bool EsAlto(Vector2i c) { return Nivel(c) == ALTO; }

} // namespace alturas
