# Créditos

Dungeon Agenda está hecho en [Godot Engine](https://godotengine.org) con
[Jenova Framework](https://jenova-framework.github.io) para el código C++.

## Arte propio

- Sprites de los héroes (y sus capas de equipo), iconos de objetos y de habilidades e ilustraciones
  de los eventos (`Assets/Sprites`, `Assets/Iconos`, `Assets/Habilidades` y `Assets/Eventos`).
- Interfaz de grimorio (`Assets/UI/Grimorio`): texturas generadas por código con
  `Tools/generar_ui_grimorio.gd`.

## Arte de terceros

| Recurso | Autor | Uso |
|---|---|---|
| 2D Pixel Dungeon Asset Pack | Pixel_Poem | Enemigos y tileset (`Assets/2D Pixel Dungeon Asset Pack`) |

## Fuentes

Todas con licencia [SIL Open Font License 1.1](https://openfontlicense.org); el texto de la licencia
va junto a cada fuente (`OFL.txt`).

| Fuente | Autor | Uso |
|---|---|---|
| Alegreya | Juan Pablo del Peral (Huerta Tipográfica) | Texto de la interfaz |
| IM FELL English / English SC | Igino Marini | Títulos |

## Sonido

Los sonidos **no se incluyen en el repositorio**: son packs de terceros y algunas licencias no
permiten redistribuirlos. Sin la carpeta `Assets/Audio` el juego funciona igual, en silencio
(`Scripts/GDD/AudioManager.gd` solo carga los que encuentra).

| Recurso | Autor | Licencia |
|---|---|---|
| Universal UI Soundpack | Nathan Gibson | CC BY 4.0 |
| RPG Essentials SFX | Leohpaz | Gratuita, con atribución |
| Epic Dungeon Beat | Kaden Cook (Pixabay) | Licencia de Pixabay |
| Dice | abdrtar (Freesound) | Freesound |
