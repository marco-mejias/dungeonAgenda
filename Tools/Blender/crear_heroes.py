# Crea los 4 protagonistas en 3D low poly (estilo PS1) con su esqueleto y animación de reposo.
#   D:\blender.exe --background --factory-startup --python Tools/Blender/crear_heroes.py -- <raiz_proyecto> [<carpeta_previas>]
# Salida: Assets/Modelos/Heroes/<clase>.glb y heroes.blend (para retocarlos a mano), y en
# <carpeta_previas> unas imágenes de los cuatro juntos.
#
# Cada héroe es UNA malla hecha de piezas rígidas (cajas achaflanadas, conos, esferas de pocos
# lados), cada pieza pegada al 100 % a un hueso: así se animaban los personajes en PS1.
# Huesos: Cadera, Torso, Cabeza, Brazo/Mano/Pierna .L y .R. Las manos y la cabeza sirven de
# enganche para armas, escudos y cascos. Mira hacia -Y (de frente), mide ~0.9 m (una casilla = 1 m).

import bpy, bmesh, math, sys, os
from mathutils import Vector, Matrix, Euler

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
RAIZ = argv[0] if argv else os.getcwd()
PREVIAS = argv[1] if len(argv) > 1 else os.path.join(RAIZ, "Assets", "Modelos", "Heroes")
SALIDA = os.path.join(RAIZ, "Assets", "Modelos", "Heroes")
os.makedirs(SALIDA, exist_ok=True)
os.makedirs(PREVIAS, exist_ok=True)

HUESOS = ["Cadera", "Torso", "Cabeza", "Brazo.L", "Brazo.R", "Mano.L", "Mano.R", "Pierna.L", "Pierna.R"]


