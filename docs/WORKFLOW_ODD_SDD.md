# Flujo de trabajo: Vibe Coding dentro de ODD, SDD para auditorías

Versión 1.0 · 21 de octubre de 2026

## Propósito

Este repositorio permite exploración rápida con IA sin confundir generación con
aceptación. El flujo habitual es ODD proporcional: entender, comprobar, cambiar lo
mínimo, revisar y cerrar con evidencia. SDD se reserva para auditorías, incidentes y
cambios de contratos sensibles.

> La libertad de generación aumenta la velocidad; el alcance explícito, las pruebas y
> la evidencia determinan qué se acepta.

La política operativa breve para agentes vive en [`../AGENTS.md`](../AGENTS.md). Este
documento es la referencia humana y explica cómo aplicarla a los dos componentes propios
del repositorio.

## Los dos componentes

| Componente | Superficie | Responsabilidad | Consumidor/gate principal |
| --- | --- | --- | --- |
| `rc_shared_math_grid` | `src/shared/` | Matemáticas, DDA, defaults y rutas compartidas | Debe compilar la biblioteca y `raycast_standalone` |
| `raycast_standalone` | `src/engine/`, `src/world/`, `src/entities/`, `src/main_standalone.cpp`, `data/`, `config.json` | Juego standalone, render, input, física, contenido y persistencia local | Build del ejecutable, self-tests y smoke/manual según el cambio |

`thirdparty/luau/` es una dependencia MIT vendorizada. No forma un tercer proyecto de
producto y queda fuera del trabajo normal. Sus licencias se preservan y cualquier
actualización se trata como tarea explícita de dependencia.

## Elegir la ruta

| Situación | Ruta mínima |
| --- | --- |
| Texto, comentario o documentación sin efecto funcional | ODD directo: inspección y diff |
| Fix o función local con comportamiento comprendido | ODD directo con prueba pertinente |
| Trabajo de varios pasos o que continuará en otra sesión | ODD + una cápsula en `.control/tasks/` |
| Prototipo con resultado aún abierto | Exploración acotada; aceptación definida antes de integrar |
| API de `src/shared`, JSON persistido, rutas/guardado, IDs/materiales, controles, compatibilidad o política cero-red | Contrato breve previo + SDD focalizado |
| Auditoría solicitada, incidente, corrupción o vulnerabilidad | SDD estricto con matriz requisito/verificación |

La ruta o el tamaño del diff son señales, no medidas suficientes de riesgo. Un cambio de
una constante puede alterar persistencia; un movimiento grande de archivos puede no
cambiar comportamiento.

## Ciclo diario ODD

### 1. Entender y preservar

Antes de editar:

1. Lee `AGENTS.md`, la solicitud, el código relevante y sus consumidores.
2. Ejecuta `git status --short --branch` y atribuye los cambios preexistentes.
3. Define objetivo observable, alcance, exclusiones y aceptación.
4. Si hay trabajo paralelo o un árbol sucio ajeno, usa una rama/worktree aislado.

### 2. Explorar con límites

La generación rápida sirve para probar una interacción, algoritmo o estructura. Conserva
una referencia recuperable del inicio. No introduzcas infraestructura especulativa y no
integres el prototipo hasta que cumpla los gates ODD.

### 3. Probar lo que importa

- **Bug determinista:** reproduce RED antes del fix cuando sea viable; después demuestra
  GREEN. Una prueba que también pasa con el fix retirado no es evidencia suficiente.
- **Regla nueva:** cubre el resultado y sus límites relevantes.
- **Refactor:** demuestra preservación con cobertura existente y añade prueba solo ante
  un vacío material.
- **Input/render/UI:** combina prueba determinista con inspección manual exacta; no
  presentes una revisión estática como validación visual.
- **Datos/scripts:** carga el contenido afectado y verifica sidecars, plantillas y saves.
- **Solo documentación:** revisa contenido y diff; no fabriques tests irrelevantes.

### 4. Implementar y revisar

Haz el cambio mínimo conforme a las convenciones existentes. Revisa productores,
consumidores y transiciones de estado, no solo el archivo nombrado. Antes de publicar:

```powershell
git diff --check
git status --short --branch
git diff --stat <base>...HEAD
git diff <base>...HEAD
```

Incluye archivos nuevos y binarios en la revisión. No cambies una prueba válida para
acomodar código incorrecto; si el requisito era erróneo, corrígelo explícitamente.

### 5. Cerrar una unidad revisable

El cierre ODD exige objetivo satisfecho, alcance respetado, gates pertinentes en verde,
diff revisado y límites explícitos. Un único “build OK” no sustituye esa evaluación.

## Gates reales del repositorio

Configura con la ruta del SDK que corresponda al equipo:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 `
  -DRC_OSGVERSE_BUILD=C:/MV3D/osgverse-mv3d/build
```

### `rc_shared_math_grid`

```powershell
cmake --build build --config Release --target rc_shared_math_grid
cmake --build build --config Release --target raycast_standalone
```

