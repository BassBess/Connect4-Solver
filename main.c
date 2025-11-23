#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "defs.h"
#include "board.h"
#include "logic.h"
#include "ai.h"
#include "bot_hard.h" 

int getIntInput() {
    char input[20];
    fgets(input, sizeof(input), stdin);
    int value;
    if (sscanf(input, "%d", &value) == 1) {
        return value;
    }
    return -1; 
}

void setupPlayers(Player players[2], int *botMode, int *botDiff) {
    printf("Welcome to Connect 4!\n\n");
    printf("Choose game mode:\n");
    printf("1 - Player vs Player\n");
    printf("2 - Player vs Computer (Easy)\n");
    printf("3 - Player vs Computer (Medium)\n");
    printf("4 - Player vs Computer (Hard)\n");
    printf("Enter your choice (1-4): ");
    
    int choice = getIntInput();
    while (choice < 1 || choice > 4) {
        printf("Invalid choice. Try again: ");
        choice = getIntInput();
    }
    
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

        setupPlayers(players, &botMode, &botDiff);

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

        printf("\nGame starting!\n");
        printBoard(board);

        while (!gameOver)
        {
            int column = -1;

            if (botMode && current == 1)  
            {
                printf("\nThinking...\n");
                
                column = getBotMove(board, botDiff, players[1].symbol, players[0].symbol);
                
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

        freeBoard(board);
    }
    
    return 0;
}
