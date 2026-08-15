# MV3DOT-Raycast-Standalone

Prototipo standalone de raycast sobre mini-voxels (OSG + SDL2 + Luau).

**Cero red, cero sockets, cero `bridge_server`.** `StandaloneEngine` es el host: no hay
cliente ni servidor, y el guardado es local en JSON.

- **JSON** = definiciones (contenido, mundo, quests, vocaciones).
- **Luau** = hooks allowlisted.
- **C++** = física, DDA, voxels, cámara, balística.

## Requisitos

Una sola dependencia vive fuera del repo:

| Dependencia | Estado |
|---|---|
| **osgVerse / OpenSceneGraph** | **Externa.** SDK ya compilado (~483 MB). No es vendorizable. |
| SDL2 | Vía `vcpkg_installed` del build de osgVerse |
| Luau (MIT) | Vendorizado en `thirdparty/luau` |
| math / grid DDA | Vendorizado en `src/shared` |

Todo lo demás está en el repo. Clonar y compilar funciona siempre que apuntes osgVerse.

## Compilar

Si tu SDK de osgVerse no está en `C:/MV3D/osgverse-mv3d`, indícalo al configurar:

```bash
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DRC_OSGVERSE_BUILD=<ruta>/build
```

Con la ruta por defecto basta con:

```bash
build.bat
```

El binario queda en `build/Release/` y se copia a la raíz. CMake también copia `data/` y
`config.json` junto al exe.

## Ejecutar

```bash
raycast_standalone.exe
```

Acepta un mapa de voxeles opcional como primer argumento; si no, manda `voxelWorld` del
meta del pack, y en último término `data/worlds/standalone_sandbox.json`.

Teclas: `1` salto, `2` gun, `3` melee, `4` bomba, `5` láser, `C` cámara, `F2` editor,
`F5` guardar mapa, `F9` recargarlo en caliente.

Al arrancar anuncia de dónde lee todo:

```
[data] root C:/OTRaycast-MV3D-Engine-Standalone
```

CMake copia `data/` junto al exe, así que puede haber varios árboles en disco, pero la
raíz se elige una sola vez y vale para contenido, mapas, scripts y partidas: gana el repo
de verdad (el que tiene `src/`), ejecutes desde donde ejecutes. Detalle en
[`docs/DATA_LUAU_STANDALONE.md`](docs/DATA_LUAU_STANDALONE.md#raíz-de-datos-una-sola).

## Datos y partidas

- `data/**` — contenido versionado. Editable sin recompilar.
- `data/player/*.json` — **plantillas** de estado inicial. Solo lectura.
- `data/player/save/` — partidas locales. Ignorado por git, así jugar no ensucia el diff.

Documentación del contrato de contenido: [`docs/DATA_LUAU_STANDALONE.md`](docs/DATA_LUAU_STANDALONE.md).

## Licencias

Luau es MIT; se conservan `thirdparty/luau/LICENSE.txt` y `lua_LICENSE.txt`. No se
incluye ninguna parte GPL de Canary.
