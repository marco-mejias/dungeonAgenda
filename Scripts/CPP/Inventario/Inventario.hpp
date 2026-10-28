#pragma once
// Inventario y equipo de los héroes, sus stats finales y los efectos de los objetos.
//
// Los objetos se definen en Data/Objetos (.tres, clases en Scripts/GDD/Datos) y se piden
// por id al autoload Catalogo. Cada héroe guarda:
//   meta "equipo"          Dictionary ranura -> id   (lo equipado NO pesa)
//   meta "inventario"      Array de {id, cantidad, usos}
//   meta "stats_base"      Dictionary stat -> valor  (de su PersonajeData)
//   meta "mods_temporales" Array de {stat, valor, turnos}  (turnos < 0 = hasta el final del piso)
// Las metas de siempre ("fuerza", "defensa", "hp_max"...) guardan el valor FINAL
// (base + equipo + temporales), así que el resto del juego las sigue leyendo igual.

#include <Godot/godot.hpp>
#include <Godot/classes/node.hpp>
#include <Godot/classes/resource.hpp>
#include <Godot/classes/texture2d.hpp>
#include <Godot/variant/utility_functions.hpp>
#include <algorithm>
#include <cmath>

namespace inventario {

using namespace godot;

const float CAPACIDAD_SIN_MOCHILA = 3.0f; // lo que cabe en los bolsillos
const int NUM_RANURAS = 8;
const char* const RANURAS[NUM_RANURAS] = { "mano_principal", "mano_secundaria", "cabeza", "cuerpo", "accesorio_1", "accesorio_2", "luz", "mochila" };
const int NUM_STATS = 9;
const char* const STATS[NUM_STATS] = { "hp_max", "velocidad", "fuerza", "defensa", "magia", "suerte", "radio_luz", "sanidad_max", "hambre_max" };

// Mismo orden que los enums de efecto.gd y entrada_botin.gd
enum TipoEfecto { E_CURAR, E_DANAR, E_CORDURA, E_HAMBRE, E_PA, E_ORO, E_COMIDA, E_MODIFICAR_STAT, E_DAR_OBJETO };
enum RecursoBotin { B_OBJETO, B_ORO, B_COMIDA };

// ==========================================
// CATÁLOGO Y DEFINICIONES
// ==========================================

inline Node* Catalogo(Node* ctx) { return ctx ? ctx->get_node_or_null("/root/Catalogo") : nullptr; }

inline Ref<Resource> Definicion(Node* ctx, const String& id) {
	Node* cat = Catalogo(ctx);
	if (!cat || id.is_empty()) return Ref<Resource>();
	return cat->call("obtener", id);
}

inline String IdDe(const Ref<Resource>& def) { return def.is_valid() ? String(def->call("obtener_id")) : String(); }
inline String NombreDe(const Ref<Resource>& def) { return def.is_valid() ? String(def->call("nombre_completo")) : String("?"); }
inline float PesoDe(const Ref<Resource>& def) { return def.is_valid() ? (float)(double)def->get("peso") : 0.0f; }
inline bool EsEquipo(const Ref<Resource>& def) { return def.is_valid() && def->get("ranura").get_type() != Variant::NIL; }
inline bool EsArma(const Ref<Resource>& def) { return def.is_valid() && def->get("alcance_max").get_type() != Variant::NIL; }
inline bool EsMochila(const Ref<Resource>& def) { return def.is_valid() && def->get("capacidad").get_type() != Variant::NIL; }

inline Dictionary Equipo(Node* h) { return h ? Dictionary(h->get_meta("equipo", Dictionary())) : Dictionary(); }
inline Array Inventario(Node* h) { return h ? Array(h->get_meta("inventario", Array())) : Array(); }

inline Ref<Resource> Equipado(Node* h, const String& ranura) {
	Dictionary eq = Equipo(h);
	return eq.has(ranura) ? Definicion(h, eq[ranura]) : Ref<Resource>();
}

// El arma de la mano principal (los enemigos no tienen equipo: devuelve nulo)
inline Ref<Resource> Arma(Node* h) {
	Ref<Resource> a = Equipado(h, "mano_principal");
	return EsArma(a) ? a : Ref<Resource>();
}

// ==========================================
// PESO
// ==========================================

inline float PesoInventario(Node* h) {
	Array inv = Inventario(h);
	float total = 0.0f;
	for (int i = 0; i < inv.size(); i++) {
		Dictionary e = inv[i];
		total += PesoDe(Definicion(h, e["id"])) * (int)e["cantidad"];
	}
	return total;
}

inline float Capacidad(Node* h) {
	Ref<Resource> m = Equipado(h, "mochila");
	return EsMochila(m) ? (float)(double)m->get("capacidad") : CAPACIDAD_SIN_MOCHILA;
}

// ==========================================
// STATS FINALES
// ==========================================

inline void RecalcularStats(Node* h) {
	if (!h) return;
	Dictionary base = h->get_meta("stats_base", Dictionary());
	if (base.is_empty()) return;

	Dictionary plano, porcentaje;
	auto sumar = [&](const Ref<Resource>& m) {
		if (m.is_null()) return;
		String s = m->get("stat");
		plano[s] = (int)plano.get(s, 0) + (int)m->get("plano");
		porcentaje[s] = (int)porcentaje.get(s, 0) + (int)m->get("porcentaje");
	};

	Dictionary eq = Equipo(h);
	Array ranuras = eq.keys();
	for (int i = 0; i < ranuras.size(); i++) {
		Ref<Resource> d = Definicion(h, eq[ranuras[i]]);
		if (!EsEquipo(d)) continue;
		Array mods = d->get("modificadores");
		for (int j = 0; j < mods.size(); j++) sumar(mods[j]);
		Array encantamientos = d->get("encantamientos");
		for (int j = 0; j < encantamientos.size(); j++) {
			Ref<Resource> enc = encantamientos[j];
			if (enc.is_null()) continue;
			Array mods_enc = enc->get("modificadores");
			for (int k = 0; k < mods_enc.size(); k++) sumar(mods_enc[k]);
		}
	}

	Array temporales = h->get_meta("mods_temporales", Array());
	for (int i = 0; i < temporales.size(); i++) {
		Dictionary m = temporales[i];
		String s = m["stat"];
		plano[s] = (int)plano.get(s, 0) + (int)m["valor"];
	}

	for (int i = 0; i < NUM_STATS; i++) {
		String s = STATS[i];
		if (!base.has(s)) continue;
		int v = (int)base[s] + (int)plano.get(s, 0);
		v = (int)std::round(v * (1.0 + (int)porcentaje.get(s, 0) / 100.0));
		bool minimo_uno = (s == "hp_max" || s == "velocidad" || s == "sanidad_max" || s == "hambre_max");
		h->set_meta(s, std::max(minimo_uno ? 1 : 0, v));
	}

	// Derivados: los PA por día salen de la velocidad; lo actual no puede pasar del máximo
	h->set_meta("pa_max", h->get_meta("velocidad"));
	h->set_meta("hp_actual", std::min((int)h->get_meta("hp_actual", 0), (int)h->get_meta("hp_max")));
	h->set_meta("sanidad_actual", std::min((int)h->get_meta("sanidad_actual", 0), (int)h->get_meta("sanidad_max")));
	h->set_meta("hambre_actual", std::min((int)h->get_meta("hambre_actual", 0), (int)h->get_meta("hambre_max")));
}

// Un día menos para los efectos temporales; devuelve true si alguno se ha acabado
inline bool AvanzarDia(Node* h) {
	Array temporales = h->get_meta("mods_temporales", Array());
	Array quedan;
	for (int i = 0; i < temporales.size(); i++) {
		Dictionary m = temporales[i];
		int turnos = m["turnos"];
		if (turnos < 0) { quedan.push_back(m); continue; }
		if (turnos - 1 > 0) { m["turnos"] = turnos - 1; quedan.push_back(m); }
	}
	bool cambio = quedan.size() != temporales.size();
	h->set_meta("mods_temporales", quedan);
	if (cambio) RecalcularStats(h);
	return cambio;
}

// Al cambiar de piso se acaban los efectos "hasta el final del piso"
inline void FinDePiso(Node* h) {
	Array temporales = h->get_meta("mods_temporales", Array());
	Array quedan;
	for (int i = 0; i < temporales.size(); i++) {
		Dictionary m = temporales[i];
		if ((int)m["turnos"] >= 0) quedan.push_back(m);
	}
	h->set_meta("mods_temporales", quedan);
	RecalcularStats(h);
}

// ==========================================
// INVENTARIO
// ==========================================

// Mete hasta 'cantidad' unidades (apilando si se puede). Sin 'forzar' respeta el peso.
// Devuelve cuántas han entrado.
inline int AnadirInterno(Node* h, const String& id, int cantidad, bool forzar) {
	Ref<Resource> d = Definicion(h, id);
	if (!h || d.is_null() || cantidad <= 0) return 0;

	float peso = PesoDe(d);
	int caben = cantidad;
	if (!forzar && peso > 0.0001f) {
		float libre = Capacidad(h) - PesoInventario(h);
		caben = std::min(cantidad, (int)std::floor((libre + 0.001f) / peso));
	}
	if (caben <= 0) return 0;

	bool apilable = d->get("apilable");
	int max_pila = std::max(1, (int)d->get("max_pila"));
	int usos = d->get("usos");
	Array inv = Inventario(h);
	int restantes = caben;
	if (apilable) {
		for (int i = 0; i < inv.size() && restantes > 0; i++) {
			Dictionary e = inv[i];
			if (String(e["id"]) != id) continue;
			int n = std::min(max_pila - (int)e["cantidad"], restantes);
			if (n <= 0) continue;
			e["cantidad"] = (int)e["cantidad"] + n;
			restantes -= n;
		}
	}
	while (restantes > 0) {
		int n = apilable ? std::min(max_pila, restantes) : 1;
		Dictionary e;
		e["id"] = id;
		e["cantidad"] = n;
		e["usos"] = usos;
		inv.push_back(e);
		restantes -= n;
	}
	h->set_meta("inventario", inv);
	return caben;
}

inline int Anadir(Node* h, const String& id, int cantidad) { return AnadirInterno(h, id, cantidad, false); }

inline bool Quitar(Node* h, int indice, int cantidad) {
	Array inv = Inventario(h);
	if (indice < 0 || indice >= inv.size() || cantidad <= 0) return false;
	Dictionary e = inv[indice];
	int queda = (int)e["cantidad"] - cantidad;
	if (queda < 0) return false;
	if (queda == 0) inv.remove_at(indice);
	else e["cantidad"] = queda;
	h->set_meta("inventario", inv);
	return true;
}

// ==========================================
// EQUIPO
// ==========================================

inline bool PuedeLlevar(Node* h, const Ref<Resource>& def) {
	PackedStringArray clases = def->get("clases_permitidas");
	return clases.is_empty() || clases.has(String(h->get_meta("clase", "")));
}

inline String RanuraPara(Node* h, const Ref<Resource>& def) {
	switch ((int)def->get("ranura")) {
		case 0: return "mano_principal";
		case 1: return "mano_secundaria";
		case 2: return "cabeza";
		case 3: return "cuerpo";
		case 4: {
			Dictionary eq = Equipo(h);
			if (!eq.has("accesorio_1")) return "accesorio_1";
			if (!eq.has("accesorio_2")) return "accesorio_2";
			return "accesorio_1";
		}
		case 5: return "luz";
		default: return "mochila";
	}
}

inline bool DosManos(const Ref<Resource>& def) { return EsArma(def) && (bool)def->get("dos_manos"); }

// Equipa el objeto 'indice' del inventario. Lo que estuviera en la ranura vuelve al inventario
// (y un arma a dos manos desplaza también la mano secundaria). Devuelve "" o el motivo del fallo.
inline String Equipar(Node* h, int indice) {
	Array inv = Inventario(h);
	if (indice < 0 || indice >= inv.size()) return "No existe ese objeto";
	Dictionary entrada = inv[indice];
	Ref<Resource> def = Definicion(h, entrada["id"]);
	if (!EsEquipo(def)) return "No se puede equipar";
	if (!PuedeLlevar(h, def)) return "Tu clase no puede llevarlo";

	String ranura = RanuraPara(h, def);
	Dictionary eq = Equipo(h);
	PackedStringArray salen;
	if (eq.has(ranura)) salen.push_back(ranura);
	if (ranura == "mano_principal" && DosManos(def) && eq.has("mano_secundaria")) salen.push_back("mano_secundaria");
	if (ranura == "mano_secundaria" && DosManos(Equipado(h, "mano_principal"))) salen.push_back("mano_principal");

	// Simulamos el peso: sale una unidad del objeto y entra lo que se quita
	float peso = PesoInventario(h) - PesoDe(def);
	for (int i = 0; i < salen.size(); i++) peso += PesoDe(Definicion(h, eq[salen[i]]));
	float capacidad = ranura == "mochila" ? (float)(double)def->get("capacidad") : Capacidad(h);
	if (peso > capacidad + 0.001f) return "No cabe en la mochila";

	Quitar(h, indice, 1);
	PackedStringArray ids_salen;
	for (int i = 0; i < salen.size(); i++) { ids_salen.push_back(eq[salen[i]]); eq.erase(salen[i]); }
	eq[ranura] = IdDe(def);
	h->set_meta("equipo", eq);
	for (int i = 0; i < ids_salen.size(); i++) AnadirInterno(h, ids_salen[i], 1, true);
	RecalcularStats(h);
	return "";
}

inline String Desequipar(Node* h, const String& ranura) {
	Dictionary eq = Equipo(h);
	if (!eq.has(ranura)) return "No hay nada en esa ranura";
	Ref<Resource> def = Definicion(h, eq[ranura]);
	float capacidad = ranura == "mochila" ? CAPACIDAD_SIN_MOCHILA : Capacidad(h);
	if (PesoInventario(h) + PesoDe(def) > capacidad + 0.001f) return "No cabe en la mochila";

	String id = eq[ranura];
	eq.erase(ranura);
	h->set_meta("equipo", eq);
	AnadirInterno(h, id, 1, true);
	RecalcularStats(h);
	return "";
}

// ==========================================
// INICIALIZACIÓN DE UN HÉROE (desde su PersonajeData)
// ==========================================

inline void Inicializar(Node* h, const Ref<Resource>& p) {
	Dictionary base;
	base["hp_max"] = p->get("vida_max");
	base["velocidad"] = p->get("velocidad");
	base["fuerza"] = p->get("fuerza");
	base["defensa"] = p->get("defensa");
	base["magia"] = p->get("magia");
	base["suerte"] = p->get("suerte");
	Variant radio = p->get("radio_luz");
	base["radio_luz"] = radio.get_type() == Variant::NIL ? Variant(4) : radio;
	base["sanidad_max"] = 100;
	base["hambre_max"] = 10;
	h->set_meta("stats_base", base);

	Variant clase = p->get("clase");
	h->set_meta("clase", clase.get_type() == Variant::NIL ? Variant(String()) : clase);
	h->set_meta("equipo", Dictionary());
	h->set_meta("inventario", Array());
	h->set_meta("mods_temporales", Array());

	Array equipo_inicial = p->get("equipo_inicial");
	for (int i = 0; i < equipo_inicial.size(); i++) {
		Ref<Resource> d = equipo_inicial[i];
		if (!EsEquipo(d)) continue;
		Dictionary eq = Equipo(h);
		eq[RanuraPara(h, d)] = IdDe(d);
		h->set_meta("equipo", eq);
	}
	RecalcularStats(h);

	Array inventario_inicial = p->get("inventario_inicial");
	for (int i = 0; i < inventario_inicial.size(); i++) Anadir(h, IdDe(inventario_inicial[i]), 1);
}

// ==========================================
// EFECTOS (consumibles, armas al golpear, eventos)
// ==========================================

inline bool EsHeroe(Node* n) { return n && n->get_parent() && n->get_parent()->get_name() == StringName("PartyManager"); }

inline void SumarMeta(Node* n, const char* meta, int delta, int minimo, int maximo) {
	int v = (int)n->get_meta(meta, 0) + delta;
	n->set_meta(meta, std::clamp(v, minimo, maximo));
}

// Aplica un Efecto al objetivo. Devuelve un texto corto para mostrar ("+25 PV"), o "" si no pasó nada.
inline String AplicarEfecto(const Ref<Resource>& ef, Node* objetivo) {
	if (ef.is_null() || !objetivo) return "";
	int prob = ef->get("probabilidad");
	if (prob < 100 && (int)(UtilityFunctions::randi() % 100) >= prob) return "";
	int valor = ef->get("valor");
	String num = (valor >= 0 ? String("+") : String("")) + String::num_int64(valor);

	switch ((int)ef->get("tipo")) {
		case E_CURAR:
			SumarMeta(objetivo, "hp_actual", valor, 0, objetivo->get_meta("hp_max", 1));
			return num + " PV";
		case E_DANAR: {
			Node* pm = objetivo->get_node_or_null("/root/Main/PartyManager");
			if (EsHeroe(objetivo) && pm) pm->call("RecibirDano", valor, objetivo);
			else objetivo->set_meta("hp_actual", (int)objetivo->get_meta("hp_actual", 0) - valor);
			return "-" + String::num_int64(valor);
		}
		case E_CORDURA:
			SumarMeta(objetivo, "sanidad_actual", valor, 0, objetivo->get_meta("sanidad_max", 100));
			return num + " cordura";
		case E_HAMBRE:
			SumarMeta(objetivo, "hambre_actual", valor, 0, objetivo->get_meta("hambre_max", 10));
			return num + " saciedad";
		case E_PA:
			SumarMeta(objetivo, "pa_actual", valor, 0, 99);
			return num + " PA";
		case E_ORO:
			SumarMeta(objetivo, "oro", valor, 0, 999999);
			return num + " oro";
		case E_COMIDA:
			SumarMeta(objetivo, "comida_bolsa", valor, 0, 999);
			return num + " comida";
		case E_MODIFICAR_STAT: {
			Array temporales = objetivo->get_meta("mods_temporales", Array());
			Dictionary m;
			m["stat"] = ef->get("stat");
			m["valor"] = valor;
			int duracion = ef->get("duracion_turnos");
			m["turnos"] = duracion > 0 ? duracion : -1;
			temporales.push_back(m);
			objetivo->set_meta("mods_temporales", temporales);
			RecalcularStats(objetivo);
			return String(ef->get("stat")) + " " + num;
		}
		case E_DAR_OBJETO: {
			Ref<Resource> o = ef->get("objeto");
			int n = Anadir(objetivo, IdDe(o), std::max(1, valor));
			return n > 0 ? "+" + String::num_int64(n) + " " + NombreDe(o) : String("Mochila llena");
		}
	}
	return "";
}

inline String AplicarEfectos(const Array& efectos, Node* objetivo) {
	String texto;
	for (int i = 0; i < efectos.size(); i++) {
		String t = AplicarEfecto(efectos[i], objetivo);
		if (t.is_empty()) continue;
		texto += (texto.is_empty() ? String("") : String(", ")) + t;
	}
	return texto;
}

// Usa el objeto 'indice' del inventario: aplica sus efectos al héroe y gasta un uso
// (al acabarse los usos se va una unidad). Devuelve "" o el motivo del fallo.
inline String Usar(Node* h, int indice, String& resultado) {
	Array inv = Inventario(h);
	if (indice < 0 || indice >= inv.size()) return "No existe ese objeto";
	Dictionary e = inv[indice];
	Ref<Resource> def = Definicion(h, e["id"]);
	if (def.is_null()) return "Objeto desconocido";
	Array efectos = def->get("efectos_al_usar");
	if (efectos.is_empty()) return "No se puede usar";

	resultado = AplicarEfectos(efectos, h);
	int usos_max = def->get("usos");
	if (usos_max > 0) {
		int usos = (int)e.get("usos", usos_max) - 1;
		if (usos <= 0) {
			Quitar(h, indice, 1);
			inv = Inventario(h);
			// Si quedan más unidades en la pila, la siguiente empieza con todos sus usos
			if (indice < inv.size()) {
				Dictionary siguiente = inv[indice];
				if (String(siguiente["id"]) == IdDe(def)) siguiente["usos"] = usos_max;
				h->set_meta("inventario", inv);
			}
		} else {
			e["usos"] = usos;
			h->set_meta("inventario", inv);
		}
	}
	return "";
}

// ==========================================
// BOTÍN
// ==========================================

// Entrega al héroe lo que salió de una tabla de botín (oro, comida, objetos), con el icono
// volando a su ficha. Devuelve lo que no ha cabido (mismo formato que la entrada).
inline Array EntregarBotin(Node* h, const Array& botin, Vector3 pos_mundo) {
	Array restos;
	if (!h) return botin;
	Node* feel = h->get_node_or_null("/root/Feel");
	String nombre = h->get_name();
	for (int i = 0; i < botin.size(); i++) {
		Dictionary b = botin[i];
		int cantidad = b["cantidad"];
		switch ((int)b["recurso"]) {
			case B_ORO:
				SumarMeta(h, "oro", cantidad, 0, 999999);
				if (feel) feel->call("volar_a_ficha", pos_mundo, nombre, Rect2(96, 128, 16, 16), "+" + String::num_int64(cantidad));
				break;
			case B_COMIDA:
				SumarMeta(h, "comida_bolsa", cantidad, 0, 999);
				if (feel) feel->call("volar_a_ficha", pos_mundo, nombre, Rect2(96, 112, 16, 16), "+" + String::num_int64(cantidad));
				break;
			default: {
				String id = b["id"];
				int n = Anadir(h, id, cantidad);
				Ref<Resource> def = Definicion(h, id);
				if (n > 0 && feel && def.is_valid())
					feel->call("volar_textura_a_ficha", pos_mundo, nombre, def->get("icono"), n > 1 ? NombreDe(def) + " x" + String::num_int64(n) : NombreDe(def));
				if (n < cantidad) {
					Dictionary resto = b.duplicate();
					resto["cantidad"] = cantidad - n;
					restos.push_back(resto);
				}
			}
		}
	}
	return restos;
}

// Para la interfaz y los tests: el inventario con nombres, iconos y pesos
inline Array DescribirInventario(Node* h) {
	Array out;
	Array inv = Inventario(h);
	for (int i = 0; i < inv.size(); i++) {
		Dictionary e = inv[i];
		Ref<Resource> def = Definicion(h, e["id"]);
		Dictionary d;
		d["indice"] = i;
		d["id"] = e["id"];
		d["cantidad"] = e["cantidad"];
		d["nombre"] = NombreDe(def);
		d["peso"] = PesoDe(def);
		d["icono"] = def.is_valid() ? def->get("icono") : Variant();
		d["equipable"] = EsEquipo(def);
		d["usable"] = def.is_valid() && !Array(def->get("efectos_al_usar")).is_empty();
		out.push_back(d);
	}
	return out;
}

} // namespace inventario
