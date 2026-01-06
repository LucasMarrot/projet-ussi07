## Mini Projet de Lucas MARROT et Kyliann CLARET-LAVAL

### Description des fichiers :

#### `server.c`

Implémente un serveur de chat multicanal utilisant les sockets TCP et les threads (pthread).

- Écoute les connexions entrantes sur le port 12345
- Gère plusieurs clients simultanément (un thread par client)
- Organise les clients par channels (salons de discussion)
- Enregistre l'historique des messages dans des fichiers de stockage
- Diffuse des messages à tous les clients du channel (sauf l'expéditeur)
- Gère la commande `/switch` pour changer de channel

#### `client.c`

Implémente un client de chat qui se connecte au serveur.

- Se connecte au serveur sur l'adresse 127.0.0.1 et le port 12345
- Permet à l'utilisateur de saisir son nom et son channel initial
- Utilise `select()` pour gérer simultanément les entrées utilisateur et les messages du serveur
- Affiche l'historique du channel et une invite de saisie
- Supporte les commandes `/help`, `/switch` et `/quit`

### Compilation :

```bash
gcc -o server server.c -lpthread
```

```bash
gcc -o client client.c
```

### Exécution :

```bash
./server
```

```bash
./client
```

### Commandes disponibles :

- `/help` : Afficher l'aide

###

- `/quit` : Quitter le chat
- `/switch [channel]` : Changer de channel
