# Orden de trabajo del repositorio

Este archivo aplica a todo el repositorio. La política completa y sus motivos están en
[`docs/WORKFLOW_ODD_SDD.md`](docs/WORKFLOW_ODD_SDD.md).

## Componentes bajo esta orden

1. `rc_shared_math_grid`: biblioteca estática de matemáticas, rutas y DDA en
   `src/shared/`.
2. `raycast_standalone`: ejecutable y datos locales en `src/`, `data/` y
   `config.json`; consume `rc_shared_math_grid`.

`thirdparty/luau/` es código vendorizado. No se modifica salvo que la tarea sea
explícitamente actualizar o reparar esa dependencia.

## Flujo obligatorio

Aplica ODD proporcional como flujo habitual:

1. Antes de editar, lee la solicitud, estas instrucciones, el código relevante, sus
   consumidores y `git status`. Identifica y preserva cambios preexistentes.
2. Define el resultado observable, el alcance, lo que queda fuera y la comprobación
   pertinente. Para trabajo pequeño no fabriques documentos de planificación.
3. Usa exploración rápida solo con objetivo y límites claros. Un prototipo no queda
   aceptado hasta superar inspección, prueba y revisión del diff.
4. Para un bug determinista, reproduce RED antes de corregir cuando sea viable. Verifica
   GREEN y demuestra que la prueba habría detectado el defecto.
5. Implementa la solución más pequeña que cumpla el objetivo. Revisa el diff completo,
   incluidos archivos nuevos, staged y binarios.
6. Si cambia un contrato sensible, fija primero el comportamiento esperado y usa SDD
   focalizado. Son sensibles: formatos JSON persistidos, rutas de datos, guardado,
   materiales/IDs, API de `src/shared`, controles, compatibilidad y la garantía de cero
   red/sockets.
7. Cierra indicando resultado, evidencia ejecutada y límites. No declares verificado lo
   que solo se haya inspeccionado.

Para trabajo sustancial o que deba continuar en otra sesión, crea una única cápsula desde
`.control/TASK_TEMPLATE.md` en `.control/tasks/<slug>.md`. No la dupliques en otros
directorios.

## Gates por componente

### Cambios en `rc_shared_math_grid`

- Compila `rc_shared_math_grid`.
- Compila también `raycast_standalone`, porque es su consumidor directo.
- Ejecuta el self-test o reproducción que cubra la regla modificada.
- Si cambia una cabecera pública, revisa todos sus consumidores con `rg`.

### Cambios en `raycast_standalone`

- Compila `raycast_standalone`.
- Ejecuta `--selftest-chunks` si afecta grid, voxels, malla, materiales o X-Ray.
- Ejecuta `--selftest-aim` si afecta balística, movimiento de objetivos o proyectiles.
- Para input, cámara, HUD o render, añade una comprobación determinista cuando sea
  posible y registra además la prueba manual exacta que falta o se realizó.
- Para datos o scripts, arranca el mapa afectado y confirma que plantillas versionadas y
  `data/player/save/` no se mezclan.

Los comandos canónicos están en `docs/WORKFLOW_ODD_SDD.md`.

## Git y GitHub

- Trabaja desde una base conocida. Usa rama o worktree aislado cuando haya trabajo
  paralelo o cambios locales ajenos.
- No uses `reset --hard`, `checkout -- .` ni `clean -fd` para resolver alcance o fallos.
- No incluyas builds, logs ni partidas. El ejecutable raíz solo se actualiza cuando la
  entrega lo exige explícitamente y debe provenir del commit validado.
- Crea commits cohesivos con asunto imperativo. No mezcles refactor, comportamiento y
  datos sin una razón revisable.
- Antes del push: gates pertinentes, `git diff --check`, revisión de `git diff` y estado
  limpio salvo cambios del usuario identificados.
- El PR debe indicar componente, actividad (exploración/ODD/SDD), alcance, evidencia y
  límites. Push y merge requieren autorización; no confundas un push con aceptación.
- Tras el merge, sincroniza `main` y confirma que coincide con `origin/main`.

## Escalado

- Si el alcance crece, reclasifica el riesgo y divide unidades cohesivas.
- Tras dos correcciones fallidas del mismo síntoma, detén los parches y revisa la
  reproducción y la hipótesis.
- Ante corrupción, vulnerabilidad o incidente, preserva evidencia y abre una auditoría
  SDD antes de reparar, salvo contención urgente expresamente autorizada.
- Si un gate necesario no puede ejecutarse, registra `no verificado`; no lo sustituyas
  por una afirmación de éxito.
