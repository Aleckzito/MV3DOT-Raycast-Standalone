# Data JSON + Luau — Standalone (mapa BASE v0.3)

Recrea el contrato de contenido de
`C:\OTRaycast-MV3D_v0.3_BASE_2026-08-01` dentro de este prototipo OSG.
No se copia `bridge_server` ni el cliente de red.

**Regla de oro:** cero red, cero sockets, cero opcodes. `StandaloneEngine` es el host.

**JSON** = definiciones. **Luau** = hooks allowlisted. **C++** = física, DDA, voxels, cámara, balística.

Fallo de catálogo o script = WARN + defaults del prototipo. El exe arranca igual.

---

## Orden (acomodado a lo que ya corre)

| Fase | Qué | Estado |
|---|---|---|
| S0 | Este documento | hecho |
| S1 | Árbol `data/` + schemas + catálogo 1:1 con tipos C++ | hecho |
| S2 | `LocalContentRegistry` al boot (HP/dmg/speed) | hecho |
| S3 | Pack sandbox + `standalone_sandbox.meta.json` (spawns) | hecho |
| S3.1 | Mapa de voxeles al boot + `playerSpawn` del meta + F9 reload | hecho |
| S4 | Abilities teclas 1–5 + loot/XP/energy JSON | hecho |
| S5 | Luau VM + `LocalScriptHost` + heartbeat | hecho |
| S6 | Hooks `onKill` / `onFire` / `onHunter` / `onLockOn` / `onLoot` | hecho |
| S7 | Inventario local (JSON stub + `LocalInventory`) | stub data + clase mínima |
| S8 | NPCs / talkactions en el voxel | hecho: Oracle/Merchant cajas + talk al acercarse |
| S9 | Quests / storage | hecho: First Blood (1 kill) + `save/sandbox_storage.json` |
| S10 | Vocaciones sobre DummyActor | hecho: Oracle otorga Scout tras First Blood |
| S11 | Isla OTBM como segundo mundo | no portar; formato incompatible |

Fuente BASE (solo lectura, no duplicar audits): `docs/plan/master_compendium.md`.

---

## Dos mundos

- `data/worlds/standalone_sandbox.json` — mini-voxels `[vx,vy,vz,matId]`. **El mapa manda**:
  si carga, `spawnDefaultPillars()` no corre. Si falta, WARN + arena hardcodeada.
- `data/worlds/standalone_sandbox.meta.json` — packId, `voxelWorld`, `playerSpawn`, spawns.
  No es el JSON v2 OTBM.
- `rookgaard_test.json` no se carga aquí.

Prioridad de ruta del mapa: `argv[1]` > `voxelWorld` del meta > `data/worlds/standalone_sandbox.json`.
Se resuelve contra la raíz única (ver más abajo), nunca relativa cruda.

Las rocas (`HeavyBoulder2x2`) vacían su celda, así que siempre se colocan **después** del mapa.

### Editar el mapa sin recompilar

1. F5 en el juego guarda el grid actual a `data/worlds/standalone_sandbox.json`.
2. F9 recarga ese archivo en caliente (rebuild de mesh + reset del DDA).

Verificación en consola: `[world-io] loaded N voxels <- <ruta absoluta>` seguido de `[world] mapa <- ...`.
Si sale `[arena] 4 cover pillars (default C++)`, el JSON no se leyó.

---

## Raíz de datos (una sola)

CMake copia `data/` y `config.json` junto al exe, así que en disco puede haber varios
árboles `data/` válidos a la vez. **Todo** —contenido, mapas, scripts, plantillas y
partidas— se resuelve contra una única raíz, en `src/standalone/DataRoot.{h,cpp}`:

**Manda la ubicación del ejecutable, no el CWD.** Las bases se recorren en este orden:
directorio del exe, sus tres padres, y el CWD **el último**. Sobre esa lista:

