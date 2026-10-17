# PS5 Hub

Application homebrew **native C++20 / OpenGL** pour PS5, en français. L’alpha 0.2 ajoute une bibliothèque graphique et un bot Windows pour préparer et transférer vos PKG personnels.

**Démarrage de l’alpha 0.1 signalé sur la PS5 13.60 de l’utilisateur**, avec photos du titre sur l’accueil et de l’interface. Cela reste une validation partielle : l’alpha 0.2 et le transfert HyperDPI ne sont pas encore testés sur sa console. Les versions exactes de HyperHEN et ShadowMountPlus restent inconnues.

![Accueil de PS5 Hub, rendu du C++ natif avec Mesa sur PC](docs/images/accueil.jpg)

Capture du rendu natif sur PC ; aucune capture PS5 n’est présentée comme preuve matérielle.

## Fonctionnalités de cette alpha

- Interface sombre, six rubriques : Accueil, Jeux, Homebrew, Applications, Fichiers, Paramètres.
- Grille de jaquettes avec cache par page, images chargées sur un travailleur et rendu OpenGL.
- Bot PC avec fenêtre native : coller une liste complète, importer TXT / CSV / JSON, préparer jusqu’à 5000 fiches et synchroniser les jaquettes par FTP.
- Recherche facultative de suggestions de jaquettes Steam ; images personnalisées locales ou HTTPS.
- Transfert de gros PKG depuis le PC : FTP vérifié ou demande d’installation à HyperDPI après confirmation. Les fichiers ne sont pas convertis.
- Navigation au pavé directionnel / stick, actions DualSense et clavier à l’écran du kit.
- Icône originale et dossier de titre natif `PPSA99841` pour les chargeurs compatibles avec les dossiers homebrew.
- Catalogue de projets libres, avec une archive officielle Homebrew UI Lab et son empreinte de release.
- Ajout de liens personnels vers des archives ou des PKG depuis la manette ; sauvegarde dans le stockage du titre.
- Téléchargement après confirmation, HTTPS vérifié, contrôle SHA-256 obligatoire, annulation et fichiers temporaires.
- Liste des téléchargements, préférences persistantes, journal des erreurs et recherche de version PS5 Hub.
- Scripts de compilation WSL, préparation du ZIP, transfert FTP 2121 vérifié et workflow GitHub Actions.

**Limites :** l’application PS5 ne réalise pas encore d’installation système elle-même et n’explore pas tout le disque. Ses petits téléchargements utilisent le stockage privé. Les gros PKG passent par le bot PC ; une acceptation HyperDPI n’est pas annoncée comme une installation terminée. Aucune collection de jeux commerciaux n’est fournie.

## Importer tous vos liens

Téléchargez le ZIP **PS5-Hub-Bot-PC** dans les [préversions](https://github.com/tomoff1/ps5-hub/releases). Lancez **Bibliotheque-PC.cmd**, collez un lien direct par ligne, préparez la liste puis synchronisez-la avec l’IP locale de votre PS5. Un CSV permet de fournir les noms et les jaquettes. Le [guide du bot](docs/BOT_BIBLIOTHEQUE_FR.md) détaille aussi le transfert PKG et HyperDPI.

PS5 Hub 0.1 vérifie les versions mais ne se met pas à jour automatiquement. Pour passer à 0.2, fermez l’application et remplacez les fichiers du dossier `PPSA99841` par ceux du nouveau ZIP, en conservant vos fichiers personnels. Une synchronisation du bot ajoute les fiches et images ; elle ne remplace pas l’exécutable.

## Compiler et installer

Voir [compilation Windows 11 / WSL](docs/BUILD_WINDOWS_FR.md), [installation USB / FTP](docs/INSTALLATION_FR.md) et [protocole de validation 13.60](docs/COMPATIBILITE_13_60.md).

Dans Ubuntu 24.04 avec les dépendances du guide :

```bash
bash tools/hub-build.sh
```

Le dossier de test est `dist/PPSA99841/`. Le ZIP expérimental et son SHA-256 sont créés dans `dist/`. Un build réussi confirme la compilation et le format ; il ne confirme pas la compatibilité matérielle.

[Résultats des tests locaux 0.2](docs/VALIDATION_ALPHA2.md) : 427 tests C++ réussis, 11 tests Python du bot et des outils FTP, rendus OpenGL vérifiés. Le protocole HyperDPI est vérifié contre un simulateur, sans installation réelle sur console. Le [rapport 0.1](docs/VALIDATION_PC.md) reste disponible pour l’ancien binaire.

## Sources et architecture

`src/concepts/hub.cpp` contient l’écran principal. `src/hub/` contient le modèle du catalogue et le travailleur réseau/disque. Le rendu, les composants, les entrées et le runtime viennent du kit natif amont. Les exemples de la galerie restent dans les sources comme références et tests du kit ; seul PS5 Hub est enregistré et affiché.

Le catalogue personnel et les journaux restent sur la console. Le dépôt public ne contient ni adresse de console, ni identifiants FTP, ni liens personnels, ni jeu commercial.

## Bases open source

- [ps5-homebrew-ui](https://github.com/blackbearreloaded/ps5-homebrew-ui), commit `bbe19054ee2e7ec069254775273aeb8846c4bd3e` : rendu, composants, contrôleur, outils et runtime intégrés.
- [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate), commit `44ae1dab529654eac795d2a7c4f5a0425102b8d7` : référence de packaging et adaptateur libcurl console.
- SDK public ps5-payload-dev, ps5-opengl et PacBrew : dépendances récupérées par les scripts épinglés du kit.

Licence **GPL-3.0-or-later**, avec les licences tierces dans [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Distribuez les sources correspondantes avec les versions binaires et conservez les notices. PS5 Hub est un projet indépendant, sans affiliation avec Sony ou Overdrive ; ses visuels sont originaux.
