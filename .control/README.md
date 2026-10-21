# Cápsulas de trabajo

`.control/` es un mecanismo documental, no un sandbox ni un sistema automático de
permisos. Su presencia no autoriza cambios, pushes o merges y no impide escrituras fuera
del alcance declarado.

Para una tarea sustancial, copia `TASK_TEMPLATE.md` a `tasks/<slug>.md`, completa el
estado y mantenla viva hasta el cierre. Una tarea pequeña que cabe en una sesión no
necesita cápsula.

Reglas:

- Una cápsula por trabajo; no dupliques la misma información en `docs/` o `specs/`.
- Registra la referencia inicial y distingue cambios preexistentes de los de la tarea.
- La evidencia identifica el comando o revisión y su resultado, no solo “OK”.
- `terminada` significa que se cumplieron los criterios aplicables; si falta un gate,
  usa `bloqueada` o registra expresamente `no verificado`.
- Elimina o archiva la cápsula al cerrar según aporte valor histórico. Los contratos y
  decisiones duraderas pertenecen a `docs/`, no a una cápsula temporal.