def color(hexa):
    h = hexa.lstrip("#")
    c = [int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    # sRGB -> lineal (lo que espera el BSDF)
    return tuple(((x + 0.055) / 1.055) ** 2.4 if x > 0.04045 else x / 12.92 for x in c) + (1.0,)


class Heroe:
    """Va juntando piezas en un solo bmesh, cada una con su material y su hueso."""

    def __init__(self, nombre, paleta):
        self.nombre = nombre
        self.paleta = paleta
        self.bm = bmesh.new()
        self.deform = self.bm.verts.layers.deform.verify()
        self.mats = list(paleta.keys())

    def _pieza(self, crear, hueso, col, pos, rot=(0, 0, 0), escala=(1, 1, 1), chaflan=0.0):
        antes = set(self.bm.verts)
        crear()
        nuevos = [v for v in self.bm.verts if v not in antes]
        bmesh.ops.scale(self.bm, vec=Vector(escala), verts=nuevos)
        bmesh.ops.rotate(self.bm, cent=Vector((0, 0, 0)), matrix=Euler([math.radians(a) for a in rot]).to_matrix(), verts=nuevos)
        bmesh.ops.translate(self.bm, vec=Vector(pos), verts=nuevos)
        if chaflan > 0:
            aristas = list({e for v in nuevos for e in v.link_edges})
            bmesh.ops.bevel(self.bm, geom=nuevos + aristas, offset=chaflan, segments=1, affect='EDGES', profile=0.5)
            nuevos = [v for v in self.bm.verts if v not in antes]
        conjunto = set(nuevos)
        grupo = HUESOS.index(hueso)
        for v in nuevos:
            v[self.deform][grupo] = 1.0
        indice = self.mats.index(col)
        for f in self.bm.faces:
            if all(v in conjunto for v in f.verts):
                f.material_index = indice
                f.smooth = False

    def caja(self, tam, pos, col, hueso, rot=(0, 0, 0), chaflan=0.0):
        self._pieza(lambda: bmesh.ops.create_cube(self.bm, size=1.0), hueso, col, pos, rot, tam, chaflan)

    def cono(self, r1, r2, alto, pos, col, hueso, lados=6, rot=(0, 0, 0), escala=(1, 1, 1)):
        self._pieza(lambda: bmesh.ops.create_cone(self.bm, cap_ends=True, cap_tris=False, segments=lados, radius1=r1, radius2=r2, depth=alto),
                    hueso, col, pos, rot, escala)

    def esfera(self, r, pos, col, hueso, escala=(1, 1, 1), lados=8, anillos=6):
        self._pieza(lambda: bmesh.ops.create_uvsphere(self.bm, u_segments=lados, v_segments=anillos, radius=r), hueso, col, pos, (0, 0, 0), escala)

    brillo = 2.0  # fuerza de lo que brilla (ojos del mago, visor del tanque)

    def construir(self, desplazamiento=(0, 0, 0), emisivos=()):
        malla = bpy.data.meshes.new(self.nombre)
        self.bm.to_mesh(malla)
        self.bm.free()
        for nombre in self.mats:
            m = bpy.data.materials.get(self.nombre + "_" + nombre) or bpy.data.materials.new(self.nombre + "_" + nombre)
            m.use_nodes = True
            bsdf = m.node_tree.nodes.get("Principled BSDF")
            bsdf.inputs["Base Color"].default_value = color(self.paleta[nombre])
            bsdf.inputs["Roughness"].default_value = 1.0
            if "Specular IOR Level" in bsdf.inputs:
                bsdf.inputs["Specular IOR Level"].default_value = 0.0
            if nombre in emisivos:
                bsdf.inputs["Emission Color"].default_value = color(self.paleta[nombre])
                bsdf.inputs["Emission Strength"].default_value = self.brillo
            m.diffuse_color = color(self.paleta[nombre])
            malla.materials.append(m)
        obj = bpy.data.objects.new(self.nombre, malla)
        bpy.context.scene.collection.objects.link(obj)
        for h in HUESOS:
            obj.vertex_groups.new(name=h)
        arm = crear_esqueleto(self.nombre + "_Esqueleto", self.ancho_hombros, self.ancho_piernas)
        obj.parent = arm
        mod = obj.modifiers.new("Esqueleto", 'ARMATURE')
        mod.object = arm
        arm.location = Vector(desplazamiento)
        crear_reposo(arm, self.nombre)
        return arm, obj


def crear_esqueleto(nombre, hombro, pierna):
    datos = bpy.data.armatures.new(nombre)
    arm = bpy.data.objects.new(nombre, datos)
    bpy.context.scene.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode='EDIT')
    eb = datos.edit_bones
    def hueso(n, cabeza, cola, padre=None):
        b = eb.new(n)
        b.head = Vector(cabeza)
        b.tail = Vector(cola)
        if padre:
            b.parent = eb[padre]
        return b
    hueso("Cadera", (0, 0, 0.22), (0, 0, 0.28))
    hueso("Torso", (0, 0, 0.28), (0, 0, 0.47), "Cadera")
    hueso("Cabeza", (0, 0, 0.47), (0, 0, 0.82), "Torso")
    for lado, s in (("L", 1), ("R", -1)):
        hueso("Brazo." + lado, (s * hombro, 0, 0.45), (s * hombro, 0, 0.27), "Torso")
        hueso("Mano." + lado, (s * hombro, 0, 0.27), (s * hombro, 0, 0.17), "Brazo." + lado)
        hueso("Pierna." + lado, (s * pierna, 0, 0.22), (s * pierna, 0, 0.0), "Cadera")
    bpy.ops.object.mode_set(mode='OBJECT')
    return arm


def crear_reposo(arm, nombre):
    """Reposo en bucle de 2 s: respira (sube y baja), balancea brazos y mueve la cabeza."""
    accion = bpy.data.actions.new("Reposo")
    arm.animation_data_create()
    arm.animation_data.action = accion
    pb = arm.pose.bones
    for b in pb:
        b.rotation_mode = 'XYZ'
    claves = [1, 13, 25, 37, 49]
    respira = [0.0, 1.0, 0.0, -1.0, 0.0]
    for f, k in zip(claves, respira):
        pb["Cadera"].location = (0, 0, 0.012 * max(k, 0) - 0.004 * max(-k, 0))
        pb["Cadera"].keyframe_insert("location", frame=f)
        pb["Torso"].rotation_euler = (math.radians(2.0 * k), 0, 0)
        pb["Torso"].keyframe_insert("rotation_euler", frame=f)
        pb["Cabeza"].rotation_euler = (math.radians(-1.5 * k), 0, math.radians(3.0 * k))
        pb["Cabeza"].keyframe_insert("rotation_euler", frame=f)
        for lado, s in (("L", 1), ("R", -1)):
            pb["Brazo." + lado].rotation_euler = (math.radians(4.0 * k), math.radians(s * 3.0 * abs(k)), 0)
            pb["Brazo." + lado].keyframe_insert("rotation_euler", frame=f)
    bpy.context.scene.frame_start = 1
    bpy.context.scene.frame_end = 48


