# Vérifications de l’alpha 0.1

Date : **17 octobre 2026**. Environnement local : Windows 11, WSL2 Kali Linux, Clang/LLVM 21.1.8, Mesa llvmpipe 26.0.6. Les commandes de compilation Ubuntu 24.04 / Clang 18 sont fournies séparément et exécutables dans GitHub Actions.

| Vérification | Résultat local |
|---|---|
| Suite C++ complète | **425 tests réussis**, 59 suites, AddressSanitizer et UndefinedBehaviorSanitizer |
| Derniers changements du catalogue et du parcours UI | **12 tests ciblés réussis** après la suite complète |
| Outils FTP, serveur simulé | **4 tests réussis** : contrôle des hashes, publication du param.json en dernier, refus d’écrasement et identifiant invalide |
| Rendu OpenGL sur PC | 8 PNG, dont les cinq rubriques, le clavier et la confirmation ; erreur GL `0x0` pour chaque capture |
| Inspection visuelle | Accueil, rubriques, clavier et confirmation ouverts et vérifiés |
| HTTPS réel sur PC | Archive officielle Homebrew UI Lab téléchargée et vérifiée ; deuxième téléchargement rejeté avec une mauvaise empreinte ; fichier temporaire absent après rejet |
| Build PS5 | Compilation et lien réussis ; dossier `PPSA99841` généré |
| Runtime | Reconstruction reproductible du runtime du kit ; hash enregistré par l’amont vérifié |
| Format de l’exécutable | Inspection FSELF : intégrité valide |
| ZIP de distribution | CRC interne, SHA-256 et permissions Unix 0777 des 112 fichiers vérifiés |
| Console PS5 / firmware 13.60 | **Non testé** |
| Installation système des PKG dans le hub | **Non implémentée** |

SHA-256 du ZIP local publié `PS5-Hub-00.001.000-experimental.zip` :

```text
84644eb881b1d2757aa79392936c07c062f2cddb8da22028bbaa1e30b5162217
```

L’archive attachée à la préversion correspond au code du tag `v0.1.0-alpha.1`. Un build avec un autre compilateur ou une autre date peut avoir une empreinte différente ; ne remplacez pas l’empreinte d’une archive existante par celle d’un autre build.

Les logs détaillés restent dans `build/` et ne sont pas publiés avec des chemins personnels. Les captures du README sont des rendus du programme natif sur PC. Le workflow distant est une vérification supplémentaire : son statut doit être consulté dans GitHub Actions, et n’est pas déduit de ces résultats locaux.
