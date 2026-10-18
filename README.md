# PS5 Hub

Application homebrew **native C++20 / OpenGL** pour PS5, en français. L’alpha 0.5 fournit 5663 fiches au 18 octobre 2026 : 92 projets et références communautaires et 5571 références PSPKG issues des listes publiques PS4/PS5. Elle ajoute le catalogue Cloudflare, les sources, les jaquettes WebP/JPEG/PNG, la recherche, les filtres PS4/PS5, les favoris et le téléchargement avec reprise. Le bot Windows prépare et transfère vos PKG personnels.

**Démarrage de l’alpha 0.1 signalé sur la PS5 13.60 de l’utilisateur**, avec photos du titre sur l’accueil et de l’interface. Cela reste une validation partielle : l’alpha 0.5 et le transfert HyperDPI ne sont pas encore testés sur sa console. Les versions exactes de HyperHEN et ShadowMountPlus restent inconnues.

![Accueil de PS5 Hub, rendu du C++ natif avec Mesa sur PC](docs/images/accueil.jpg)

Capture de l’alpha 0.3 rendue sur PC ; la 0.5 conserve ce moteur et ajoute le catalogue connecté et les filtres PS4/PS5. Aucune capture PC n’est présentée comme une preuve matérielle PS5.

## Fonctionnalités de cette alpha

- Interface sombre, six rubriques : Accueil, Jeux, Homebrew, Applications, Téléchargements, Paramètres.
- Grille de jaquettes avec cache par page, images chargées sur un travailleur et rendu OpenGL.
- Bot PC avec fenêtre native : coller une liste complète, importer TXT / CSV / JSON, préparer jusqu’à 5000 fiches et synchroniser les jaquettes par FTP.
- Recherche facultative de suggestions de jaquettes Steam ; images personnalisées locales ou HTTPS.
- Transfert de gros PKG depuis le PC : FTP vérifié ou demande d’installation à HyperDPI après confirmation. Les fichiers ne sont pas convertis.
- Navigation au pavé directionnel / stick, actions DualSense et clavier à l’écran du kit.
- Icône originale et dossier de titre natif `PPSA99841` pour les chargeurs compatibles avec les dossiers homebrew.
- Catalogue fourni avec le ZIP pour fonctionner hors ligne, actualisé au démarrage puis toutes les 15 minutes depuis Cloudflare. Filtres Tous / Favoris / Téléchargeables / PS4 / PS5 ; provenance et état des sources dans les paramètres.
- Fiches sans QR : « Ouvrir sur PS5 » appelle le navigateur système par l’API du SDK. Ouverture et navigation réelles sur 13.60 restent à tester ; aucune session Web n’est partagée avec libcurl. Les fichiers directs autorisés avec SHA-256 restent téléchargeables dans le Hub.
- Favoris sauvegardés sur la console ; les imports personnels restent prioritaires et privés.
- Ajout de liens personnels vers des archives ou des PKG depuis la manette ; sauvegarde dans le stockage du titre.
- Téléchargement après confirmation, HTTPS vérifié, SHA-256 obligatoire, progression et vitesse, suspension/reprise et publication atomique. Inspection CNT/FIH pour les fichiers PKG ; taille et identité vérifiées lorsqu’elles sont fournies.
- File persistante de téléchargements, pause, reprise HTTP, annulation, vitesse, taille et durée restante estimée. Historique de 200 contenus maximum ; les travaux interrompus sont restaurés en pause au redémarrage. Préférences et journal des erreurs conservés.
- Détection d’etaHEN DPIv2 sur 12800, inspection CNT/FIH et nouvelle vérification SHA-256 avant envoi multipart du PKG local, après confirmation. Un port 9090 ouvert est signalé sans lui envoyer de commande. Demande acceptée, installation terminée et jeu jouable sont des états distincts.
- Scripts de compilation WSL, préparation du ZIP, transfert FTP 2121 vérifié et workflow GitHub Actions.

**Limites :** navigateur système externe au Hub, sans moteur Web embarqué ni interception des liens ou cookies. L’envoi DPIv2 fonctionne avec le simulateur local ; aucun essai sur console n’a validé le navigateur, l’installation ou les téléchargements 0.5 sur 13.60. Le contrôle d’espace vérifie le stockage accessible et la copie de staging, pas la taille décompressée, les licences, les clés ou le firmware minimum d’un jeu. Pas de suivi de fin d’installation, de téléchargement en arrière-plan système ni de remplacement automatique du binaire. [Fonctionnement et limites de la 0.5](docs/TELECHARGEMENTS_NATIFS_FR.md). Aucune collection de jeux commerciaux n’est fournie.

