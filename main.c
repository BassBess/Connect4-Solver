#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "defs.h"
#include "board.h"
#include "logic.h"
#include "ai.h"
#include "bot_hard.h"
#include "network.h"

int getIntInput() {
char input[20];
fgets(input, sizeof(input), stdin);
int value;
if (sscanf(input, "%d", &value) == 1) {
return value;
}
return -1;
}

void setupPlayers(Player players[2], int *botMode, int *botDiff, int *networkMode, int *socket_fd, int *isLocalPlayerFirst) {
printf("Welcome to Connect 4!\n\n");
printf("Choose game mode:\n");
printf("1 - Player vs Player\n");
printf("2 - Player vs Computer (Easy)\n");
printf("3 - Player vs Computer (Medium)\n");
printf("4 - Player vs Computer (Hard)\n");
printf("5 - Player vs Player (Online)\n");
printf("Enter your choice (1-5): ");

int choice = getIntInput();
while (choice < 1 || choice > 5) {
    printf("Invalid choice. Try again: ");
    choice = getIntInput();
}

if (choice == 5) {
    *networkMode = 1;
    *botMode = 0;
    
    printf("\nOnline Multiplayer\n");
    printf("1 - Host game (Server)\n");
    printf("2 - Join game (Client)\n");
    printf("Enter choice (1 or 2): ");
    
    int netChoice = getIntInput();
    while (netChoice != 1 && netChoice != 2) {
        printf("Invalid choice. Enter 1 or 2: ");
        netChoice = getIntInput();
    }
    
    int server_fd = -1;
    bool isServer = (netChoice == 1);
    
    if (isServer) {
        server_fd = network_create_server(8888);
        if (server_fd < 0) {
            printf("Failed to create server!\n");
            exit(1);
        }
        
        printf("\n=== HOSTING GAME ===\n");
        printf("Your IP address(es):\n");
        system("hostname -I 2>/dev/null || ipconfig getifaddr en0 2>/dev/null || echo '(Could not detect IP)'");
        printf("\nPort: 8888\n");
        printf("Tell your opponent to connect to: <your-ip>:8888\n");
        printf("====================\n");
        
        *socket_fd = network_accept_client(server_fd);
        if (*socket_fd < 0) {
            close(server_fd);
            exit(1);
        }
        close(server_fd); 
        
        printf("\nOpponent connected!\n");
        
        printf("\nWho should go first?\n");
        printf("1 - You\n");
        printf("2 - Opponent\n");
        printf("Enter choice (1 or 2): ");
        
        int firstChoice = getIntInput();
        while (firstChoice != 1 && firstChoice != 2) {
            printf("Invalid choice. Enter 1 or 2: ");
            firstChoice = getIntInput();
        }
        
        network_send_first_player(*socket_fd, firstChoice);
        
        *isLocalPlayerFirst = (firstChoice == 1);
        
        strcpy(players[0].name, "You");
        players[0].symbol = 'A';
        strcpy(players[1].name, "Opponent");
        players[1].symbol = 'B';
        
    } else {
        printf("\nEnter server IP address: ");
        char ip[50];
        fgets(ip, sizeof(ip), stdin);
        ip[strcspn(ip, "\n")] = 0; 
        
        *socket_fd = network_connect_to_server(ip, 8888);
        if (*socket_fd < 0) {
            printf("Failed to connect to server!\n");
            exit(1);
        }
        
        printf("\nWaiting for host to choose who goes first...\n");
        int firstChoice = network_receive_first_player(*socket_fd);
        
        if (firstChoice < 0) {
            printf("Connection error!\n");
            network_close(*socket_fd);
            exit(1);
        }
        
        *isLocalPlayerFirst = (firstChoice == 2);
        
        strcpy(players[0].name, "Opponent");
        players[0].symbol = 'A';
        strcpy(players[1].name, "You");
        players[1].symbol = 'B';
    }
    
    printf("\nPlayers set: %s (%c) vs %s (%c)\n", 
           players[0].name, players[0].symbol,
           players[1].name, players[1].symbol);
    return;
}

*networkMode = 0;
strcpy(players[0].name, "Player A");
players[0].symbol = 'A';

if (choice >= 2 && choice <= 4) {
    *botMode = 1;
    
    if (choice == 2) {
        *botDiff = 1;
        strcpy(players[1].name, "Computer (Easy)");
    } else if (choice == 3) {
        *botDiff = 2;
        strcpy(players[1].name, "Computer (Medium)");
    } else {
        *botDiff = 3;
        strcpy(players[1].name, "Computer (Hard)");
    }
    
    players[1].symbol = 'B';
} else {
    *botMode = 0;
    strcpy(players[1].name, "Player B");
    players[1].symbol = 'B';
}

printf("Players set: %s (%c) vs %s (%c)\n", 
       players[0].name, players[0].symbol,
       players[1].name, players[1].symbol);
}

