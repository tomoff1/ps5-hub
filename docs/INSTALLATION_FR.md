# Installer PS5 Hub (alpha expérimentale)

Le démarrage de l’alpha 0.1 a été signalé par l’utilisateur sur sa PS5 déclarée en **13.60**. L’alpha 0.6 reste à tester. Les versions exactes de HyperHEN et ShadowMountPlus de la console cible sont inconnues. Le dossier fourni est un titre natif expérimental.

## Passer de 0.1 / 0.2 / 0.3 / 0.6 à 0.6

La vérification de version n’effectue pas la mise à jour. Fermez PS5 Hub, décompressez le nouveau ZIP puis remplacez les fichiers de son dossier `PPSA99841` par USB ou FTP. Conservez vos éventuels `assets/library.tsv` et jaquettes ajoutées : le nouveau ZIP n’en contient pas de personnels. Ne supprimez pas le stockage privé `/download0/hub`. Relancez ensuite l’application : le bandeau doit afficher **0.6 ALPHA**.

Pour importer beaucoup de jeux et leurs jaquettes, utilisez le [bot PC](BOT_BIBLIOTHEQUE_FR.md). Il faut d’abord mettre le titre en 0.6 : synchroniser une liste sur l’ancien exécutable 0.1 n’ajoute pas les nouvelles fonctionnalités.

Le paquet 0.6 contient 5663 fiches : 92 projets communautaires et 5571 références
PSPKG issues des listes publiques parcourues le 18 octobre 2026. Ce sont des
fiches, pas des jeux fournis avec l’application. Une référence PSPKG affiche sa
source, sa jaquette en ligne et son état de vérification ; aucun lien PKG commercial
n’a été extrait. Le bouton de téléchargement est réservé aux fichiers directs
avec autorisation et SHA-256. Les images publiques manquantes restent des
illustrations de remplacement ; WebP, JPEG et PNG sont décodés sur le travailleur.
Le titre se synchronise au démarrage puis toutes les quinze minutes lorsqu’il
est ouvert et qu’aucune autre opération n’occupe le travailleur.

## USB

1. Décompressez `PS5-Hub-00.006.000-experimental.zip` sur votre PC.
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
- Carré hors fiche : actualiser, ou suspendre l’opération en cours. Dans Téléchargements, Carré annule le transfert sélectionné et supprime sa copie partielle ; il conserve les fichiers finaux.
- Dans Téléchargements, Croix met en pause ou reprend ; sur un PKG terminé, Croix propose l’installation après détection d’etaHEN.
- Dans une fiche, Triangle ouvre le navigateur système PS5 ; Croix lance le fichier direct autorisé ou explique le blocage sans ouvrir de page Web ; Carré propose l’installation d’un PKG local, ou Télécharger et installer si DPIv2 est identifié.
- Paramètres > Détecter etaHEN DPIv2 : identification du service 12800, sans installation. Le port 9090 est seulement sondé par connexion TCP, sans commande.
- Paramètres : animations, sons, recherche de mise à jour.

**Jeux** reçoit les PKG personnels ; **Homebrew** et **Applications** reçoivent des archives homebrew. Le catalogue public est actualisé par HTTPS sans remplacer vos imports. L1 recherche, R1 filtre Tous / Favoris / Téléchargeables / PS4 / PS5 et Options change les favoris. Les fiches proposent « Ouvrir sur PS5 » via le navigateur système ; ouverture et commandes du navigateur sur 13.60 restent à tester. Les protections et connexions s’effectuent dans le navigateur, dont les cookies ne sont pas partagés avec le Hub. Voir [catalogue partagé](CATALOGUE_PARTAGE_FR.md).

