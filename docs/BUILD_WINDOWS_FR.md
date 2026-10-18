# Compiler sous Windows 11 avec WSL

Utilisez une distribution Ubuntu 24.04 WSL2 fonctionnelle. Les compilateurs et les dépendances PS5 sont publics ; aucun SDK propriétaire Sony n’est fourni.

```powershell
wsl --install -d Ubuntu-24.04
```

Dans Ubuntu :

```bash
sudo apt update
sudo apt install -y clang-18 lld-18 clang-format-18 clang-tidy-18 make ninja-build ccache pkg-config git curl wget unzip python3 python3-venv libegl-dev libgl-dev libgl1-mesa-dri libcurl4-openssl-dev
git clone https://github.com/tomoff1/ps5-hub.git
cd ps5-hub
bash tools/hub-build.sh
```

Gardez de préférence le dépôt dans le système de fichiers Linux de WSL. Les dépendances ont besoin d’un espace disque libre suffisant et d’un accès HTTPS à GitHub. Les scripts téléchargent des versions épinglées du SDK public, de ps5-opengl et des ports PacBrew et vérifient les empreintes prévues par l’amont.

### Stocker la compilation sur D:

Le clone Windows peut rester dans `D:\pkg app ps5\ps5-hub`. Le cache Linux est
stocké dans le disque virtuel de la distribution WSL : placer uniquement le clone
sur D: ne déplace donc pas les dépendances Linux. Sur une version de WSL proposant
`--manage --move`, fermez les tâches de cette distribution puis utilisez, par
exemple, `wsl --manage kali-linux --move "D:\pkg app ps5\wsl-kali"`.
Le dossier de destination ne doit pas déjà exister. Cette commande déplace la
distribution complète et conserve ses fichiers ; ne la remplacez pas par
`--unregister`. Les autres distributions et les données de Windows restent à leur
emplacement actuel.

`HUB_BUILD_CACHE` permet de choisir un cache Linux absolu sans espaces. Par exemple,
`HUB_BUILD_CACHE=/tmp/ps5-hub-build bash tools/hub-build.sh` réutilise ce cache
dans la distribution déplacée. Les dossiers `.local`, `node_modules` et
`.wrangler` ne sont pas copiés dans le cache de compilation.

Pour le serveur de catalogue, `./Start-HubServer.ps1 -Action build` utilise les
temporaires, les journaux Wrangler et le cache npm du dossier `.local` du projet, donc sur D: si le
projet est sur D:. Les actions `test`, `dev`, `deploy` et `whoami` sont également
disponibles. Ce réglage s'applique seulement au processus lancé ; il ne modifie
pas les variables globales de Windows ni les identifiants Cloudflare.

Depuis un clone Windows, le point d’entrée complet est :

```powershell
.\Build-Hub.ps1 -Distribution Ubuntu-24.04
```

`Build-Hub.ps1` exécute **Make**, qui ajoute les options OpenGL et libcurl. Le vieux `build.ps1` hérité est conservé à titre de référence ; utilisez `Build-Hub.ps1` pour PS5 Hub.

Sorties :

```text
dist/PPSA99841/
  eboot.bin
  sce_sys/param.json
  sce_sys/icon0.png
  sce_module/libc.prx
  assets/...
dist/PS5-Hub-00.003.000-experimental.zip
dist/PS5-Hub-00.003.000-experimental.zip.sha256
```

Le dossier contient aussi les modules et fichiers produits par le kit. Conservez-les tous. Un contrôle FSELF, la présence des fichiers et l’empreinte du ZIP sont vérifiés par `tools/hub-package.py`. Cela confirme le format du paquet, pas son exécution sur PS5.

## Tests et aperçu

```bash
make test-unit HOST_CXX=clang++-18 HOST_CC=clang-18
HOST_CXX=clang++-18 HOST_CC=clang-18 bash tools/host-snapshots.sh build/snapshots hub
```

Les PNG proviennent du même C++ et des mêmes shaders que la console, rendus par Mesa. Ce n’est pas une simulation de firmware. Ils ne permettent pas de mesurer la fréquence d’affichage sur console ou de valider les pilotes DualSense.

Test réseau optionnel sur PC : `bash tools/hub-network-smoke.sh`. Il télécharge deux fois l’archive officielle du catalogue (environ 64 Mo au total), vérifie un téléchargement correct puis le rejet d’un SHA-256 volontairement faux. Il utilise AddressSanitizer et UndefinedBehaviorSanitizer. Il ne contacte aucune console et ne procède à aucune installation système.

Pour reconstruire les polices avec les caractères accentués français : `bash tools/bake-fonts.sh`. Pour reconstruire l’icône originale : installez Pillow et lancez `python3 tools/hub-assets.py`. Les artefacts de police et l’icône sont déjà présents dans le dépôt.

## Catalogue

Le fichier `assets/catalog.tsv` contient sept colonnes séparées par des tabulations :

```text
id  titre  version  type  url  sha256  description
```

Types : `link` pour une fiche de projet, `homebrew` pour une archive ZIP, `pkg` pour un paquet. Les identifiants sont composés de lettres minuscules, chiffres et tirets ; les URL sont HTTPS ; le SHA-256 est en hexadécimal minuscule. Le catalogue importé du bot accepte 5000 fiches et 4 Mio ; l’ajout manuel reste limité à 500 fiches. Une erreur de lecture ou de validation ne remplace pas le catalogue par des données partielles.

Le catalogue personnel est enregistré sur le titre par le clavier DualSense. Il n’est jamais ajouté à GitHub automatiquement. La vérification de mise à jour lit uniquement `release-version.txt` dans le dépôt officiel PS5 Hub ; elle ne télécharge ni n’installe automatiquement une mise à jour.
