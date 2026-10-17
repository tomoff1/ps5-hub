# Compatibilité et protocole de test — firmware 13.60

Retour utilisateur du 17 octobre 2026 : **l’alpha 0.1 apparaît sur l’accueil et démarre sur sa configuration déclarée PS5 13.60**, avec photos fournies dans le chat. Les photos personnelles ne sont pas redistribuées dans le dépôt. Ce retour ne valide pas toutes les fonctions, ni l’alpha 0.3.

| Élément | État |
|---|---|
| PS5 jailbreakée, firmware 13.60 | Configuration déclarée par l’utilisateur |
| HyperHEN | Version exacte inconnue |
| ShadowMountPlus | Version exacte inconnue |
| FTP 2121 | Disponibilité déclarée ; connexion non testée |
| Enregistrement du titre | Signalé sur l’alpha 0.1 ; icône personnalisée non confirmée |
| Lancement et rendu OpenGL | Signalés sur l’alpha 0.1 ; alpha 0.3 à tester |
| DualSense, son, cadence et fermeture | À tester |
| HTTPS et stockage dans le sandbox | À tester |
| Installation de PKG depuis le hub | Non implémentée |
| Bot PC / HyperDPI | Protocole testé avec simulateur ; console réelle à tester |

Le projet amont du boilerplate rapporte des tests de son propre runtime sur 6.02 et 12.70. Ils ne valident ni cette application, ni le runtime OpenGL sur 13.60. Le README de [HyperHEN](https://github.com/Michele-M-Media/HYPERHEN) mentionne un chemin 13.60, mais une mention amont n’est pas un résultat de test de PS5 Hub.

## Premier test manuel, avec le propriétaire présent

1. Noter le modèle de PS5 et les versions exactes des chargeurs, sans changer le firmware.
2. Calculer le SHA-256 du ZIP et enregistrer le commit utilisé.
3. Installer le dossier par USB ou FTP, attendre l’analyse du chargeur et relever la présence de l’icône.
4. Ouvrir une seule fois. Vérifier l’écran Accueil et relever une éventuelle erreur exacte.
5. Parcourir les six rubriques avec le pavé directionnel et le stick ; ouvrir puis fermer une fiche ; vérifier le retour avec Rond.
6. Couper les sons et activer la réduction des animations, fermer normalement depuis l’interface système puis relancer manuellement pour vérifier la persistance.
7. Ajouter un petit homebrew personnel avec son URL et le SHA-256 vérifié depuis la source de confiance. Confirmer le téléchargement ; vérifier la présence du fichier, l’annulation et les erreurs réseau.
8. Tester volontairement une mauvaise empreinte : aucun fichier final ne doit être publié. Vérifier les logs et l’absence de faux message d’installation.
9. Rechercher une mise à jour, relever le résultat, puis fermer normalement. Ne pas tuer un processus qui dessine encore.

Arrêter si le premier lancement bloque la console. Pas de boucle de relance, pas de test automatique non surveillé, pas de changement de réglages système.

## Compte rendu à compléter

```text
Date / testeur :
Modèle PS5 :
Firmware affiché :
HyperHEN (version ou commit) :
ShadowMountPlus (version ou commit) :
Commit PS5 Hub :
SHA-256 du ZIP :
Icône / lancement :
Menus / DualSense / son :
Sauvegarde / HTTPS / téléchargement / annulation :
Fermeture normale :
Erreurs exactes et journaux :
Résultat : non testé / échec / partiel / validé pour CETTE configuration
```

La validation ne doit être ajoutée au README qu’avec ce compte rendu et l’artefact correspondant.
