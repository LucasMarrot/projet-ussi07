#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/stat.h>

#define BUFFER_SIZE 1024

/**
 * Formate l'heure actuelle dans un buffer.
 * @param buffer Le buffer où l'heure sera formatée.
 * @param buffer_size La taille du buffer.
 */
void format_current_time(char *buffer, size_t buffer_size)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, buffer_size, "%d/%m/%Y %H:%M:%S", tm_info);
}

/**
 * Affiche l'historique et le prompt.
 * @param history L'historique des messages.
 * @param prompt_message Le message de prompt.
 */
void display_history_and_prompt(const char *history, const char *prompt_message)
{
    system("clear");
    printf("%s", history);
    printf("\n%s", prompt_message);
    fflush(stdout);
}

/**
 * Gère la communication avec le serveur.
 * @param client_socket Le socket du client.
 * @param channel_name Le nom du channel.
 */
void chat(int client_socket, char *channel_name)
{
    char buffer[BUFFER_SIZE];
    char history[BUFFER_SIZE * 10] = "";
    fd_set read_fds;

    while (1)
    {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(client_socket, &read_fds);

        if (select(client_socket + 1, &read_fds, NULL, NULL, NULL) < 0)
        {
            perror("Erreur lors de select");
            break;
        }

        if (FD_ISSET(client_socket, &read_fds))
        {
            int read_size = recv(client_socket, buffer, sizeof(buffer), 0);
            if (read_size <= 0)
            {
                printf("Déconnecté du serveur\n");
                break;
            }
            buffer[read_size] = '\0';
            strcat(history, buffer);
            display_history_and_prompt(history, "Envoyer un message : ");
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds))
        {
            fgets(buffer, sizeof(buffer), stdin);
            buffer[strcspn(buffer, "\n")] = '\0';

            if (strcmp(buffer, "/quit") == 0)
            {
                system("clear");
                printf("Déconnexion...\n");
                break;
            }

            if (strncmp(buffer, "/switch ", 8) == 0)
            {
                char new_channel[BUFFER_SIZE];
                sscanf(buffer + 8, "%s", new_channel);
                send(client_socket, buffer, strlen(buffer), 0);
                strncpy(channel_name, new_channel, sizeof(channel_name) - 1);
                channel_name[sizeof(channel_name) - 1] = '\0';
                snprintf(history, sizeof(history), "Changement vers le channel '%s'\n", channel_name);
                display_history_and_prompt(history, "Envoyer un message : ");
                continue;
            }

            if (strcmp(buffer, "/help") == 0)
            {
                system("clear");
                printf("\n\nCommandes disponibles :\n");
                printf("-------------------------\n");
                printf("/quit             : Quitter le chat\n");
                printf("/switch [channel] : Changer de channel\n");
                printf("/help             : Afficher cette aide\n\n");
                printf("Appuyez sur Entrée pour revenir au chat...\n");
                getchar();
                display_history_and_prompt(history, "Envoyer un message : ");
                continue;
            }

            if (send(client_socket, buffer, strlen(buffer), 0) == -1)
            {
                perror("Erreur lors de l'envoi du message");
                break;
            }

            char time_buffer[26];
            format_current_time(time_buffer, sizeof(time_buffer));

            char formatted_message[BUFFER_SIZE];
            snprintf(formatted_message, sizeof(formatted_message), "[%s] (%s) Moi : %s\n", channel_name, time_buffer, buffer);

            strcat(history, formatted_message);
            display_history_and_prompt(history, "Envoyer un message : ");
        }
    }
}

int main()
{
    int client_socket;
    struct sockaddr_in server_addr;
    char name[50];
    char channel_name[50];

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1)
    {
        perror("Erreur lors de la création du socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(12345);

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Erreur lors de la connexion au serveur");
        exit(EXIT_FAILURE);
    }

    printf("Entrez votre nom : ");
    fgets(name, sizeof(name), stdin);
    name[strcspn(name, "\n")] = '\0';
    send(client_socket, name, strlen(name), 0);

    printf("Entrez le nom du channel : ");
    fgets(channel_name, sizeof(channel_name), stdin);
    channel_name[strcspn(channel_name, "\n")] = '\0';
    send(client_socket, channel_name, strlen(channel_name), 0);

    chat(client_socket, channel_name);
    close(client_socket);
    return 0;
}
