/**************************************************************
 * Tetris-like Game in C++ (Console-based) with:
 *   - ANSI Background Colors
 *   - Pause (toggle with 'p')
 *   - Modified Interface Layout (left panel, center board, right panel)
 *   - Same controls (arrows, space, ESC, etc.)
 *
 * Platform: Windows (using <conio.h> for kbhit/getch).
 *           For Linux/macOS, adapt input (ncurses/termios).
 **************************************************************/

#include <iostream>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <conio.h> // Windows-specific for _kbhit() and getch()
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#endif

using namespace std;
#ifndef _WIN32

// Fresh getch() implementation for Linux (with simple arrow-key support)
int getch(void)
{
    struct termios oldt, newt;
    int ch;

    // Save current terminal attributes
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    // Disable canonical mode (buffered i/o) and disable echo
    newt.c_lflag &= ~(ICANON | ECHO);
    // Apply new settings immediately
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    ch = getchar();
    // Handle arrow keys (escape sequences)
    if (ch == 27)
    { // ESC character
        int c1 = getchar();
        if (c1 == '[')
        {
            int c2 = getchar();
            switch (c2)
            {
            case 'A':
                ch = 72;
                break; // Up arrow → 72
            case 'B':
                ch = 80;
                break; // Down arrow → 80
            case 'C':
                ch = 77;
                break; // Right arrow → 77
            case 'D':
                ch = 75;
                break; // Left arrow → 75
            default:
                ch = c2;
                break;
            }
        }
        else
        {
            ungetc(c1, stdin); // Push back if not part of an escape sequence
        }
    }

    // Restore original terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

// Fresh kbhit() implementation for Linux
int _kbhit(void)
{
    struct termios oldt, newt;
    int ch;
    int oldFlags;

    // Save current terminal attributes
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    // Disable canonical mode and echo
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    // Save current file status flags and set non-blocking mode
    oldFlags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldFlags | O_NONBLOCK);

    ch = getchar();

    // Restore settings
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldFlags);

    if (ch != EOF)
    {
        ungetc(ch, stdin); // Push the character back so getch() can retrieve it later
        return 1;
    }
    return 0;
}
#endif

/**************************************************************
 * 1) Utility: Non-blocking key press check
 **************************************************************/
int score = 0;
int highscore = 0;

bool kbhit_non_blocking()
{
    // Windows-specific. On Linux/macOS, use an alternative approach.
    return _kbhit() != 0;
}

/**************************************************************
 * 2) Color/Terminal Utilities (ANSI escape sequences)
 **************************************************************/

// inline functions : copies entrie code from the function instead of making a stack

inline void clearScreen()
{
#ifdef _WIN32
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), {0, 0});
#else
    system("clear");
#endif
}

inline void setCursorPos(int row, int col)
{
    // ANSI escape code to set cursor position(1 based indexing)
    cout << "\033[" << row << ";" << col << "H";
}

// Set background color using ANSI. Color in [0..7] range for basic colors.
inline void setBackgroundColor(int color)
{
    // 40 + color sets background (e.g., 41 = red background)
    cout << "\033[" << (40 + color) << "m";
}

// Reset all attributes (foreground, background, etc.)
inline void resetColor()
{
    cout << "\033[0m";
}

/**************************************************************
 * 3) Basic definitions for Tetris
 **************************************************************/
const int BOARD_WIDTH = 10;  // You can adjust as you like
const int BOARD_HEIGHT = 20; // You can adjust as you like

// 7 standard Tetromino shapes (4x4)
static const vector<vector<vector<int>>> TETROMINO_SHAPES = {
    // I
    {
        {0, 0, 0, 0},
        {1, 1, 1, 1},
        {0, 0, 0, 0},
        {0, 0, 0, 0}},
    // O
    {
        {1, 1, 0, 0},
        {1, 1, 0, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0}},
    // T
    {
        {0, 1, 0, 0},
        {1, 1, 1, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0}},
    // S
    {
        {0, 1, 1, 0},
        {1, 1, 0, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0}},
    // Z
    {
        {1, 1, 0, 0},
        {0, 1, 1, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0}},
    // J
    {
        {1, 0, 0, 0},
        {1, 1, 1, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0}},
    // L
    {
        {0, 0, 1, 0},
        {1, 1, 1, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0}}};

/**************************************************************
 * 4) Tetromino Base Class
 **************************************************************/
class Tetromino
{
protected:
    vector<vector<int>> shape; // 4x4
    int colorIndex;            // for distinct color

public:
    Tetromino(const vector<vector<int>> &shp, int color)
        : shape(shp), colorIndex(color) {}
    virtual ~Tetromino() {}

