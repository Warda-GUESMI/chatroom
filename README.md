# ChatRoom — Serveur de Chat Multi-Rooms en C

Projet de Programmation Systèmes et Réseaux couvrant :
- **Synchronisation** : mutexes par ressource (UserManager, RoomManager, User), variables de condition (Queue)
- **Gestion des threads** : thread pool de 8 workers, file de tâches producteur/consommateur
- **Communication par sockets** : TCP/IP, client multi-thread (envoi + réception)

## Compilation

```bash
make
```

## Lancement

```bash
# Terminal 1 — Serveur
./server/chatserver 9090

# Terminal 2 — Client A
./client/chatclient 127.0.0.1 9090

# Terminal 3 — Client B
./client/chatclient 127.0.0.1 9090
```

## Commandes

| Commande              | Action                         |
|-----------------------|--------------------------------|
| `/nick <pseudo>`      | Changer de pseudo (unique)     |
| `/join <salle>`       | Rejoindre une salle            |
| `/msg <texte>`        | Envoyer un message dans la salle |
| `/pm <user> <texte>`  | Message privé                  |
| `/list`               | Lister salles et utilisateurs  |
| `/quit`               | Se déconnecter                 |

## Debug / Analyse

```bash
# Détecter les races et deadlocks
valgrind --tool=helgrind ./server/chatserver 9090

# Vérifier le port
ss -tnp | grep 9090
```

## Corrections apportées (vs version originale)

| # | Problème | Correction |
|---|----------|------------|
| 1 | Race condition sur les champs `nick`/`current_room` de `User` | Ajout d'un mutex `user->lock` par utilisateur |
| 2 | Deadlock potentiel dans CMD_JOIN (`global_lock` + `room->lock`) | Lecture des champs sous `u->lock`, ordre de verrou documenté |
| 3 | `write()` appelé en tenant `room->lock` (clients lents bloquent la salle) | Copie des fd sous lock, écriture après relâchement |
| 4 | Buffer `response` (1024) trop petit pour `/list` (64 utilisateurs) | Buffer agrandi à 4096 octets |
| 5 | `room_get_or_create` utilisé à la place de `room_find` pour MSG | Ajout de `room_find()` (ne crée pas de salle fantôme) |
| 6 | Nick non unique (`/pm` ambigu) | Vérification d'unicité dans CMD_NICK |
| 7 | `write()` partiel / EINTR ignoré | Fonction `write_all()` avec boucle |
| 8 | Nick par défaut basé sur `fd` réutilisable | Compteur atomique monotone |
| 9 | SIGPIPE non ignoré (crash sur client déconnecté) | `signal(SIGPIPE, SIG_IGN)` dans server et client |