1. Gana la primera base con `data/content/registry_catalog.json`, `CMakeLists.txt` y el
   directorio `src/`: ese es el repo de verdad.
2. Si ninguna tiene `src/`, la primera con catálogo (distribución suelta).
3. Sin catálogo en ningún sitio, último recurso: `findRepoRoot()` (marcador `config.json`).

El CWD va al final a propósito: si lanzas el binario del repo A estando parado en otro
clon B igual de válido, tiene que seguir usando A. Con el CWD primero, B ganaba.

Se resuelve una vez por proceso y se anuncia al arrancar:

```
[data] root C:/OTRaycast-MV3D-Engine-Standalone
```

**Ese log es el que hay que mirar** cuando algo cargue de donde no esperabas. Para un
binario dado la raíz es siempre la misma, ejecutes desde donde ejecutes.

> Antes convivían cuatro mecanismos y se mezclaban: el contenido usaba la raíz con
> `src/`, mientras las partidas usaban `findRepoRoot()`, que ancla en el CWD. Corriendo
> desde `build/Release` el mapa salía del repo pero la partida se escribía junto al exe.
> `dataRoot()` es ahora el único punto de decisión; nadie más llama a `findDataFile`.

El POST_BUILD tampoco copia `data/player/save` junto al exe, para no recrear un segundo
árbol de partidas.

---

## Plantillas vs partidas

El estado del jugador vive en dos sitios distintos y **no se pisan**:

| Ruta | Qué es | Git |
|---|---|---|
| `data/player/sandbox_storage.json` | Plantilla: quests y vocación al empezar | versionado, **solo lectura** |
| `data/player/sandbox_inventory.json` | Plantilla: inventario inicial | versionado, **solo lectura** |
| `data/player/save/sandbox_storage.json` | Partida: flags de quest + `vocationId` | ignorado |
| `data/player/save/sandbox_inventory.json` | Partida: stacks del inventario | ignorado |

Regla, en `src/standalone/PlayerSave.{h,cpp}`:

- **Leer** → `playerLoadPath()`: la partida si existe, si no la plantilla.
- **Escribir** → `playerSavePath()`: siempre bajo `data/player/save/`.

El juego nunca escribe sobre las plantillas, así que jugar no ensucia el diff del repo.
Para empezar de cero, borra `data/player/save/`: se regenera desde la plantilla.

> La plantilla de storage debe quedar en estado inicial (`flags: {}`, `vocationId: 0`).
> Si la commiteas con una partida dentro, todo clon arranca con esa quest ya hecha.

---

## Hotbar

HUD vivo: `[1] Jump | [2] Gun (Auto) | [3] Melee | [4] Bomb | [5] Laser`
en `data/ui/hotbar_sandbox.json`.

El contrato BASE de 8 slots RPG queda en `data/ui/hotbar_roles_ref.json` (referencia, no pisa el HUD).

---

## Prohibido

- `net_socket`, `ServerNetwork`, `ClientNetwork`, `ws2_32`, `OP_RC_*`
- SQLite / `sqlite_backup.luau`
- Copiar `data/libs` Canary (GPL)
- Luau decidiendo hitscan / físicas

---

## Invariantes

- Un inventario, un reloj, un pipeline de proyectiles.
- Scripts solo bajo `data/scripts/**/*.luau`, manifest `data/scripts/scripts.json`.
- Sandbox: sin `io`, `os`, `package`, `debug`, `require`, `dofile`, `loadfile`, `loadstring`, `load`.
- Identidad: `contentId` / `runtimeId` / `definitionPath`.

### S9 / S10 (sandbox)

- Matar 1 hostil completa `otr.quest.sandbox.first_blood` (popup + `data/player/save/sandbox_storage.json`).
- Acercarse al Oracle con la quest `done` y vocación 0 otorga Scout (persistido).
- Luau `otr.hooks.onQuest` / `onVocation` solo loguean; C++ es la autoridad.
