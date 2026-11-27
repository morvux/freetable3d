# Politique de signature numérique / Code signing policy

**Statut : les installateurs ne sont pas signés pour l'instant.**
Une demande de signature gratuite a été déposée auprès de la SignPath Foundation, puis refusée car le
projet n'avait pas encore une notoriété suffisante. Elle pourra être redéposée quand le projet sera
plus connu. Cette page décrit la politique prévue si une signature est mise en place.

Si une signature SignPath est un jour accordée, les installateurs Windows seront signés ainsi :

> Free code signing provided by [SignPath.io](https://signpath.io), certificate by [SignPath Foundation](https://signpath.org)

Le certificat serait alors délivré à la SignPath Foundation, qui est l'éditeur indiqué dans la signature.

## Ce qui serait signé

- `FreeTable3D-<version>-windows-setup.exe` (installateur Windows) et l'exécutable `freetable3d.exe`
  qu'il contient.
- Les bibliothèques tierces libres (Qt, OpenCascade, etc.) sont fournies telles quelles par MSYS2 ;
  elles ne sont pas présentées comme du code de FreeTable3D.
- Les AppImage Linux ne sont pas signées numériquement (usage courant pour ce format).

## Origine des fichiers signés

Les fichiers signés sont fabriqués automatiquement par GitHub Actions
(`.github/workflows/windows.yml`) à partir du dépôt public
<https://github.com/morvux/freetable3d>, sans modification manuelle. Chaque demande de
signature doit être approuvée par le responsable du projet.

## Rôles

| Rôle | Personne |
| --- | --- |
| Auteur (modifications du code) | KREUDER Olivier ([@morvux](https://github.com/morvux)) |
| Relecteur | KREUDER Olivier |
| Approbateur (chaque signature) | KREUDER Olivier |

L'authentification à deux facteurs est activée sur les comptes GitHub et SignPath.

## Vie privée

FreeTable3D n'envoie aucune donnée à l'extérieur : le logiciel fonctionne entièrement en local.

---

*English summary:* Windows installers are currently NOT signed (a SignPath Foundation application was declined for lack of notability; it may be resubmitted later). If accepted, Windows installers would be signed with a certificate issued to the SignPath
Foundation ("Free code signing provided by SignPath.io, certificate by SignPath Foundation"),
built automatically by GitHub Actions from the public repository, with manual approval of each
release by the project owner. FreeTable3D sends no data over the network.