Les téléchargements natifs acceptent au maximum 250 Gio par fichier, dans la limite de l’espace réellement accessible. Le quota du stockage privé sur PS5 reste à mesurer : cette limite logicielle ne garantit pas que de gros jeux y tiennent. Une interruption conserve les fichiers de reprise ; choisissez Croix dans Téléchargements pour reprendre. La file et son historique sont conservés dans transfers.tsv ; après fermeture du titre, une reprise manuelle est nécessaire. Un fichier n’apparaît avec son nom final qu’après réussite du transfert et vérification SHA-256.

## Stockage et journaux

Dans l’application, les données sont dans `/download0/hub` :

- `personal.tsv` : catalogue personnel ;
- `shared-v2.tsv` : copie validée du catalogue Cloudflare (l’ancien `shared.tsv` n’est plus utilisé) ;
- `favorites.txt` : identifiants des favoris ;
- `covers/` : cache des nouvelles jaquettes publiques ;
- `downloads/` : fichiers téléchargés et copies partielles ;
- `transfers.tsv` : file et historique privés (contient vos URL : ne pas publier ce fichier) ;
- `hub.log` et `hub.log.1` : résultats des opérations ;
- `network.log` et `network.log.1` : codes HTTP/libcurl/TLS, redirections et erreurs disque, sans URL ni jeton ;
- `settings.bin` : préférences du kit ;
- `dev/` : éventuels relevés du kit en mode développement.

Selon le montage du chargeur, le serveur FTP peut exposer ces données pendant l’exécution sous `/mnt/sandbox/PPSA99841_000/download0/hub`. Ce chemin et les droits d’accès doivent être confirmés sur votre console. Le dossier d’installation correspond à `/app0` dans le titre. Le catalogue fourni est `/app0/assets/catalog.tsv`.

## Installation des autres applications

Cette alpha télécharge et vérifie les fichiers directs autorisés. Pour un PKG, elle propose un envoi au service etaHEN DPIv2 identifié, après confirmation et contrôles locaux ; le résultat final doit être vérifié dans les notifications système. La rubrique Téléchargements consulte uniquement les fichiers du Hub ; elle n’est pas un explorateur de tout le disque.

Pour une archive homebrew, vérifiez son origine et les instructions de son auteur, extrayez-la sur PC, puis transférez le dossier natif selon sa documentation. Sans service DPIv2 compatible, utilisez votre installateur déjà testé pour les PKG. Le script FTP de ce projet est destiné aux dossiers natifs structurés comme PS5 Hub, pas à l’installation des PKG.

Si PS5 Hub ne se lance pas, relevez le message exact, le modèle de console, les versions de HyperHEN et ShadowMountPlus et le hash du ZIP. Arrêtez le test si la console ne répond plus ; ne multipliez pas les relances automatiques.

Sources de structure des dossiers : [ShadowMountPlus](https://github.com/jakovasaur/ShadowMountPlus), [kit natif](https://github.com/blackbearreloaded/ps5-native-app-boilerplate). Ces sources ne valident pas PS5 Hub sur 13.60.

## Navigateur et installation etaHEN

Voir [le guide natif 0.6](TELECHARGEMENTS_NATIFS_FR.md). Le navigateur système est une tentative d’ouverture, pas un WebView intégré garanti. La bibliothèque des téléchargements peut transmettre un PKG validé à etaHEN DPIv2. L’espace de staging `/user/data/tmp` doit être accessible et suffisant ; faute de pouvoir le vérifier, l’envoi est refusé. L’espace final de l’application et son firmware minimum restent à la charge du système d’installation. Aucun test matériel de la 0.6 n’a encore été réalisé.

## Contrôles 0.6

X sur une fiche directe autorisée démarre le téléchargement sans ouvrir le navigateur. X sur une référence propose les conditions et le filtre Téléchargeables ; Triangle seul ouvre la source. Dans Téléchargements, Triangle affiche le diagnostic. Sur une demande acceptée, X permet de confirmer manuellement la fin d’installation puis supprimer le PKG du Hub. Ce contrôle ne prouve pas que l’installation est réussie : vérifiez les notifications système. Aucun fichier partagé d’etaHEN n’est supprimé.
