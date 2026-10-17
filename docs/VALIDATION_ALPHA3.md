# Vérification de l'alpha 0.3 — 17 octobre 2026

## Vérifications locales effectuées

- 431 tests C++ réussis, avec AddressSanitizer et UndefinedBehaviorSanitizer. Cela comprend la recherche avec accents, les jeux/archives/références, les icônes du catalogue fourni et la priorité des imports privés sur le catalogue public.
- 7 tests du bot Python et 4 tests FTP simulés réussis.
- 90 fiches communautaires publiées importées, dont 31 jeux ou portages ; 90 icônes lues et converties. Deux références de développement restent disponibles en plus.
- Rendus du C++ natif avec Mesa : accueil, grille de jeux, fiche, confirmation, rubriques et clavier. Erreur OpenGL `0x0` sur les captures produites. Les commandes du parcours de capture ont ensuite été séparées des étapes photographiées pour que Recherche et Ajout montrent leur état après l'entrée.
- QR de la fiche SuperTuxKart décodé depuis la capture par OpenCV : `https://github.com/Marice/stk-ps5`.
- Le binaire PS5 0.3 a compilé dans WSL Kali avec Clang 21.1.8. Le format FSELF et son intégrité ont été vérifiés par l'outil du SDK. Le générateur QR utilise la version C de Project Nayuki, compatible avec l'absence d'exceptions C++ du SDK.

Le disque Windows C: plein a ensuite empêché le redémarrage de WSL et la récupération du dossier de distribution local. Le paquet diffusé doit donc provenir de la compilation GitHub Actions du commit publié. La réussite du build et les empreintes des fichiers livrés doivent être vérifiées avant publication de la préversion.

## Limites du résultat

L'alpha 0.3 n'a pas été testée sur une vraie PS5. Le seul retour matériel fourni par l'utilisateur concerne le démarrage de l'alpha 0.1 sur sa configuration déclarée en 13.60. Ni les projets référencés ni l'installation des PKG ne sont validés par les tests PC.

Le catalogue public provient du dépôt communautaire GPL épinglé dans `assets/catalog-provenance.json`, pas du catalogue commercial PSPKG ni d'un export d'Orbit Store. Les fichiers des jeux ne sont pas intégrés au ZIP PS5 Hub ; les archives proposées restent chez leurs développeurs. Certains projets exigent les données originales du jeu.

Le flux public PS5 Hub est transporté par HTTPS mais n'est pas signé avec les clés de homebrew.page. Les tests unitaires coupent le réseau ; ils vérifient la conservation du cache en cas d'échec. Le test HTTPS réel `host/catalog_smoke.cpp` est optionnel et doit être lancé explicitement après publication du flux. Ne pas assimiler l'étude du protocole à une synchronisation réelle sur console.

L'installation système depuis l'application demeure non implémentée. Les téléchargements natifs restent limités à 2 Gio et nécessitent une confirmation ainsi qu'un SHA-256 valide. La vérification de version de PS5 Hub ne remplace pas son exécutable : la mise à jour reste manuelle.
