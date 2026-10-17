# Installer PS5 Hub (alpha expérimentale)

Le démarrage de l’alpha 0.1 a été signalé par l’utilisateur sur sa PS5 déclarée en **13.60**. L’alpha 0.3 reste à tester. Les versions exactes de HyperHEN et ShadowMountPlus de la console cible sont inconnues. Le dossier fourni est un titre natif expérimental.

## Passer de 0.1 / 0.2 à 0.3

La vérification de version n’effectue pas la mise à jour. Fermez PS5 Hub, décompressez le nouveau ZIP puis remplacez les fichiers de son dossier `PPSA99841` par USB ou FTP. Conservez vos éventuels `assets/library.tsv` et jaquettes ajoutées : le nouveau ZIP n’en contient pas de personnels. Ne supprimez pas le stockage privé `/download0/hub`. Relancez ensuite l’application : le bandeau doit afficher **0.3 ALPHA**.

Pour importer beaucoup de jeux et leurs jaquettes, utilisez le [bot PC](BOT_BIBLIOTHEQUE_FR.md). Il faut d’abord mettre le titre en 0.3 : synchroniser une liste sur l’ancien exécutable 0.1 n’ajoute pas les nouvelles fonctionnalités.

## USB

1. Décompressez `PS5-Hub-00.003.000-experimental.zip` sur votre PC.
2. Copiez le dossier `PPSA99841` dans `homebrew` à la racine de la clé USB, sur un système de fichiers pris en charge par votre chargeur. La structure attendue est `USB:/homebrew/PPSA99841/eboot.bin`, avec `sce_sys`, `sce_module` et `assets` à côté.
3. Démarrez votre environnement homebrew habituel et la version de ShadowMountPlus que vous utilisez déjà. Attendez son analyse des dossiers.
4. Si le titre est enregistré, ouvrez **PS5 Hub** depuis l’accueil. La présence de l’icône ne prouve pas que l’exécutable est compatible.

## FTP, port 2121

Avec un client FTP, connectez-vous à l’adresse de votre PS5, port **2121**, puis copiez le dossier complet dans `/data/homebrew/PPSA99841`. Faites le transfert quand le titre est fermé. Pour éviter un enregistrement incomplet, transférez `sce_sys/param.json` en dernier.

Un script Python vérifie tous les fichiers après transfert et refuse d’écraser un titre existant :

```powershell
python tools/hub-ftp.py dist/PPSA99841 --host ADRESSE_DE_VOTRE_PS5 --port 2121
```

Le script affiche la destination et demande de taper `OUI`. Pour un compte FTP personnalisé, utilisez `--user` et la variable locale `PS5_FTP_PASSWORD`. Ne publiez pas cette variable. Le script ne lance pas l’application. Si un transfert échoue, il laisse un dossier `.hub-…` identifiable ; vérifiez son contenu avant de le supprimer ou de reprendre manuellement. Le script ne contourne pas les permissions du serveur.

## Utilisation

- Croix : ouvrir / confirmer. Rond : retour.
- Croix sur un menu, ou direction droite : passer au contenu. Gauche : revenir aux menus.
- Triangle dans **Homebrew** ou **Applications** : saisir le nom, un lien HTTPS direct, puis un SHA-256 optionnel. Le SHA-256 devient obligatoire pour télécharger.
- Carré : actualiser ; pendant une opération, demander son annulation.
- Paramètres : animations, sons, recherche de mise à jour.

**Jeux** reçoit les PKG personnels ; **Homebrew** et **Applications** reçoivent des archives homebrew. Le catalogue public est actualisé par HTTPS sans remplacer vos imports. L1 recherche, R1 filtre et Options change les favoris. Les fiches proposent un QR vers le développeur. Voir [catalogue partagé](CATALOGUE_PARTAGE_FR.md).

Les téléchargements sont limités à 2 Gio par fichier et à l’espace réellement accessible. Le stockage propre au titre peut être bien plus petit : cette alpha convient aux petits homebrews, pas à une bibliothèque de gros jeux. Un fichier n’apparaît avec son nom final qu’après réussite du transfert et vérification SHA-256.

## Stockage et journaux

Dans l’application, les données sont dans `/download0/hub` :

- `personal.tsv` : catalogue personnel ;
- `shared.tsv` : copie validée du catalogue public ;
- `favorites.txt` : identifiants des favoris ;
- `covers/` : cache des nouvelles jaquettes publiques ;
- `downloads/` : fichiers téléchargés ;
- `hub.log` et `hub.log.1` : résultats des opérations ;
- `settings.bin` : préférences du kit ;
- `dev/` : éventuels relevés du kit en mode développement.

Selon le montage du chargeur, le serveur FTP peut exposer ces données pendant l’exécution sous `/mnt/sandbox/PPSA99841_000/download0/hub`. Ce chemin et les droits d’accès doivent être confirmés sur votre console. Le dossier d’installation correspond à `/app0` dans le titre. Le catalogue fourni est `/app0/assets/catalog.tsv`.

## Installation des autres applications

Cette alpha sait télécharger et vérifier une archive ou un PKG. **L’installation système depuis PS5 Hub n’est pas encore implémentée.** Aucun appel d’installation HyperHEN non documenté n’est inventé. La rubrique Fichiers consulte uniquement les téléchargements du titre ; elle n’est pas encore un explorateur de tout le disque.

Pour une archive homebrew, vérifiez son origine et les instructions de son auteur, extrayez-la sur PC, puis transférez le dossier natif selon sa documentation. Pour un PKG, utilisez votre installateur déjà compatible et testé. Le script FTP de ce projet est destiné aux dossiers natifs structurés comme PS5 Hub, pas à l’installation des PKG.

Si PS5 Hub ne se lance pas, relevez le message exact, le modèle de console, les versions de HyperHEN et ShadowMountPlus et le hash du ZIP. Arrêtez le test si la console ne répond plus ; ne multipliez pas les relances automatiques.

Sources de structure des dossiers : [ShadowMountPlus](https://github.com/jakovasaur/ShadowMountPlus), [kit natif](https://github.com/blackbearreloaded/ps5-native-app-boilerplate). Ces sources ne valident pas PS5 Hub sur 13.60.