int main()
{
init_hard_bot();
srand(time(NULL));
int playAgain = 1;

while (playAgain)
{
    char **board = createBoard();
    Player players[2];
    int current = 0, gameOver = 0;
    int botMode = 0;  
    int botDiff = 0;
    int networkMode = 0;  
    int socket_fd = -1;   
    int isLocalPlayerFirst = 0; 

    setupPlayers(players, &botMode, &botDiff, &networkMode, &socket_fd, &isLocalPlayerFirst);

    if (!networkMode) {
        printf("\nWho should go first?\n");
        printf("1 - %s (%c)\n", players[0].name, players[0].symbol);
        printf("2 - %s (%c)\n", players[1].name, players[1].symbol);
        printf("Enter choice (1 or 2): ");
        
        int first = getIntInput();
        while (first != 1 && first != 2) {
             printf("Invalid choice. Enter 1 or 2: ");
             first = getIntInput();
        }

        if (first == 2) current = 1;
        else current = 0;
    } else {
        if (isLocalPlayerFirst) {
            current = (strcmp(players[0].name, "You") == 0) ? 0 : 1;
        } else {
            current = (strcmp(players[0].name, "Opponent") == 0) ? 0 : 1;
        }
    }

    printf("\nGame starting!\n");
    printBoard(board);

    while (!gameOver)
    {
        int column = -1;
        bool isLocalPlayer = false;
        
        if (networkMode) {
            isLocalPlayer = (strcmp(players[current].name, "You") == 0);
        } else {
            isLocalPlayer = true; 
        }

        if (botMode && current == 1)  
        {
            printf("\nThinking...\n");
            column = getBotMove(board, botDiff, players[1].symbol, players[0].symbol);
            printf("%s (%c) chooses column %d\n", players[current].name, players[current].symbol, column);
        }
        else if (networkMode && !isLocalPlayer)
        {
            printf("\nWaiting for opponent's move...\n");
            column = network_receive_move(socket_fd);
            
            if (column < 0) {
                printf("\nConnection lost! Opponent disconnected.\n");
                gameOver = 1;
                playAgain = 0;
                break;
            }
            
            printf("%s (%c) chooses column %d\n", players[current].name, players[current].symbol, column);
        }
        else
        {
            printf("\n%s (%c), choose a column (1-7): ", players[current].name, players[current].symbol);
            column = getIntInput();

            if (column == -1) {
                printf("Please enter a valid number.\n");
                continue;
            }
        }

        if (column < 1 || column > COLS)
        {
            printf("Invalid column. Choose between 1 and %d.\n", COLS);
            continue;
        }

        int row_placed;
        if (!placePiece(board, column, players[current].symbol, &row_placed))
            continue; 

        if (networkMode && isLocalPlayer) {
            if (!network_send_move(socket_fd, column)) {
                printf("\nFailed to send move! Connection error.\n");
                gameOver = 1;
                playAgain = 0;
                break;
            }
        }

        printBoard(board);

        if (checkWin(board, row_placed, column - 1, players[current].symbol))
        {
            printf("\n%s (%c) wins!\n", players[current].name, players[current].symbol);
            gameOver = 1;
        }
        else if (isBoardFull(board))
        {
            printf("\nIt's a draw!\n");
            gameOver = 1;
        }
        else
        {
            current = !current;
        }

        if (gameOver) {
            if (networkMode) {
                printf("\nGame over! Closing connection.\n");
                network_close(socket_fd);
                playAgain = 0; 
            } else {
                printf("Play again? (y/n): ");
                char response[10];
                fgets(response, sizeof(response), stdin);
                
                if (response[0] == 'y' || response[0] == 'Y') {
                    playAgain = 1;
                    printf("\nStarting new game...\n\n");
                } else {
                    playAgain = 0;
                    printf("Thanks for playing!\n");
                }
            }
        }
    }

    freeBoard(board);
}

return 0;
}