    virtual void rotateCW()
    {
        // Rotate shape 90 degrees clockwise
        vector<vector<int>> rotated(4, vector<int>(4, 0));
        for (int r = 0; r < 4; r++)
        {
            for (int c = 0; c < 4; c++)
            {
                rotated[c][4 - 1 - r] = shape[r][c];
            }
        }
        shape = rotated;
    }

    // Accessors
    const vector<vector<int>> &getShape() const { return shape; }
    int getColorIndex() const { return colorIndex; }
};

// Concrete Tetromino Classes (for demonstration)(shape, colorIndex)
class TetrominoI : public Tetromino
{
public:
    TetrominoI() : Tetromino(TETROMINO_SHAPES[0], 1) {}
};
class TetrominoO : public Tetromino
{
public:
    TetrominoO() : Tetromino(TETROMINO_SHAPES[1], 2) {}
};
class TetrominoT : public Tetromino
{
public:
    TetrominoT() : Tetromino(TETROMINO_SHAPES[2], 3) {}
};
class TetrominoS : public Tetromino
{
public:
    TetrominoS() : Tetromino(TETROMINO_SHAPES[3], 4) {}
};
class TetrominoZ : public Tetromino
{
public:
    TetrominoZ() : Tetromino(TETROMINO_SHAPES[4], 5) {}
};
class TetrominoJ : public Tetromino
{
public:
    TetrominoJ() : Tetromino(TETROMINO_SHAPES[5], 6) {}
};
class TetrominoL : public Tetromino
{
public:
    TetrominoL() : Tetromino(TETROMINO_SHAPES[6], 7) {}
};

/**************************************************************
 * 5) Board Class: Encapsulates the 2D grid
 **************************************************************/

class Board
{
private:
    // board[r][c] = 0 if empty, else color index
    vector<vector<int>> board;

public:
    Board()
    {
        board.resize(BOARD_HEIGHT, vector<int>(BOARD_WIDTH, 0));
    }

