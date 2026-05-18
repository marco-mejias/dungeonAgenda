#include <Godot/godot.hpp>
#include <Godot/classes/node.hpp>
#include <Godot/variant/utility_functions.hpp>

using namespace godot;
using namespace jenova::sdk;

JENOVA_SCRIPT_BEGIN

void OnReady(Caller* instance) {
	Node* self = GetSelf<Node>(instance);
	if (!self) return;

	// Inicializamos las stats en los metadatos únicos de este nodo
	// Toma el nombre que le pongas al nodo en Godot por defecto
	self->set_meta("char_name", self->get_name()); 
	self->set_meta("hp", 100);
	self->set_meta("max_hp", 100);
	
	// Tus 5 stats principales
	self->set_meta("fuerza", 5);
	self->set_meta("resistencia", 5);
	self->set_meta("magia", 5);
	self->set_meta("suerte", 5);
	self->set_meta("velocidad", 4); // La velocidad dictará el alcance en el tablero

	// Estado del turno
	self->set_meta("grid_pos", Vector2i(0, 0));
	self->set_meta("ha_movido", false);
}

// --- GETTERS Y SETTERS GENÉRICOS ---

Variant GetStat(Caller* instance, String stat_name) {
	Node* self = GetSelf<Node>(instance);
	return self->get_meta(stat_name);
}

void SetStat(Caller* instance, String stat_name, Variant value) {
	Node* self = GetSelf<Node>(instance);
	self->set_meta(stat_name, value);
}

// Funciones específicas para el tablero
void SetGridPos(Caller* instance, int x, int y) {
	Node* self = GetSelf<Node>(instance);
	self->set_meta("grid_pos", Vector2i(x, y));
}

Vector2i GetGridPos(Caller* instance) {
	Node* self = GetSelf<Node>(instance);
	return self->get_meta("grid_pos");
}

bool HasMoved(Caller* instance) {
	Node* self = GetSelf<Node>(instance);
	return self->get_meta("ha_movido");
}

void SetHasMoved(Caller* instance, bool state) {
	Node* self = GetSelf<Node>(instance);
	self->set_meta("ha_movido", state);
}

JENOVA_SCRIPT_END
