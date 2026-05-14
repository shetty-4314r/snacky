#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

struct Point {
    int x{};
    int y{};

    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};

enum class Direction {
    Up,
    Down,
    Left,
    Right
};

class Snake {
public:
    explicit Snake(const Point& start) : body_{start} {}

    const Point& head() const { return body_.front(); }
    const std::deque<Point>& body() const { return body_; }

    void setDirection(Direction direction) { direction_ = direction; }
    Direction direction() const { return direction_; }

    bool occupies(const Point& point) const {
        return std::find(body_.begin(), body_.end(), point) != body_.end();
    }

    Point nextHeadPosition() const {
        Point next = head();
        switch (direction_) {
            case Direction::Up:
                --next.y;
                break;
            case Direction::Down:
                ++next.y;
                break;
            case Direction::Left:
                --next.x;
                break;
            case Direction::Right:
                ++next.x;
                break;
        }
        return next;
    }

    void move(bool grow) {
        body_.push_front(nextHeadPosition());
        if (!grow) {
            body_.pop_back();
        }
    }

    bool hitsItself() const {
        const Point& currentHead = head();
        return std::find(body_.begin() + 1, body_.end(), currentHead) != body_.end();
    }

private:
    std::deque<Point> body_;
    Direction direction_ = Direction::Right;
};

class Console {
public:
    static void setup() {
#ifdef _WIN32
        HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_CURSOR_INFO info;
        GetConsoleCursorInfo(output, &info);
        info.bVisible = FALSE;
        SetConsoleCursorInfo(output, &info);
#else
        std::cout << "\033[?25l";
        std::cout.flush();
#endif
    }

    static void cleanup() {
#ifdef _WIN32
        HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_CURSOR_INFO info;
        GetConsoleCursorInfo(output, &info);
        info.bVisible = TRUE;
        SetConsoleCursorInfo(output, &info);
#else
        std::cout << "\033[?25h";
        std::cout.flush();
#endif
    }

    static void gotoxy(int x, int y) {
#ifdef _WIN32
        COORD pos = {static_cast<SHORT>(x), static_cast<SHORT>(y)};
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
#else
        std::cout << "\033[" << (y + 1) << ";" << (x + 1) << 'H';
#endif
    }

    static void clear() {
#ifdef _WIN32
        HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(output, &csbi);

        const DWORD cells = csbi.dwSize.X * csbi.dwSize.Y;
        DWORD count;
        COORD home = {0, 0};

        FillConsoleOutputCharacter(output, ' ', cells, home, &count);
        FillConsoleOutputAttribute(output, csbi.wAttributes, cells, home, &count);
        SetConsoleCursorPosition(output, home);
#else
        std::cout << "\033[2J\033[H";
#endif
    }
};

class Input {
public:
#ifndef _WIN32
    Input() {
        tcgetattr(STDIN_FILENO, &originalTermios_);
        termios raw = originalTermios_;
        raw.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }

    ~Input() {
        tcsetattr(STDIN_FILENO, TCSANOW, &originalTermios_);
    }
#endif

    bool kbhit() const {
#ifdef _WIN32
        return _kbhit() != 0;
#else
        timeval timeout{0, 0};
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        return select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &timeout) > 0;
#endif
    }

    int getch() const {
#ifdef _WIN32
        return _getch();
#else
        unsigned char ch;
        return (read(STDIN_FILENO, &ch, 1) == 1) ? ch : -1;
#endif
    }

private:
#ifndef _WIN32
    termios originalTermios_{};
#endif
};

class Game {
public:
    Game(int width, int height, int delayMs)
        : width_(width), height_(height), delayMs_(delayMs), snake_({width_ / 2, height_ / 2}), rng_(std::random_device{}()) {
        loadHighScore();
        spawnFood();
    }

    void run() {
        Input input;
        bool keepPlaying = true;

        while (keepPlaying) {
            reset();
            gameLoop(input);
            keepPlaying = showGameOverAndAskRestart(input);
        }
    }

private:
    const int width_;
    const int height_;
    const int delayMs_;

    Snake snake_;
    Point food_{};
    int score_ = 0;
    int highScore_ = 0;
    bool gameOver_ = false;
    bool paused_ = false;

    std::mt19937 rng_;

    void reset() {
        snake_ = Snake({width_ / 2, height_ / 2});
        score_ = 0;
        gameOver_ = false;
        paused_ = false;
        spawnFood();
        Console::clear();
    }