# ==========================================
# CUERPO BASE (proporciones chibi, como los sprites)
# ==========================================

def cuerpo_base(h, c, ancho=1.0):
    """c: dict de colores por parte (pierna, pie, torso, cinto, brazo, mano, cabeza)."""
    hx = 0.21 * ancho
    px = 0.085 * ancho
    h.ancho_hombros = hx
    h.ancho_piernas = px
    for lado, s in (("L", 1), ("R", -1)):
        h.caja((0.12 * ancho, 0.13, 0.20), (s * px, 0, 0.12), c["pierna"], "Pierna." + lado)
        h.caja((0.13 * ancho, 0.18, 0.06), (s * px, -0.02, 0.03), c["pie"], "Pierna." + lado, chaflan=0.01)
        h.caja((0.09 * ancho, 0.10, 0.20), (s * hx, 0, 0.36), c["brazo"], "Brazo." + lado, chaflan=0.01)
        h.caja((0.09 * ancho, 0.09, 0.08), (s * hx, -0.005, 0.22), c["mano"], "Mano." + lado, chaflan=0.012)
    h.caja((0.32 * ancho, 0.22, 0.24), (0, 0, 0.34), c["torso"], "Torso", chaflan=0.02)
    h.caja((0.33 * ancho, 0.23, 0.05), (0, 0, 0.245), c["cinto"], "Cadera")
    h.caja((0.36, 0.32, 0.32), (0, 0, 0.64), c["cabeza"], "Cabeza", chaflan=0.045)


def caballero(desp=(0, 0, 0)):
    h = Heroe("Caballero", {"acero": "9aa3b5", "acero_osc": "5f6778", "oro": "d7b24a", "visor": "15161b", "capa": "8c2f2f"})
    cuerpo_base(h, {"pierna": "acero_osc", "pie": "acero", "torso": "acero", "cinto": "oro", "brazo": "acero", "mano": "acero_osc", "cabeza": "acero"})
    h.caja((0.26, 0.02, 0.05), (0, -0.162, 0.64), "visor", "Cabeza")                     # rendija del yelmo
    h.caja((0.03, 0.02, 0.14), (0, -0.162, 0.58), "visor", "Cabeza")                     # nasal
    for s in (1, -1):
        h.cono(0.075, 0.0, 0.13, (s * 0.115, 0.01, 0.84), "acero", "Cabeza", lados=4, rot=(0, 0, 45))   # orejas de gato
        h.cono(0.04, 0.0, 0.08, (s * 0.115, -0.025, 0.83), "oro", "Cabeza", lados=4, rot=(0, 0, 45))
        h.caja((0.14, 0.16, 0.07), (s * 0.2, 0, 0.475), "acero", "Brazo." + ("L" if s > 0 else "R"), chaflan=0.02)  # hombreras
    h.caja((0.08, 0.012, 0.08), (0, -0.113, 0.38), "oro", "Torso", rot=(0, 45, 0))          # emblema
    h.caja((0.30, 0.03, 0.32), (0, 0.125, 0.31), "capa", "Torso")                           # capa
    return h.construir(desp)


def ladron(desp=(0, 0, 0)):
    h = Heroe("Ladron", {"piel": "e0a878", "pelo": "4a2f1e", "cuero": "6b4a2e", "cuero_osc": "45301e", "panuelo": "2f7d6d", "panuelo_osc": "1f5a4e", "pantalon": "34343e", "ojos": "15161b"})
    cuerpo_base(h, {"pierna": "pantalon", "pie": "cuero_osc", "torso": "cuero", "cinto": "cuero_osc", "brazo": "cuero", "mano": "piel", "cabeza": "piel"})
    h.caja((0.37, 0.33, 0.09), (0, 0.0, 0.775), "pelo", "Cabeza", chaflan=0.03)             # pelo
    h.caja((0.37, 0.07, 0.24), (0, 0.145, 0.66), "pelo", "Cabeza")                          # nuca
    h.caja((0.375, 0.335, 0.05), (0, 0, 0.725), "panuelo", "Cabeza")                        # pañuelo en la frente
    h.caja((0.05, 0.13, 0.035), (0.05, 0.23, 0.72), "panuelo_osc", "Cabeza", rot=(-25, 0, 15))  # puntas del nudo
    h.caja((0.05, 0.11, 0.035), (-0.03, 0.22, 0.70), "panuelo_osc", "Cabeza", rot=(-35, 0, -20))
    for s in (1, -1):
        h.caja((0.055, 0.012, 0.06), (s * 0.08, -0.162, 0.635), "ojos", "Cabeza")
    h.caja((0.10, 0.012, 0.02), (0, -0.162, 0.56), "pelo", "Cabeza")                       # boca
    h.caja((0.30, 0.24, 0.06), (0, 0, 0.47), "panuelo", "Torso")                            # bufanda-serpiente
    h.caja((0.06, 0.02, 0.15), (0.07, -0.122, 0.39), "panuelo", "Torso")
    h.caja((0.07, 0.022, 0.03), (0.07, -0.124, 0.31), "panuelo_osc", "Torso")              # "cabeza" de la serpiente
    for x in (-0.11, 0.0, 0.11):
        h.caja((0.07, 0.05, 0.07), (x, -0.13, 0.24), "cuero_osc", "Cadera", chaflan=0.01)  # bolsas del cinturón
    return h.construir(desp)


