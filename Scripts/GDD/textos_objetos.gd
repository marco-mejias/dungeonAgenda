class_name TextosObjetos
extends RefCounted

# Item texts for the interface (inventory screen, F10 panel): one-line summary, detailed card and
# comparison with what the hero is wearing in that slot.

const NOMBRE_STAT := {
	"hp_max": "vida máx", "velocidad": "PA", "fuerza": "fuerza", "defensa": "defensa", "magia": "magia",
	"suerte": "suerte", "radio_luz": "luz", "sanidad_max": "cordura máx", "hambre_max": "hambre máx",
}
const RANURA_DE := ["mano_principal", "mano_secundaria", "cabeza", "cuerpo", "accesorio_1", "luz", "mochila"]
const NOMBRE_RANURA := {
	"mano_principal": "Mano principal", "mano_secundaria": "Mano secundaria", "cabeza": "Cabeza", "cuerpo": "Cuerpo",
	"accesorio_1": "Accesorio", "accesorio_2": "Accesorio", "luz": "Luz", "mochila": "Mochila",
}
const VERDE := "7fe08a"
const ROJO := "ff7a6e"
const GRIS := "a6b0d8"

# Readable stat name ("fuerza" -> "Fuerza")
static func nombre_stat(s: String) -> String:
	return NOMBRE_STAT.get(s, s)

# "+2 Fuerza", "+10% Defensa"...
static func describir_mod(m: ModificadorStat) -> String:
	var partes: PackedStringArray = []
	if m.plano != 0:
		partes.append("%+d" % m.plano)
	if m.porcentaje != 0:
		partes.append("%+d%%" % m.porcentaje)
	return "%s %s" % [" ".join(partes), nombre_stat(m.stat)]

# One line: "Espada corta [daño 4, 2 PA, alcance 1]"
static func resumen(o: ObjetoData) -> String:
	if o == null:
		return "—"
	var extra: PackedStringArray = []
	if o is ArmaData:
		extra.append("daño %d, %d PA, alcance %s" % [o.dano, o.coste_pa, _alcance(o)])
	if o is MochilaData:
		extra.append("%.0f kg" % o.capacidad)
	if o is EquipoData:
		for m in o.modificadores:
			if m:
				extra.append(describir_mod(m))
	for e in o.efectos_al_usar:
		if e:
			extra.append(e.describir())
	return o.nombre_completo() + ("  [" + ", ".join(extra) + "]" if not extra.is_empty() else "")

# Weapon reach as text ("1" or "2-4")
static func _alcance(a: ArmaData) -> String:
	return str(a.alcance_min) if a.alcance_min == a.alcance_max else "%d-%d" % [a.alcance_min, a.alcance_max]

# Full card in BBCode (description, stats, effects, weight)
static func detalle(o: ObjetoData) -> String:
	if o == null:
		return ""
	var t := "[b]%s[/b]\n" % o.nombre_completo()
	if o.descripcion != "":
		t += "[i][color=#%s]%s[/color][/i]\n" % [GRIS, o.descripcion]
	if o is ArmaData:
		t += "Daño %d + %s · %d PA · alcance %s%s%s\n" % [o.dano, o.stat_ataque, o.coste_pa, _alcance(o),
			" · necesita ver al objetivo" if o.requiere_vision and o.alcance_max > 1 else "", " · a dos manos" if o.dos_manos else ""]
		if o.critico_extra > 0 or o.mult_critico != 2.0:
			t += "Crítico +%d%% · x%.1f\n" % [o.critico_extra, o.mult_critico]
	if o is MochilaData:
		t += "Capacidad: %.1f kg\n" % o.capacidad
	if o is EquipoData:
		var mods: PackedStringArray = []
		for m in o.modificadores:
			if m:
				mods.append(describir_mod(m))
		t += "Se lleva en: %s" % NOMBRE_RANURA[RANURA_DE[o.ranura]]
		t += ("  ·  " + ", ".join(mods) if not mods.is_empty() else "") + "\n"
		if not o.clases_permitidas.is_empty():
			t += "[color=#%s]Solo: %s[/color]\n" % [GRIS, ", ".join(o.clases_permitidas)]
	var efs: PackedStringArray = []
	for e in o.efectos_al_usar:
		if e:
			efs.append(e.describir())
	if not efs.is_empty():
		t += "Al usarlo: %s%s\n" % [", ".join(efs), "" if o.usos <= 0 else " (%d uso%s)" % [o.usos, "" if o.usos == 1 else "s"]]
	t += "[color=#%s]%.1f kg · %d oro[/color]" % [GRIS, o.peso, o.valor]
	return t

# Total bonuses of an equipment piece: {stat: flat value} (+ damage/reach if it's a weapon)
static func _bonos(o) -> Dictionary:
	var b := {}
	if o is EquipoData:
		for m in o.modificadores:
			if m:
				b[m.stat] = b.get(m.stat, 0) + m.plano
		for enc in o.encantamientos:
			if enc:
				for m in enc.modificadores:
					if m:
						b[m.stat] = b.get(m.stat, 0) + m.plano
	if o is ArmaData:
		b["daño"] = o.dano
		b["alcance"] = o.alcance_max
		b["coste PA"] = -o.coste_pa # Less cost = better
	if o is MochilaData:
		b["kg"] = int(o.capacidad)
	return b

# Comparison with what's equipped in the same slot, in BBCode ("" if it isn't equipment)
static func comparar(o: ObjetoData, equipado: ObjetoData) -> String:
	if not (o is EquipoData):
		return ""
	if equipado == null:
		return "[color=#%s]Ranura vacía: todo es ganancia[/color]" % VERDE
	var a := _bonos(o)
	var b := _bonos(equipado)
	var claves := a.keys()
	for k in b:
		if not k in claves:
			claves.append(k)
	var partes: PackedStringArray = []
	for k in claves:
		var d: int = int(a.get(k, 0)) - int(b.get(k, 0))
		if d == 0:
			continue
		var texto := "%+d %s" % [-d if k == "coste PA" else d, nombre_stat(k)]
		partes.append("[color=#%s]%s[/color]" % [VERDE if d > 0 else ROJO, texto])
	if partes.is_empty():
		return "[color=#%s]Igual que %s[/color]" % [GRIS, equipado.nombre_completo()]
	return "Frente a %s: %s" % [equipado.nombre_completo(), ", ".join(partes)]

# The same comparison without colours (for tooltips)
static func comparar_plano(o: ObjetoData, equipado: ObjetoData) -> String:
	var re := RegEx.create_from_string("\\[/?[a-z]+(=[^\\]]*)?\\]")
	return re.sub(comparar(o, equipado), "", true)