    void gameLoop(const Input& input) {
        while (!gameOver_) {
            processInput(input);
            if (!paused_) {
                update();
            }
            draw();
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs_));
        }
        updateHighScore();
    }

    void processInput(const Input& input) {
        if (!input.kbhit()) {
            return;
        }

        const int key = std::tolower(input.getch());
        Direction nextDirection = snake_.direction();

        if (key == 'w') {
            nextDirection = Direction::Up;
        } else if (key == 's') {
            nextDirection = Direction::Down;
        } else if (key == 'a') {
            nextDirection = Direction::Left;
        } else if (key == 'd') {
            nextDirection = Direction::Right;
        } else if (key == 'p') {
            paused_ = !paused_;
            return;
        } else {
            return;
        }

        if (!isOppositeDirection(snake_.direction(), nextDirection)) {
            snake_.setDirection(nextDirection);
        }
    }

    void update() {
        const Point nextHead = snake_.nextHeadPosition();

        // Wall collision: touching border ends the game.
        if (nextHead.x <= 0 || nextHead.x >= width_ - 1 || nextHead.y <= 0 || nextHead.y >= height_ - 1) {
            gameOver_ = true;
            return;
        }

        const bool eatsFood = (nextHead == food_);
        snake_.move(eatsFood);

        // Self collision: head matching any body segment ends the game.
        if (snake_.hitsItself()) {
            gameOver_ = true;
            return;
        }

        if (eatsFood) {
            ++score_;
            spawnFood();
        }
    }

    void draw() const {
        Console::gotoxy(0, 0);

        std::string frame;
        const std::size_t scoreLineMax = std::string("Score: ").size() + 20 +
                                         std::string("   High Score: ").size() + 20 +
                                         std::string("   [PAUSED - press P]\n").size();
        const std::size_t controlsLine = std::string("Controls: W A S D to move | P to pause\n").size();
        frame.reserve((width_ + 1) * height_ + scoreLineMax + controlsLine);

        frame += "Score: " + std::to_string(score_) + "   High Score: " + std::to_string(highScore_);
        frame += paused_ ? "   [PAUSED - press P]\n" : "\n";

        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                Point point{x, y};
                if (y == 0 || y == height_ - 1 || x == 0 || x == width_ - 1) {
                    frame += '#';
                } else if (point == snake_.head()) {
                    frame += 'O';
                } else if (point == food_) {
                    frame += '*';
                } else if (snake_.occupies(point)) {
                    frame += 'o';
                } else {
                    frame += ' ';
                }
            }
            frame += '\n';
        }

        frame += "Controls: W A S D to move | P to pause\n";

        std::cout << frame << std::flush;
    }

    bool showGameOverAndAskRestart(const Input& input) const {
        constexpr int restartPollDelayMs = 40;
        Console::gotoxy(0, height_ + 3);
        std::cout << "Game Over! Final Score: " << score_
                  << " | Press R to restart or Q to quit: " << std::flush;

        while (true) {
            if (!input.kbhit()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(restartPollDelayMs));
                continue;
            }

            const int key = std::tolower(input.getch());
            if (key == 'r') {
                return true;
            }
            if (key == 'q') {
                return false;
            }
        }
    }

    void spawnFood() {
        std::vector<Point> freeCells;
        freeCells.reserve(static_cast<std::size_t>((width_ - 2) * (height_ - 2)));

        for (int y = 1; y < height_ - 1; ++y) {
            for (int x = 1; x < width_ - 1; ++x) {
                Point candidate{x, y};
                if (!snake_.occupies(candidate)) {
                    freeCells.push_back(candidate);
                }
            }
        }

        if (freeCells.empty()) {
            gameOver_ = true;
            return;
        }

        std::uniform_int_distribution<std::size_t> cellDist(0, freeCells.size() - 1);
        food_ = freeCells[cellDist(rng_)];
    }

    static bool isOppositeDirection(Direction current, Direction next) {
        return (current == Direction::Up && next == Direction::Down) ||
               (current == Direction::Down && next == Direction::Up) ||
               (current == Direction::Left && next == Direction::Right) ||
               (current == Direction::Right && next == Direction::Left);
    }

    void loadHighScore() {
        std::ifstream highscoreFile("highscore.txt");
        if (highscoreFile >> highScore_) {
            return;
        }
        highScore_ = 0;
    }

    void updateHighScore() {
        if (score_ <= highScore_) {
            return;
        }

        highScore_ = score_;
        std::ofstream highscoreFile("highscore.txt", std::ios::trunc);
        if (highscoreFile.is_open()) {
            highscoreFile << highScore_;
        }
    }
};

int main() {
    std::cout << "=== Console Snake Game ===\n";
    std::cout << "Choose difficulty: 1) Slow 2) Medium 3) Fast\n";
    std::cout << "Enter choice (default 2): ";

    int difficulty = 2;
    if (!(std::cin >> difficulty)) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        difficulty = 2;
    }

    int width = 40;
    int height = 20;

    std::cout << "Board width (min 20, default 40): ";
    if (!(std::cin >> width) || width < 20) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        width = 40;
    }

    std::cout << "Board height (min 10, default 20): ";
    if (!(std::cin >> height) || height < 10) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        height = 20;
    }

    int delay = 120;
    if (difficulty == 1) {
        delay = 180;
    } else if (difficulty == 3) {
        delay = 75;
    }

    Console::setup();
    Console::clear();

    {
        Game game(width, height, delay);
        game.run();
    }

    Console::cleanup();
    Console::gotoxy(0, height + 5);
    std::cout << "Thanks for playing!\n";
    return 0;
}