def mago(desp=(0, 0, 0)):
    h = Heroe("Mago", {"tunica": "3b3570", "tunica_clara": "5a52a0", "plumas": "8a6a45", "plumas_claras": "c8a878", "cara": "1a1530", "ojos": "ffd23f", "pico": "e08a2a"})
    cuerpo_base(h, {"pierna": "tunica", "pie": "cara", "torso": "tunica", "cinto": "plumas_claras", "brazo": "tunica", "mano": "plumas_claras", "cabeza": "plumas"})
    h.cono(0.25, 0.17, 0.26, (0, 0, 0.13), "tunica", "Cadera", lados=8)                      # falda de la túnica
    h.cono(0.255, 0.252, 0.04, (0, 0, 0.02), "tunica_clara", "Cadera", lados=8)              # ribete
    h.caja((0.27, 0.02, 0.21), (0, -0.162, 0.615), "cara", "Cabeza")                         # cara en sombra
    for s in (1, -1):
        h.caja((0.065, 0.014, 0.065), (s * 0.065, -0.168, 0.645), "ojos", "Cabeza")          # ojos que brillan
        h.cono(0.055, 0.0, 0.15, (s * 0.135, 0.0, 0.86), "plumas", "Cabeza", lados=4, rot=(0, s * -20, 45))  # penachos de búho
        h.caja((0.11, 0.12, 0.20), (s * 0.215, 0, 0.36), "tunica", "Brazo." + ("L" if s > 0 else "R"), chaflan=0.01)  # mangas anchas
    h.cono(0.032, 0.0, 0.06, (0, -0.19, 0.585), "pico", "Cabeza", lados=4, rot=(90, 0, 45))   # pico
    h.caja((0.42, 0.27, 0.06), (0, 0.0, 0.475), "plumas_claras", "Torso", chaflan=0.02)       # capa de plumas
    h.caja((0.30, 0.04, 0.22), (0, 0.13, 0.36), "plumas", "Torso")
    return h.construir(desp, emisivos=("ojos",))


def tanque(desp=(0, 0, 0)):
    h = Heroe("Tanque", {"armadura": "4f7a3a", "armadura_osc": "2f4a26", "gris": "3a3a44", "visor": "d79a2a", "concha": "b5743a", "concha_osc": "7a4a24", "bola": "cfe08a"})
    cuerpo_base(h, {"pierna": "gris", "pie": "armadura_osc", "torso": "armadura", "cinto": "gris", "brazo": "armadura", "mano": "armadura_osc", "cabeza": "armadura"}, ancho=1.18)
    h.caja((0.28, 0.02, 0.10), (0, -0.162, 0.645), "visor", "Cabeza")                        # visor ámbar
    h.caja((0.38, 0.06, 0.06), (0, -0.14, 0.53), "armadura_osc", "Cabeza")                   # barbilla
    for s in (1, -1):
        h.cono(0.016, 0.012, 0.15, (s * 0.09, 0.0, 0.88), "armadura", "Cabeza", lados=5, rot=(0, s * 18, 0))  # antenas
        h.esfera(0.035, (s * 0.115, 0.0, 0.955), "bola", "Cabeza", lados=6, anillos=4)
        h.caja((0.17, 0.19, 0.09), (s * 0.245, 0, 0.48), "armadura_osc", "Brazo." + ("L" if s > 0 else "R"), chaflan=0.025)  # hombreras
    h.esfera(0.25, (0, 0.23, 0.44), "concha", "Torso", escala=(1.0, 0.62, 1.0), lados=8, anillos=6)   # concha (asoma por arriba)
    h.esfera(0.16, (0.0, 0.33, 0.48), "concha_osc", "Torso", escala=(1.0, 0.5, 1.0), lados=8, anillos=5)
    h.esfera(0.08, (0.0, 0.385, 0.50), "concha", "Torso", lados=6, anillos=4)
    h.brillo = 0.8
    return h.construir(desp, emisivos=("visor",))


