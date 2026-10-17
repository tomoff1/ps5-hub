# Votre bibliothèque de jeux, en une seule importation

Le bot PC organise **vos liens directs** et prépare des jaquettes pour PS5 Hub 0.2. Il ne modifie pas le contenu des PKG et ne convertit pas un jeu incompatible en jeu compatible. Il ne contient pas de liste de téléchargement de jeux commerciaux.

## Le plus simple sous Windows

1. Décompressez le ZIP **PS5-Hub-Bot-PC** dans un dossier où vous pouvez écrire.
2. Si nécessaire, installez Python, puis ouvrez un terminal dans ce dossier et lancez `python -m pip install -r requirements-bot.txt`.
3. Double-cliquez sur **Bibliotheque-PC.cmd**.
4. Collez tous vos liens, **un par ligne**, puis cliquez sur **Préparer les liens collés**. Le bot traite toute la liste ; vous n’avez pas à ajouter chaque jeu sur la manette.
5. Pour des noms propres et vos propres jaquettes, utilisez plutôt un fichier CSV. Cliquez sur **Ouvrir TXT / CSV / JSON**.
6. Installez PS5 Hub **0.2**, fermez le titre, indiquez l’IP locale de votre PS5 dans le bot puis cliquez sur **Synchroniser fiches + jaquettes**. Le serveur FTP doit être actif sur 2121. Relancez PS5 Hub puis appuyez sur **Carré** : les fiches apparaissent dans **Jeux**.

Les informations personnelles restent dans `.local/library` sur le PC. Ce dossier est ignoré par Git. Aucun lien de votre collection n’est publié automatiquement sur GitHub.

## Formats acceptés

**TXT**, un lien HTTPS direct par ligne :

```text
https://mon-serveur.example/mon-jeu.pkg
https://mon-serveur.example/mon-autre-jeu.pkg
```

**CSV**, avec des points-virgules :

```csv
nom;lien;jaquette;sha256
Mon jeu;https://mon-serveur.example/mon-jeu.pkg;images/mon-jeu.jpg;
Mon autre jeu;https://mon-serveur.example/autre.pkg;https://mon-serveur.example/jaquette.jpg;
```

Les adresses ci-dessus sont des exemples de syntaxe, pas des liens vers des jeux. `jaquette` et `sha256` sont facultatifs. Une jaquette peut être un JPEG / PNG local ou une URL HTTPS. Le SHA-256 doit venir d’une source de confiance pour vérifier le fichier téléchargé.

**JSON** : une liste d’objets avec `title`, `url`, `cover`, `sha256`, ou un objet `{"games":[...]}`. Les colonnes françaises `nom`, `lien`, `jaquette`, `empreinte` sont aussi acceptées. Les doublons exacts sont éliminés. Limite : **5000 fiches** par import, catalogue natif de 4 Mio maximum. Pour des liens à jetons renouvelés, fournissez un `id` stable en CSV / JSON.

Un lien vers une page d’hébergeur n’est pas forcément le lien du fichier. Le bot refuse une page HTML reçue à la place du PKG ; il ne contourne ni compte, ni captcha, ni lien protégé. En TXT, le nom vient du nom de fichier dans l’URL : si ce nom est opaque, un CSV avec `nom` donne un meilleur résultat.

## Jaquettes

Le bot convertit les images en un petit fichier RGBA `.hubart` pour éviter de décoder de grosses images sur la console. PS5 Hub charge seulement la page de jaquettes utile, sur un travailleur, puis transmet les pixels au rendu OpenGL.

La recherche facultative sur **Steam** fournit des **suggestions** d’illustration d’après le nom du jeu. Elles peuvent correspondre à une autre édition que la PS5 : vérifiez-les et remplacez-les avec `jaquette` si nécessaire. Les noms sont envoyés à la boutique Steam uniquement lorsque cette recherche est activée. Sans résultat, une illustration originale de remplacement est générée ; elle n’est jamais présentée comme une jaquette d’éditeur. Les images récupérées pour votre collection ne sont pas incluses dans le dépôt public.

## Transférer un PKG sélectionné

Le bot télécharge le fichier **sur le PC**, avec lecture par blocs : il n’utilise pas le petit stockage privé de l’application PS5. Le SHA-256 est calculé ; si vous avez fourni une empreinte, elle est comparée avant tout transfert. Un fichier incomplet n’est pas publié avec son nom final.

Deux modes sont disponibles, après confirmation pour le jeu sélectionné :

- **ftp** : copie vérifiée dans `/data/ps5-hub/pkg` sur le port 2121. Le fichier est relu et son SHA-256 comparé. **Cela n’installe pas le jeu.** Un fichier du même nom n’est pas écrasé.
- **hyperdpi** : envoie le PKG à l’API HyperDPI de HyperHEN, par défaut sur **9090**, puis demande son installation. HyperDPI doit déjà être activé dans votre environnement. Le bot vérifie `/ping`, envoie le premier Mio avec `head256`, envoie les blocs suivants puis appelle `finalize=1`.

**Une demande acceptée par HyperDPI ne prouve pas la fin de l’installation.** Vérifiez la progression et le résultat sur sa WebUI ou la console. La compatibilité du PKG et de votre version de HyperHEN n’est pas déduite de sa taille ou de son nom. Le protocole du bot est testé contre un simulateur local ; son fonctionnement sur votre console reste à confirmer.

La [documentation publique HyperDPI](https://github.com/Michele-M-Media/HYPERHEN/tree/main/third_party/hyperdpi) décrit les ports et le protocole. Elle indique que le service est désactivé par défaut et ses écouteurs HTTP ne sont pas authentifiés. N’exposez pas ces ports sur Internet ; le bot accepte seulement une adresse IPv4 locale pour la PS5.

## Commandes pour automatiser la préparation locale

```powershell
python tools/hub-library-bot.py import mes-liens.txt --auto-covers
python tools/hub-library-bot.py list
python tools/hub-library-bot.py sync --ps5 IP_DE_LA_PS5
python tools/hub-library-bot.py transfer --id IDENTIFIANT_AFFICHE --ps5 IP_DE_LA_PS5 --mode ftp
python tools/hub-library-bot.py transfer --id IDENTIFIANT_AFFICHE --ps5 IP_DE_LA_PS5 --mode hyperdpi
python tools/hub-library-bot.py watch mes-liens.csv
```

`watch` surveille la liste et régénère les fiches quand vous la modifiez. **Il ne télécharge ni ne transfère de jeux automatiquement.** Ctrl+C l’arrête. Un import entièrement invalide conserve le catalogue précédent et produit un message d’erreur. Les avertissements par ligne sont dans `.local/library/import-report.json`.

La synchronisation attend l’installation interne `/data/homebrew/PPSA99841`. Si vous avez gardé le hub uniquement sur USB, copiez manuellement `bundle/library.tsv` vers `PPSA99841/assets/library.tsv` et `bundle/covers/*` vers `PPSA99841/assets/covers/` sur votre support, quand l’application est fermée.
