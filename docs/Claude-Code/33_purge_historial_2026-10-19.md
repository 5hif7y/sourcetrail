# Purga de historial: `error.log` y `build_log.txt`

**Fecha:** 2026-10-19
**Aprobación:** explícita del usuario (`tenes la aprobacion para purgar`).
**Árbol canónico:** `_work\modernized`
**Fork:** `git@github.com:5hif7y/sourcetrail.git`, rama `modernized`

---

## 1. Auditoría previa

Antes de reescribir se verificó dónde estaban realmente los blobs:

- `origin/modernized` (28 commits) y `origin/master` (2.761 commits) **no
  contienen** `error.log` ni `build_log.txt`: `git log --all -- '*error.log'
  '*build_log.txt'` y `git rev-list --objects --all` no devuelven nada.
- Escaneo de **29.655 blobs** alcanzables por todas las refs remotas, en UTF-8
  y UTF-16: **0** apariciones de los identificadores personales del usuario.
- Aparecen 14 rutas upstream con rutas de usuario genéricas en datos de test
  (`vsSolutionParserTestSuite`, `test_log.txt`, un `.suo`, un `.jar` y un
  `.VC.db`). No contienen identificadores del usuario y son parte del
  historial upstream; no se purgaron.
- Los dos logs sí estaban en el historial **local y sin remoto** de
  `Sourcetrail-profundo\neo-sourcetrail`: se agregaron en el commit WIP
  `20ffe0b` y sus padres no los tenían.

Por eso no había nada que force-pushear: reescribir las ramas publicadas sólo
habría cambiado hashes sin eliminar ningún blob.

---

## 2. Purga local

En `neo-sourcetrail`, sin tocar el árbol de trabajo sucio:

```powershell
git rm --cached error.log build_log.txt
git commit --amend --no-edit
git reflog expire --expire=now --all
git gc --prune=now
```

- HEAD local: `20ffe0b` → **`eae59f6`**.
- El amend sólo quitó los dos archivos del índice; los cambios sin commitear
  del árbol de build se conservaron.
- Verificación: no hay `error.log`/`build_log.txt` en `git log --all`,
  `git ls-tree -r HEAD` ni `git rev-list --objects --all`; el reflog y los
  objetos viejos se purgaron.
- `neo-sourcetrail` no tiene remoto para `master`, así que no hubo push.

---

## 3. Remoto

- `origin/modernized`: **sin cambios** (no contenía los blobs).
- `origin/master`: **sin cambios** (tampoco).
- No se hizo force-push. Si GitHub conserva objetos inalcanzables de pushes
  anteriores, no son accesibles por refs y no se pueden eliminar con un
  force-push; requerirían la recolección de basura del servidor.

---

## 4. Verificación

```powershell
# En neo-sourcetrail:
git log --all --oneline -- '*error.log' '*build_log.txt'   # vacío
git rev-list --objects --all | Select-String 'error\.log|build_log'  # vacío
git log -1 --oneline                                        # eae59f6

# En _work\modernized:
git log --all --oneline -- '*error.log' '*build_log.txt'    # vacío
git rev-list --objects --all | Select-String 'error\.log|build_log'  # vacío
```

Escaneo de blobs remotos: 0 identificadores personales; quedan únicamente los
14 paths upstream con rutas de usuario genéricas.

---

## 5. Estado

T3 (purge de historial) queda **cerrado**. No hay force-push pendiente y el
remoto publicado no contiene los logs ni identificadores del usuario.
