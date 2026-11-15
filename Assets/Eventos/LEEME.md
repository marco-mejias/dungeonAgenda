# Arte de los eventos

Cada evento tiene su carpeta (`Assets/Eventos/<evento>/`) con estos PNG. Ahora mismo son
**placeholders**: sobrescribe el archivo con tu dibujo **con el mismo nombre y tamaño** y el juego lo usa
sin tocar nada más. `python Tools/Sprites/datos_eventos.py` no pisa los que hayas dibujado tú.

| Archivo | Tamaño | Qué es |
|---|---|---|
| `mapa.png` | 16x16 (o una tira de 16 de alto: 64x16 = 4 fotogramas, se anima) | Cómo se ve el evento en el mapa (manuales y de proximidad; las trampas no se ven) |
| `escena.png` | 96x96 | La ilustración al abrir el evento (se ve a 2x, píxel nítido) |
| `opcionN_exito.png` | 96x96 | Lo que se ve al GANAR con la opción N (1, 2, 3...) |
| `opcionN_fracaso.png` | 96x96 | Lo que se ve al PERDER con la opción N |
| `exito.png` / `fracaso.png` | 96x96 | Las de reserva del evento (si una opción no tiene la suya) |

Estilo: el de los héroes (contorno #25131a de 1 px, luz arriba a la izquierda, paleta apagada).
