> Protocole du client 0.3. Pour le catalogue Cloudflare et les sources PS4/PS5 du client 0.4, voir [le guide Cloudflare](CLOUDFLARE_FR.md).

# Catalogue et interface 0.3

La version 0.3 fournit 90 projets publiés dans le catalogue communautaire PS5, dont 31 jeux ou portages, ainsi que deux références de développement conservées. Le catalogue est disponible dès le premier lancement, avec 90 icônes des développeurs. Les utilisateurs n'ont pas besoin de préparer une liste pour voir ces projets. **Il ne contient pas le catalogue commercial de PSPKG.**

L'accueil met en avant le jeu sélectionné. La rubrique Jeux regroupe les jeux et portages homebrew avec les PKG que vous importez personnellement. Homebrew et Applications présentent les autres projets. Les filtres et la recherche restent actifs quand vous changez de rubrique.

- Croix : entrer dans la grille, puis ouvrir une fiche.
- Cercle : fermer la fiche ou revenir au menu latéral.
- L1 : rechercher ; effacez le texte puis validez pour tout retrouver. Les accents français courants et la casse sont ignorés.
- R1 : changer de filtre, entre Tous, Favoris et Archives.
- Options : ajouter ou retirer le contenu sélectionné des favoris. Ils sont enregistrés dans `/download0/hub/favorites.txt`.
- Triangle : ajouter un lien personnel dans Jeux, Homebrew ou Applications.
- Carré : annuler une opération en cours, ou recharger la bibliothèque et actualiser le catalogue partagé. Dans Fichiers, recharge seulement les téléchargements locaux.

Chaque fiche propose un QR vers le dépôt du développeur, lisible avec l'appareil photo du téléphone. Les titres non hébergés sur GitHub utilisent leur URL telle quelle. Les QR de liens personnels peuvent donc contenir leurs paramètres privés : ne publiez pas de capture de ces fiches.

## Téléchargement et compatibilité

Une entrée « ARCHIVE » désigne un ZIP publié par le développeur avec une empreinte SHA-256. La console demande confirmation avant le téléchargement, vérifie cette empreinte et garde le résultat dans `/download0/hub/downloads`. La limite reste de 2 Gio par fichier. Une fiche « PROJET » fournit une référence sans téléchargement natif.

**Télécharger ne signifie pas installer.** Le hub n'extrait pas les archives et n'installe pas de PKG dans le système. Scannez le QR et consultez les instructions du projet pour les dépendances, les données originales éventuelles et l'installation. Un moteur libre ou un portage ne fournit pas automatiquement les données du jeu commercial. Aucun jeu commercial ni ROM n'est livré dans PS5 Hub.

La présence d'une entrée dans le catalogue ne valide ni sa sécurité, ni ses données, ni sa compatibilité avec le firmware 13.60. Le champ de licence vient de la fiche communautaire ; il décrit le projet référencé et ne remplace pas ses notices. La version 0.3 de PS5 Hub doit elle aussi être testée sur console.

## Catalogue commun à tous

La PS5 reçoit le même catalogue fourni avec le ZIP. Au démarrage sur console, puis lors d'une actualisation, elle consulte `https://raw.githubusercontent.com/tomoff1/ps5-hub/main/shared/catalog.tsv`. La bibliothèque locale est visible pendant cette opération. Une erreur réseau ou un contenu invalide conserve les données précédentes. La copie validée est enregistrée dans `/download0/hub/shared.tsv`.

Les imports `assets/library.tsv`, puis `/download0/hub/personal.tsv`, restent prioritaires sur les entrées publiques de même identifiant. Les favoris restent locaux. Les identifiants privés ne sont jamais envoyés au serveur de jaquettes. Une nouvelle jaquette publique absente du paquet est téléchargée depuis le même dépôt GitHub et contrôlée pour son format et ses dimensions. Les icônes déjà présentes dans le paquet restent celles de cet instantané.

Le flux est transporté par HTTPS depuis le dépôt de l'éditeur. Il n'utilise pas la signature de l'API de homebrew.page : **ne pas le présenter comme un catalogue signé ou comme une validation par ce service**. Il contient uniquement des données ; il ne remplace aucun exécutable.

## Maintenir et publier

Les fiches viennent de [ps5-homebrew-catalog](https://github.com/blackbearreloaded/ps5-homebrew-catalog), publié sous GPL-3.0-or-later. Le commit utilisé, les fiches et les URL d'origine sont conservés dans `assets/catalog-provenance.json`. Les archives restent chez leurs développeurs ; le script ne les télécharge pas et ne les réhéberge pas. Les réservations de titres sans publication sont exclues.

Sur PC, après installation de Pillow :

```sh
python tools/hub-community-catalog.py --commit ab6d9911ac82e56c73b8390131b93a58ce0545ed
```

Le script génère `assets/catalog.tsv`, `shared/catalog.tsv`, les icônes `.hubart` et la provenance. Relire les changements, exécuter les tests, puis publier ces fichiers sur le dépôt. Une nouvelle entrée de jeu est de type `game` : SHA-256 vide pour une fiche de référence ; SHA-256 renseigné pour une archive ZIP du développeur. Le bot conserve le type `pkg` pour les PKG personnels.

Publier une nouvelle fiche et son icône dans le dépôt rend son catalogue accessible à toutes les consoles qui actualisent. La mise à jour de l'exécutable PS5 Hub reste manuelle par USB ou FTP ; elle demande le nouveau ZIP.
