# HUD (placeholders)

Arte del HUD de partida (`Scripts/GDD/hud.gd`). Sustituye cada PNG por el tuyo con el mismo nombre;
cualquier tamaño vale, se escalan sin suavizado (16x16 o 32x32 recomendado).

| Archivo                | Dónde sale                                                          |
|------------------------|---------------------------------------------------------------------|
| corazon_lleno.png      | medidor de vida del retrato (la parte llena del líquido)             |
| corazon_vacio.png      | medidor de vida vacío (el fondo del icono)                           |
| carne_lleno/vacio.png  | medidor de hambre                                                    |
| ojo_lleno/vacio.png    | medidor de cordura                                                   |
| mochila.png            | botón grande del centro de la rueda (abre la mochila)                |
| sol.png                | botón "Fin del día" de la rueda                                      |
| flecha.png             | botón "Fin del turno" de la rueda                                    |

El líquido se dibuja recortando `_lleno` desde abajo según el % que queda, encima de `_vacio`:
haz las dos versiones con la misma silueta.

Los iconos de atacar y habilidad salen de `Scripts/GDD/iconos_pixel.gd`, y los de las
stats del bocadillo de `Assets/UI/Indicadores` (y `Assets/UI/Iconos` para la luz).
Los regenera `python Tools/Sprites/hud_placeholders.py` (¡sobrescribe los PNG!).