CONSTRUCTORES = {"caballero": caballero, "ladron": ladron, "mago": mago, "tanque": tanque}


def vaciar():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def exportar(clase):
    vaciar()
    arm, obj = CONSTRUCTORES[clase]()
    bpy.ops.object.select_all(action='DESELECT')
    arm.select_set(True)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = arm
    ruta = os.path.join(SALIDA, clase + ".glb")
    bpy.ops.export_scene.gltf(filepath=ruta, export_format='GLB', use_selection=True, export_animations=True,
                              export_yup=True, export_apply=False)
    print("EXPORTADO", ruta)


def escena_previa():
    """Los cuatro juntos, con luz de mazmorra, para renderizar las imágenes de muestra."""
    vaciar()
    for i, clase in enumerate(["caballero", "ladron", "mago", "tanque"]):
        arm, obj = CONSTRUCTORES[clase]((-0.9 + i * 0.6, 0, 0))
    esc = bpy.context.scene
    esc.render.engine = 'BLENDER_EEVEE'
    esc.render.film_transparent = False
    esc.render.resolution_x = 1200
    esc.render.resolution_y = 520
    esc.view_settings.view_transform = 'Standard'
    mundo = bpy.data.worlds.new("Mundo")
    mundo.use_nodes = True
    mundo.node_tree.nodes["Background"].inputs[0].default_value = color("22232b")
    mundo.node_tree.nodes["Background"].inputs[1].default_value = 0.6
    esc.world = mundo
    sol = bpy.data.objects.new("Sol", bpy.data.lights.new("Sol", 'SUN'))
    sol.data.energy = 3.0
    sol.rotation_euler = Euler((math.radians(50), 0, math.radians(-35)))
    esc.collection.objects.link(sol)
    relleno = bpy.data.objects.new("Antorcha", bpy.data.lights.new("Antorcha", 'POINT'))
    relleno.data.energy = 60
    relleno.data.color = (1.0, 0.7, 0.4)
    relleno.location = (1.2, -1.4, 1.2)
    esc.collection.objects.link(relleno)
    suelo_m = bpy.data.meshes.new("Suelo")
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=3.0)
    bm.to_mesh(suelo_m)
    suelo = bpy.data.objects.new("Suelo", suelo_m)
    mat = bpy.data.materials.new("Suelo")
    mat.use_nodes = True
    mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = color("3b2a2e")
    suelo_m.materials.append(mat)
    esc.collection.objects.link(suelo)
    cam = bpy.data.objects.new("Camara", bpy.data.cameras.new("Camara"))
    esc.collection.objects.link(cam)
    esc.camera = cam
    return esc, cam


def render(esc, cam, ruta, pos, mira, lente=50):
    cam.location = Vector(pos)
    direccion = Vector(mira) - cam.location
    cam.rotation_euler = direccion.to_track_quat('-Z', 'Y').to_euler()
    cam.data.lens = lente
    esc.render.filepath = ruta
    bpy.ops.render.render(write_still=True)
    print("RENDER", ruta)


for clase in CONSTRUCTORES:
    exportar(clase)

esc, cam = escena_previa()
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(SALIDA, "heroes.blend"))
bpy.context.scene.frame_set(13)
render(esc, cam, os.path.join(PREVIAS, "heroes_frente.png"), (0.0, -3.4, 1.1), (0, 0, 0.42), 45)
render(esc, cam, os.path.join(PREVIAS, "heroes_iso.png"), (2.2, -2.6, 2.0), (0, 0, 0.38), 40)
render(esc, cam, os.path.join(PREVIAS, "heroes_espalda.png"), (-1.6, 3.0, 1.4), (0, 0, 0.40), 42)
print("FIN")
