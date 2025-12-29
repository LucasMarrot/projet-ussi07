# Serveur de chat multiclients  
**Système et Architecture des Machines – FIP1**  
*Mini-projet*

## Description
Ce projet consiste à développer un **serveur de chat multiclients** permettant à plusieurs utilisateurs de se connecter simultanément et d’échanger des messages en temps réel.  
Le serveur repose sur l’utilisation des **sockets réseau** pour la communication et des **threads** pour gérer plusieurs connexions clientes en parallèle.

Chaque message envoyé par un client est automatiquement **diffusé à l’ensemble des autres clients connectés**. Les échanges sont également **journalisés** afin de conserver une trace des communications.

## Fonctionnalités
- Connexion de plusieurs clients à un serveur unique
- Communication réseau via **sockets**
- Gestion des connexions simultanées avec **un thread par client**
- Diffusion des messages à tous les clients connectés
- Journalisation des messages échangés

#### [📘 Aide utilisateur](HELP.md)