    bool canPlace(const Tetromino &t, int row, int col) const
    {
        const auto &shape = t.getShape(); // 2d matrix for perticular shape
        for (int r = 0; r < 4; r++)
        {
            for (int c = 0; c < 4; c++)
            {
                if (shape[r][c] != 0)
                {
                    int br = row + r;
                    int bc = col + c;
                    // Out of bounds?
                    if (br < 0 || br >= BOARD_HEIGHT || bc < 0 || bc >= BOARD_WIDTH)
                    {
                        return false;
                    }
                    // Collision with existing block?
                    if (board[br][bc] != 0)
                    {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    void place(const Tetromino &t, int row, int col)
    {
        const auto &shape = t.getShape();
        int color = t.getColorIndex();
        for (int r = 0; r < 4; r++)
        {
            for (int c = 0; c < 4; c++)
            {
                if (shape[r][c] != 0)
                {
                    int br = row + r;
                    int bc = col + c;
                    board[br][bc] = color;
                }
            }
        }
    }

    // Clear full lines and return how many lines cleared
    int clearLines()
    {
        int linesCleared = 0;
        for (int r = 0; r < BOARD_HEIGHT; r++)
        {
            bool full = true;
            for (int c = 0; c < BOARD_WIDTH; c++)
            {
                if (board[r][c] == 0)
                {
                    full = false;
                    break;
                }
            }
            if (full)
            {
                // Shift everything down
                for (int rr = r; rr > 0; rr--)
                {
                    board[rr] = board[rr - 1];
                }
                // Clear top row
                board[0] = vector<int>(BOARD_WIDTH, 0);
                linesCleared++;
            }
        }
        return linesCleared;
    }

    bool isGameOver() const
    {
        // If top row is non-zero, game is over
        for (int c = 0; c < BOARD_WIDTH; c++)
        {
            if (board[0][c] != 0)
            {
                return true;
            }
        }
        return false;
    }

    // Accessor to read a specific cell (for drawing)
    int getCell(int r, int c) const
    {
        return board[r][c];
    }
};

/**************************************************************
 * 6) Game Class: Manages game state, logic, user input, etc.
 *     Includes pause functionality (toggle with 'p')
 *     Now draws an interface resembling the screenshot
 **************************************************************/
class Game
{
private:
    Board board;
    Tetromino *currentPiece;
    Tetromino *nextPiece;
    int currentRow, currentCol;
    bool gameOver;
    bool paused; // pause toggle
    int level;
    int linesClearedTotal;

public:
    Game()
        : currentPiece(nullptr), nextPiece(nullptr),
          currentRow(0), currentCol(0),
          gameOver(false), paused(false),
          level(1), linesClearedTotal(0)
    {
        srand((unsigned)time(nullptr));
        currentPiece = randomTetromino();
        nextPiece = randomTetromino();
        // Center the initial piece
        currentRow = 0;
        currentCol = BOARD_WIDTH / 2 - 2;
    }

    ~Game()
    {
        delete currentPiece;
        delete nextPiece;
    }
    /**************************************************************
     *Makeups: WelCome and GameOver screens
     **************************************************************/
    void showStartingAnimation()
    {
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
        cout << "\033[44m" << "\033[37m" << "\033[1m";

        vector<string> tetrisArt = {
            "████████╗███████╗████████╗██████╗ ██╗███████╗",
            "╚══██╔══╝██╔════╝╚══██╔══╝██╔══██╗██║██╔════╝",
            "   ██║   █████╗     ██║   ██████╔╝██║███████╗",
            "   ██║   ██╔══╝     ██║   ██╔══██╗██║╚════██║",
            "   ██║   ███████╗   ██║   ██║  ██║██║███████║",
            "   ╚═╝   ╚══════╝   ╚═╝   ╚═╝  ╚═╝╚═╝╚══════╝",
            "    M A D E   B Y   T E A M   E N I G M A    "};

        for (int i = 0; i < tetrisArt.size(); i++)
        {
            cout << tetrisArt[i] << endl;
#ifdef _WIN32
            Sleep(300);
#else
            usleep(300000);
#endif
        }

        cout << "\033[0m" << "\n\nStarting game in 3...";
        cout.flush();
#ifdef _WIN32
        Sleep(300);
#else
        usleep(300000);
#endif
        cout << "2...";
        cout.flush();
#ifdef _WIN32
        Sleep(500);
#else
        usleep(300000);
#endif
        cout << "1...";
        cout.flush();
#ifdef _WIN32
        Sleep(500);
#else
        usleep(300000);
#endif

        system("cls");
    }

    void drawPauseScreen()
    {
        // Clear screen and draw pause message
        cout << "\033[2J\033[H";
        cout << "\033[44m" << "\033[37m" << "\033[1m" << "\n\n\n\n";
        cout << "         ██████╗  █████╗ ██╗   ██╗███████╗███████╗       \n";
        cout << "         ██╔══██╗██╔══██╗██║   ██║██╔════╝██╔════╝       \n";
        cout << "         ██████╔╝███████║██║   ██║███████╗█████╗         \n";
        cout << "         ██╔═══╝ ██╔══██║██║   ██║╚════██║██╔══╝         \n";
        cout << "         ██║     ██║  ██║╚██████╔╝███████║███████╗       \n";
        cout << "         ╚═╝     ╚═╝  ╚═╝ ╚═════╝ ╚══════╝╚══════╝       \n";
        cout << "\033[0m" << "\n\n";
        cout << "\033[43m" << "\033[30m" << "           Press P to continue           " << "\033[0m" << "\n";
        while (paused)
        {
            handleInput();
        }

        cout.flush();

#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
    }

    int drawGameOverScreen()
    {
        highscore = max(score, highscore);
        clearScreen();
        cout << "\033[41m" << "\033[37m" << "\033[1m" << "\n\n\n\n";
        cout << "          ██████╗  █████╗ ███╗   ███╗███████╗     \n";
        cout << "         ██╔════╝ ██╔══██╗████╗ ████║██╔════╝     \n";
        cout << "         ██║  ███╗███████║██╔████╔██║█████╗       \n";
        cout << "         ██║   ██║██╔══██║██║╚██╔╝██║██╔══╝       \n";
        cout << "         ╚██████╔╝██║  ██║██║ ╚═╝ ██║███████╗     \n";
        cout << "          ╚═════╝ ╚═╝  ╚═╝╚═╝     ╚═╝╚══════╝     \n";
        cout << "          ██████╗ ██╗   ██╗███████╗██████╗        \n";
        cout << "         ██╔═══██╗██║   ██║██╔════╝██╔══██╗       \n";
        cout << "         ██║   ██║██║   ██║█████╗  ██████╔╝       \n";
        cout << "         ██║   ██║╚██╗ ██╔╝██╔══╝  ██╔══██╗       \n";
        cout << "         ╚██████╔╝ ╚████╔╝ ███████╗██║  ██║       \n";
        cout << "          ╚═════╝   ╚═══╝  ╚══════╝╚═╝  ╚═╝       \n";
        cout << "\033[0m" << "\n\n";
        cout << "\033[42m" << "\033[30m" << "           Your Score: " << score << "           " << "\033[0m" << "\n";
        cout << "\033[44m" << "\033[37m" << "        High Score: " << highscore << "        " << "\033[0m" << "\n\n";
        cout << "\033[43m" << "\033[30m" << "     Press R to restart or X to exit     " << "\033[0m" << "\n";
        cout.flush();

        cout << "Press 'R' to Restart\n(NOTE:Any other keys terminates the game: )" << endl;

        char rest;
        cin >> rest;

        if (rest == 'R' || rest == 'r')
            return 1;

        // Show cursor again
        cout << "\033[?25h";
        return 0;
    }

    // Factory method: returns a random Tetromino
    Tetromino *randomTetromino()
    {
        int r = rand() % 7;
        switch (r)
        {
        case 0:
            return new TetrominoI();
        case 1:
            return new TetrominoO();
        case 2:
            return new TetrominoT();
        case 3:
            return new TetrominoS();
        case 4:
            return new TetrominoZ();
        case 5:
            return new TetrominoJ();
        case 6:
            return new TetrominoL();
        }
        // fallback
        return new TetrominoI();
    }

    bool run()
    {
        score = 0;
        // Hide cursor (optional) // ANSI Escape sequence
        cout << "\033[?25l";
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
        showStartingAnimation();

        while (!gameOver)
        {
            // 1) Clear and draw interface each frame
            clearScreen();
            drawInterface();

            // 2) Handle input
            handleInput();

            // 3) Update piece position (gravity) if not paused
            if (!paused)
            {
                moveDown();
            }

            // 4) Check game over
            if (board.isGameOver())
            {
                gameOver = true;
            }

            // 5) Control speed
            int delay = 100 - (level - 1) * 10;
            if (delay < 0)
                delay = 10;
#ifdef _WIN32
            Sleep(delay);
#else
            usleep(delay * 1500);
#endif
        }

        // Final screen

#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
    }

private:
    // Draw the entire interface (left panel, board in center, right panel)
    void drawInterface()
    {
        // -------------------------------------
        // LEFT PANEL (Level, lines, score, controls)
        // -------------------------------------
        int leftPanelRow = 1;
        int leftPanelCol = 1;
        setCursorPos(leftPanelRow++, leftPanelCol);
        cout << "Your Level: " << level;

        setCursorPos(leftPanelRow++, leftPanelCol);
        cout << "Full Lines: " << linesClearedTotal;

        setCursorPos(leftPanelRow++, leftPanelCol);
        cout << "Score: " << score;

        // Show paused status
        if (paused)
        {
            drawPauseScreen();
        }
        else
        {
            setCursorPos(leftPanelRow++, leftPanelCol);
            cout << "Game Status : [ RUNNING ]\n";
        }

        setCursorPos(leftPanelRow++, leftPanelCol);
        cout << "CONTROLS:";
        setCursorPos(leftPanelRow++, leftPanelCol);
        cout << "  p/P   : Pause";
        setCursorPos(leftPanelRow++, leftPanelCol);
        cout << "  Left  : Move Left";
        setCursorPos(leftPanelRow++, leftPanelCol);
        cout << "  Right : Move Right";
        setCursorPos(leftPanelRow++, leftPanelCol);
        cout << "  Up    : Rotate";
        setCursorPos(leftPanelRow++, leftPanelCol);
        cout << "  Down  : Soft Drop";
        setCursorPos(leftPanelRow++, leftPanelCol);
        cout << "  Space : Hard Drop";
        setCursorPos(leftPanelRow++, leftPanelCol);
        cout << "  ESC   : Quit";

        // -------------------------------------
        // BOARD in the CENTER with a border
        // -------------------------------------

        int boardTop = 2;   // row offset
        int boardLeft = 30; // column offset (space to left panel)
        int cellWidth = 2;  // each cell is "  "
        int borderWidth = BOARD_WIDTH * cellWidth;
        int borderHeight = BOARD_HEIGHT;

        // Draw top border
        setCursorPos(boardTop, boardLeft);
        cout << "\033[0;101m \033[0m";
        for (int i = 0; i < borderWidth; i++)
            cout << "-";
        cout << "\033[0;101m \033[0m";

        // Draw side borders
        for (int r = 0; r < borderHeight; r++)
        {
            setCursorPos(boardTop + 1 + r, boardLeft);
            cout << "\033[0;106m \033[0m"; // Right Boarder
            setCursorPos(boardTop + 1 + r, boardLeft + borderWidth + 1);
            cout << "\033[0;106m \033[0m"; // Left Border
        }

        // Draw bottom border
        setCursorPos(boardTop + borderHeight + 1, boardLeft);
        cout << "\033[0;101m \033[0m";
        for (int i = 0; i < borderWidth; i++)
            cout << "-";
        cout << "\033[0;101m \033[0m";

        // Overlay current piece on a temp board
        Board tempBoard = board;
        tempBoard.place(*currentPiece, currentRow, currentCol);

        // Print each row of the board inside the border
        for (int r = 0; r < BOARD_HEIGHT; r++)
        {
            setCursorPos(boardTop + 1 + r, boardLeft + 1);
            for (int c = 0; c < BOARD_WIDTH; c++)
            {
                int val = tempBoard.getCell(r, c);
                if (val == 0)
                {
                    cout << "  "; // empty
                }
                else
                {
                    setBackgroundColor(val % 8);
                    cout << "  ";
                    resetColor();
                }
            }
        }

        // -------------------------------------
        // RIGHT PANEL (Statistics / Next Piece)
        // -------------------------------------
        int rightPanelRow = 2;
        int rightPanelCol = boardLeft + borderWidth + 5;
        setCursorPos(rightPanelRow++, rightPanelCol);
        cout << "STATISTICS";
        rightPanelRow++;

        // For example, show next piece preview or placeholder
        setCursorPos(rightPanelRow++, rightPanelCol);
        cout << "Next Piece:";

        // Draw next piece in a small 4x4 area
        const auto &shp = nextPiece->getShape();
        int nc = nextPiece->getColorIndex() % 8;

        for (int row = 0; row < 4; row++)
        {
            setCursorPos(rightPanelRow + row, rightPanelCol);
            for (int col = 0; col < 4; col++)
            {
                if (shp[row][col] != 0)
                {
                    setBackgroundColor(nc);
                    cout << "  ";
                    resetColor();
                }
                else
                {
                    cout << "  ";
                }
            }
        }

        // You could also show usage counts, etc., if you track them
    }

    void handleInput()
    {
        while (kbhit_non_blocking())
        {
            int ch = getch();
            switch (ch)
            {
            case 75: // Left arrow
                if (!paused)
                    tryMove(currentRow, currentCol - 1);
                break;
            case 77: // Right arrow
                if (!paused)
                    tryMove(currentRow, currentCol + 1);
                break;
            case 80: // Down arrow
                if (!paused)
                    moveDown(); // soft drop
                break;
            case 72: // Up arrow
                if (!paused)
                {
                    currentPiece->rotateCW();
                    if (!board.canPlace(*currentPiece, currentRow, currentCol))
                    {
                        // Rotate back if invalid
                        for (int i = 0; i < 3; i++)
                        {
                            currentPiece->rotateCW();
                        }
                    }
                }
                break;
            case ' ': // Hard drop
                if (!paused)
                {
                    while (board.canPlace(*currentPiece, currentRow + 1, currentCol))
                    {
                        currentRow++;
                    }
                    lockPiece();
                }
                break;
            case 'p':
                paused = !paused;
                break;
            case 27: // ESC
                gameOver = true;
                break;
            default:
                break;
            }
        }
    }

    void moveDown()
    {
        if (board.canPlace(*currentPiece, currentRow + 1, currentCol))
        {
            currentRow++;
        }
        else
        {
            lockPiece();
        }
    }

    void lockPiece()
    {
        board.place(*currentPiece, currentRow, currentCol);
        int cleared = board.clearLines();
        if (cleared > 0)
        {
            score += (cleared * 100);
            linesClearedTotal += cleared;
            // Increase level for every 10 lines, for example
            if (linesClearedTotal / 10 >= level)
            {
                level++;
            }
        }
        delete currentPiece;
        currentPiece = nextPiece;
        nextPiece = randomTetromino();
        currentRow = 0;
        currentCol = BOARD_WIDTH / 2 - 2;
    }

    void tryMove(int newRow, int newCol)
    {
        if (board.canPlace(*currentPiece, newRow, newCol))
        {
            currentRow = newRow;
            currentCol = newCol;
        }
    }
};

/**************************************************************
 * main(): Entry Point
 **************************************************************/
int main()
{
#ifdef _WIN32
    // Optionally, enable UTF-8 in Windows console if needed:
    SetConsoleOutputCP(CP_UTF8);
#endif

Start:
    Game game;
    game.run();
    int g = game.drawGameOverScreen();

    if (g == 1)
        goto Start;

    return 0;
}
