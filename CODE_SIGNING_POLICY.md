# Politique de signature numérique / Code signing policy

**Statut : candidature en préparation auprès de la SignPath Foundation.**
Une fois la candidature acceptée, les installateurs Windows de FreeTable3D seront signés ainsi :

> Free code signing provided by [SignPath.io](https://signpath.io), certificate by [SignPath Foundation](https://signpath.org)

Le certificat est délivré à la SignPath Foundation, qui est l'éditeur indiqué dans la signature.

## Ce qui est signé

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

*English summary:* Windows installers will be signed with a certificate issued to the SignPath
Foundation ("Free code signing provided by SignPath.io, certificate by SignPath Foundation"),
built automatically by GitHub Actions from the public repository, with manual approval of each
release by the project owner. FreeTable3D sends no data over the network.
