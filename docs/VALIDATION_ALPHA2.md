# Vérification de l’alpha 0.2 — 17 octobre 2026

- **427 tests C++ réussis** avec AddressSanitizer et UndefinedBehaviorSanitizer ; les nouveaux tests couvrent le catalogue importé et les images tronquées.
- **14 tests ciblés réussis** après ajout du parcours de la rubrique Jeux et de sa pagination.
- **7 tests Python du bot réussis**, dont import en masse, doublons, CSV français et image locale, préservation d’une bibliothèque après import invalide, protocole HyperDPI simulé et refus d’un service inconnu.
- **4 tests FTP simulés réussis**, conservés depuis l’alpha 0.1.
- La fenêtre Windows du bot a été construite dans un test de démarrage sans affichage.
- Rendus du C++ natif sur Mesa : accueil illustré, grille de jeux, autre page de jaquettes, clavier et confirmation. Erreur GL `0x0`. Une bibliothèque de 18 fiches fictives est utilisée pour les tests de pagination et reste dans `build/`, jamais dans le paquet livré.
- Build PS5 natif effectué avec le SDK public et Clang 21.1.8 dans WSL Kali. Le runtime et le format FSELF passent leur contrôle d’intégrité.

**Matériel :** l’utilisateur a signalé le fonctionnement de l’alpha 0.1 sur sa PS5 déclarée en firmware 13.60 et fourni deux photos dans le chat. Elles montrent l’enregistrement du titre sur l’accueil et son interface démarrée. Elles ne constituent pas une validation complète des téléchargements, des versions des chargeurs ou du transfert HyperDPI. **L’alpha 0.2 reste à tester sur cette console.**

Le bot ne dispose d’aucun jeu ni lien personnel de l’utilisateur à ce stade. Il importe la liste que celui-ci lui donne ; les tests ne téléchargent aucun PKG commercial. La recherche facultative de jaquettes fournit des suggestions visuelles, sans prouver l’édition ou la compatibilité d’un jeu.

Le protocole HyperDPI a été étudié dans le [client public de HyperHEN](https://github.com/Michele-M-Media/HYPERHEN/blob/main/third_party/hyperdpi/webui/src/App.jsx) et son serveur `pkg_server.c`. Les tests simulent les réponses `/ping`, les blocs avec `head256` et la finalisation. Aucun envoi à une vraie console n’a été effectué par l’agent ; aucune installation n’est annoncée comme terminée.
