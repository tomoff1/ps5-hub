# Validation — alpha 0.5

Le démarrage signalé par l’utilisateur concernait l’alpha 0.1. Aucun test de
la 0.5 n’a été exécuté sur une PS5 13.60 pendant ce développement.

## Vérifications automatisées

Contrôles locaux du 18 octobre 2026 : **447 tests C++ réussis**, **9 scénarios
HTTPS réussis**, **9 scénarios DPIv2 réussis**, **20 tests du catalogue réussis**
et compilation PS5 réussie. Les résultats du commit exact de publication sont
également enregistrés par son workflow GitHub Actions. Ces contrôles couvrent :

- tests C++ avec AddressSanitizer et UndefinedBehaviorSanitizer, dont file,
  persistance, restauration sans reprise automatique, annulation et identité
  de l’installateur ;
- neuf scénarios HTTPS locaux exécutant le véritable téléchargeur C++ ;
- neuf scénarios HTTP locaux exécutant le véritable connecteur DPIv2 C++ :
  détection sans POST, refus d’un autre service et d’une redirection, multipart
  contenant les octets vérifiés, mauvaise empreinte, mauvais format, refus
  etaHEN, réponse inattendue et réponse perdue ;
- tests existants du catalogue Cloudflare, du bot et du transfert FTP simulé ;
- rendu OpenGL natif hors écran sous Mesa et compilation du dossier PS5 avec
  contrôle de son conteneur FSELF et préparation du ZIP avec SHA-256.

Le serveur de test écoute uniquement sur la boucle locale du poste de test.
Aucun jeu réel n’est utilisé et aucune installation n’est demandée à une console.

## Ce que ces contrôles ne prouvent pas

Ils ne valident pas le navigateur système, sa navigation DualSense, les
connexions Web ou le retour au titre sur 13.60. Ils ne mesurent pas le quota
du sandbox ni les droits d’accès à `/user/data/tmp`. Ils ne valident pas une
installation etaHEN, les signatures/clefs, le firmware requis ou le lancement
d’un jeu. Le connecteur considère uniquement une demande acceptée, jamais la
fin d’installation. Le protocole brut 9090 n’effectue aucun envoi.

Les restrictions détaillées et les essais manuels sont dans
[Téléchargements natifs](TELECHARGEMENTS_NATIFS_FR.md) et
[Compatibilité 13.60](COMPATIBILITE_13_60.md).