La segunda línea es obligatoria para cambios de API o comportamiento compartido: el
ejecutable es el consumidor real. Busca además usos de cualquier cabecera modificada.

### `raycast_standalone`

```powershell
cmake --build build --config Release --target raycast_standalone
.\build\Release\raycast_standalone.exe --selftest-chunks
.\build\Release\raycast_standalone.exe --selftest-aim
```

Ejecuta solo los self-tests pertinentes, pero registra cuáles omitiste y por qué. Para
una validación limpia usa un directorio nuevo, por ejemplo `build_verify_<tarea>`, en vez
de borrar un build que pueda pertenecer a otra sesión.

Smoke del contenido por defecto y de la arena:

```powershell
.\build\Release\raycast_standalone.exe
.\build\Release\raycast_standalone.exe data/worlds/arena_120.json
```

Input, captura del ratón, cámara y apariencia requieren pasos manuales reproducibles.
Registra teclas/botones, estado inicial y resultado observado. Si no puede ejecutarse,
marca la validación como `no verificado`.

### Datos y persistencia

- `data/**` es contenido versionado.
- `data/player/*.json` son plantillas iniciales y no deben cambiar al jugar.
- `data/player/save/` es estado local ignorado.
- Un mapa solicitado por argumento debe cargar su sidecar `.meta.json` correspondiente.
- Si una prueba escribe mundo o plantillas, restaura únicamente el fixture propio de la
  tarea de forma consciente; nunca limpies todo el árbol.

## Contratos que disparan SDD

Antes de modificar una de estas superficies, registra comportamiento esperado,
consumidores, compatibilidad, verificación y recuperación:

- API pública, coordenadas o semántica DDA de `src/shared/`.
- Esquemas JSON, IDs de material y formato/orden determinista del mundo.
- Selección de `DataRoot`, sidecars, plantillas y ubicación de partidas.
- Controles o semántica de acciones del jugador.
- Compatibilidad de mapas/partidas existentes.
- La garantía arquitectónica de cero red, sockets y `bridge_server`.

Para una auditoría estricta, crea una matriz:

| ID | Propiedad requerida | Superficie/consumidor | Método | Evidencia | Estado |
| --- | --- | --- | --- | --- | --- |
| R-01 | Requisito exacto | Productor y consumidor | Prueba/revisión | Resultado esperado | Cumple / No cumple / No verificado / No aplica |

Conserva controles positivos y negativos y, cuando aporte valor, una mutación controlada
que demuestre que la prueba detecta el defecto. Fija commit y condiciones del entorno.
No repares un incidente antes de preservar la evidencia inicial, salvo contención urgente
autorizada.

## Cápsula proporcional

Una tarea pequeña usa la solicitud y el resumen final. Una tarea sustancial copia
`.control/TASK_TEMPLATE.md` a `.control/tasks/<slug>.md` y mantiene un único estado vivo.
La carpeta `.control` no impone permisos ni bloquea shell; solo preserva contexto y
evidencia. La aprobación sigue perteneciendo a la persona autorizada y a la revisión
concreta.

## Git hasta GitHub

1. Parte de una base identificada y conserva su SHA en la cápsula cuando exista.
2. Usa nombres descriptivos (`fix/`, `feature/`, `refactor/`, `perf/`, `docs/`).
3. Crea commits cohesivos con asunto imperativo; no ocultes trabajo en binarios.
4. Ejecuta gates, revisa diff y confirma qué archivos se publicarán.
5. Push de la rama y PR mediante `.github/pull_request_template.md`.
6. El PR registra la revisión exacta probada. Un commit posterior invalida esa evidencia
   hasta repetir los gates afectados.
7. Push, PR y merge mantienen autorizaciones separadas. Tras un merge autorizado,
   sincroniza `main`, confirma `main == origin/main` y limpia solo ramas ya integradas.

El ejecutable `raycast_standalone.exe` de la raíz está versionado por decisión del
proyecto. Solo debe actualizarse cuando la entrega lo requiera expresamente; anota de qué
commit/build procede y revisa que no sea un artefacto accidental.

## Escalado y recuperación

- Si el cambio crece, revisa el diff acumulado desde la base, reclasifica riesgo y divide
  unidades cohesivas.
- Tras dos intentos fallidos sobre el mismo síntoma, detén los parches y revisa hipótesis,
  reproducción y entorno.
- No uses `git reset --hard`, `git checkout -- .` ni `git clean -fd` como reacción a un
  fallo o umbral.
- Si un gate necesario no puede ejecutarse, registra `no verificado` y el siguiente paso.
- Una suite verde demuestra solo las propiedades que realmente ejercita.

## Evaluación del flujo

Cada varias tareas, revisa tiempo hasta entrega validada, retrabajo, defectos escapados,
ampliaciones de alcance y utilidad de las cápsulas. Si la documentación se repite sin
ayudar a retomar, redúcela. Si los tests pasan y escapan defectos, mejora la pertinencia
de las comprobaciones antes de añadir más ritual.