Le [guide du catalogue partagé](docs/CATALOGUE_PARTAGE_FR.md) conserve le protocole historique 0.3 pour les anciens clients. Aucun téléchargement commercial PSPKG n’est intégré.

Le [service de catalogue Cloudflare](docs/CLOUDFLARE_FR.md) fournit une administration protégée, des connecteurs indépendants et le flux natif v2 utilisé par 0.5. Les fiches PSPKG n’exposent aucun lien PKG et ne constituent pas des jeux distribués : elles affichent une référence et un état de vérification. Les jaquettes distantes sont chargées à la demande ; elles ne sont pas incluses dans le ZIP. Publier le serveur ne remplace pas l’exécutable déjà installé sur la console.

## Importer tous vos liens

Téléchargez le ZIP **PS5-Hub-Bot-PC** dans les [préversions](https://github.com/tomoff1/ps5-hub/releases). Lancez **Bibliotheque-PC.cmd**, collez un lien direct par ligne, préparez la liste puis synchronisez-la avec l’IP locale de votre PS5. Un CSV permet de fournir les noms et les jaquettes. Le [guide du bot](docs/BOT_BIBLIOTHEQUE_FR.md) détaille aussi le transfert PKG et HyperDPI.

PS5 Hub 0.1 vérifie les versions mais ne se met pas à jour automatiquement. Pour passer à 0.5, fermez l’application et remplacez les fichiers du dossier `PPSA99841` par ceux du nouveau ZIP, en conservant vos fichiers personnels. Une synchronisation du bot ajoute les fiches et images ; elle ne remplace pas l’exécutable.

## Compiler et installer

Voir [compilation Windows 11 / WSL](docs/BUILD_WINDOWS_FR.md), [installation USB / FTP](docs/INSTALLATION_FR.md) et [protocole de validation 13.60](docs/COMPATIBILITE_13_60.md).

Dans Ubuntu 24.04 avec les dépendances du guide :

```bash
bash tools/hub-build.sh
```

Le dossier de test est `dist/PPSA99841/`. Le ZIP expérimental et son SHA-256 sont créés dans `dist/`. Un build réussi confirme la compilation et le format ; il ne confirme pas la compatibilité matérielle.

[Validation de la 0.5](docs/VALIDATION_ALPHA4.md). Le [rapport 0.3](docs/VALIDATION_ALPHA3.md) reste disponible. Le [rapport 0.2](docs/VALIDATION_ALPHA2.md) et le [rapport 0.1](docs/VALIDATION_PC.md) restent disponibles pour les anciens binaires. Aucun test PC ne valide le firmware de la console.

## Sources et architecture

`src/concepts/hub.cpp` contient l’écran principal. `src/hub/` contient le modèle du catalogue et le travailleur réseau/disque. Le rendu, les composants, les entrées et le runtime viennent du kit natif amont. Les exemples de la galerie restent dans les sources comme références et tests du kit ; seul PS5 Hub est enregistré et affiché.

Le catalogue personnel et les journaux restent sur la console. Le dépôt public ne contient ni adresse de console, ni identifiants FTP, ni liens personnels, ni jeu commercial.

## Bases open source

- [ps5-homebrew-ui](https://github.com/blackbearreloaded/ps5-homebrew-ui), commit `bbe19054ee2e7ec069254775273aeb8846c4bd3e` : rendu, composants, contrôleur, outils et runtime intégrés.
- [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate), commit `44ae1dab529654eac795d2a7c4f5a0425102b8d7` : référence de packaging et adaptateur libcurl console.
- SDK public ps5-payload-dev, ps5-opengl et PacBrew : dépendances récupérées par les scripts épinglés du kit.

Licence **GPL-3.0-or-later**, avec les licences tierces dans [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Distribuez les sources correspondantes avec les versions binaires et conservez les notices. PS5 Hub est un projet indépendant, sans affiliation avec Sony ou Overdrive ; son identité visuelle est originale et les icônes des projets sont attribuées à leurs développeurs.
