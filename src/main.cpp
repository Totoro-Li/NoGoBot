#include <iostream>
#include <cstring>
#include "jsoncpp/json.h"
#include <cstdarg>
#include <random>
#include <functional>
#include <algorithm>
#include <utility>
#include <chrono>

#ifdef _WITH_TORCH

#include <torch/torch.h>

#endif // _WITH_TORCH

#pragma region Logging

// === Auxiliary functions ===
static inline char *time_now() {
    static char buffer[64];
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer, 64, "%H:%M:%S", timeinfo);

    return buffer;
}

#define LOG_FILE strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__

#define LOG_FMT             "%s [%s][AlphaNoGo:%s:%d]>%s:"
#define LOG_ARGS(LOG_TAG)   time_now(), LOG_TAG, LOG_FILE, __LINE__, __FUNCTION__

#define NEWLINE     "\n"
#define ERROR_TAG   "ERROR"
#define WARN_TAG    "WARN"
#define INFO_TAG    "INFO"
#define DEBUG_TAG   "DEBUG"

#define LOG_DEBUG(message, ...)  nogo_log(LOG_LEVEL_DEBUG, LOG_FMT message NEWLINE, LOG_ARGS(DEBUG_TAG), ##__VA_ARGS__)
#define LOG_INFO(message, ...)   nogo_log(LOG_LEVEL_INFO, LOG_FMT message NEWLINE, LOG_ARGS(INFO_TAG), ##__VA_ARGS__)
#define LOG_WARN(message, ...)   nogo_log(LOG_LEVEL_WARNING, LOG_FMT message NEWLINE, LOG_ARGS(WARN_TAG), ##__VA_ARGS__)
#define LOG_ERROR(message, ...)  nogo_log(LOG_LEVEL_ERROR, LOG_FMT message NEWLINE, LOG_ARGS(ERROR_TAG), ##__VA_ARGS__)
#define LOG_IF_ERROR(condition, message, ...) if (condition) nogo_log(LOG_LEVEL_ERROR, LOG_FMT message NEWLINE, LOG_ARGS(ERROR_TAG), ##__VA_ARGS__)

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARNING = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_NONE = 4,
} LogLevel;

LogLevel logLevel_default = LOG_LEVEL_INFO;

void nogo_log(LogLevel logLevel, const char *format, ...) {
    if (logLevel < logLevel_default) return;
    va_list ap;
    char message[1024];
            va_start(ap, format);
    vsnprintf(message, sizeof(message), format, ap);
            va_end(ap);
}

#pragma endregion

#pragma region Declaration

typedef std::string string;
constexpr int MAX_BOARD_SIZE = 9;
#ifdef _BOTZONE_ONLINE
// Declaration of functions for online judge

// CFFI_DEF_START

/*An AlphaNoGo function returns*/
typedef enum {
    OK = 0,
    JSON_PARSING_ERROR = -1,
    INVALID_BOARD_SIZE = -2,
    INVALID_BOARD = -3,
    INVALID_MOVE = -4,
} ExitCode;

typedef struct {
    int x;
    int y;
} Point;

typedef enum {
    SELF = -1,
    EMPTY = 0,
    ENEMY = 1
} BoardState;

typedef enum {
    EARLYHAND = 0,
    LATEHAND = 1,
} HandOrder;

typedef std::function<void(Point, BoardState)> BoardCallback;

int receive_all(BoardCallback cb);

int receive_once(const BoardCallback &cb);
// CFFI_DEF_END
#else
// Declaration of functions are identical for AlphaNoGo library export APIs
#include <alpha_nogo_common.h>
#endif
#pragma endregion


char const *const KEEP_RUNNING_CMD = ">>>BOTZONE_REQUEST_KEEP_RUNNING<<<";

const int cx[] = {-1, 0, 1, 0};
const int cy[] = {0, -1, 0, 1};

HandOrder handOrder = LATEHAND;

class PieceGroup {
private:
    std::vector<Point> locations;
    BoardState color;
public:
    PieceGroup(BoardState color) : color(color) {}

    PieceGroup(Point p, BoardState col) {
        color = col;
        locations.push_back(p);
    }

    ~PieceGroup() = default;

    //Return the number of pieces in the group
    int getSize() {
        return static_cast<int>(locations.size());
    }

    //Returns the location at a given index
    Point getLocation(int index) {
        if (index >= 0 && index < getSize()) {
            return locations[index];
        } else return {-1, -1};
    }

    //Returns whether the group contains a particular spot on the board
    bool contains(Point p) {
        return std::any_of(locations.begin(), locations.end(), [p](Point q) { return p.x == q.x && p.y == q.y; });
    }

    //returns whether the input piece is connected to this group
    bool isConnected(Point p) {
        return std::any_of(locations.begin(), locations.end(), [p](Point q) {
            return (p.x == q.x && abs(p.y - q.y) == 1) || (p.y == q.y && abs(p.x - q.x) == 1);
        });
    }

    void addPiece(Point p) {
        locations.push_back(p);
    }

    //Put the contents of this group to the input group and return the result
    PieceGroup combine(PieceGroup other) {
        std::for_each(locations.begin(), locations.end(), [&other](Point p) { other.addPiece(p); });
        return other;
    }

    BoardState getColor() { return color; }

};

class BoardBase {
private:
    BoardState board[MAX_BOARD_SIZE][MAX_BOARD_SIZE]{};
    std::vector<PieceGroup> groups;
    BoardState turn;
public:
    BoardBase() {
        memset(board, 0, sizeof(board));
    }

    BoardState get(int x, int y) const {
        return board[x][y];
    }

    void set(Point p, BoardState color) {
        if (inBorder(p.x, p.y)) {
            board[p.x][p.y] = color;
        }
    }

    void reset() {
        memset(board, 0, sizeof(board));
    }

    static BoardState oppositeColor(BoardState input) {
        return static_cast<BoardState>(-input);
    }

    static bool inBorder(int x, int y) { return x >= 0 && y >= 0 && x < MAX_BOARD_SIZE && y < MAX_BOARD_SIZE; }


    //true: available
    bool judgeAvailable(int fx, int fy, BoardState col) {
        if (col == EMPTY)
            return false;
        if (board[fx][fy]) return false;
        board[fx][fy] = col;
        if (!calcLiberties({fx, fy}) || checkCaptures(fx, fy, col)) {
            board[fx][fy] = EMPTY;
            return false;
        }
        board[fx][fy] = EMPTY;
        return true;
    }

    int calcLiberties(PieceGroup grp) const {
        int sum = 0;
        for (int i = 0; i < grp.getSize(); ++i) {
            sum += calcLiberties(grp.getLocation(i));
        }
        return sum;
    }

    //calculate liberties for a single piece
    int calcLiberties(Point p) const {
        int sum = 0;

        //check the four adjacent locations for a piece
        if (get(p.x - 1, p.y) == EMPTY) sum++;
        if (get(p.x + 1, p.y) == EMPTY) sum++;
        if (get(p.x, p.y - 1) == EMPTY) sum++;
        if (get(p.x, p.y + 1) == EMPTY) sum++;
        return sum;
    }

    //checks whether each adjacent piece to the placed piece is dead
    bool checkCaptures(int x, int y, BoardState c) {
        bool flag = false;
        for (int dir = 0; dir < 4; dir++) {
            int dx = x + cx[dir], dy = y + cy[dir];
            if (inBorder(dx, dy) && board[dx][dy] == oppositeColor(c)) {
                if (calcLiberties({dx, dy}) == 0) {
                    flag = true;
                }
            }
        }
        return flag;
    }
};


class BotzoneMsgHandler {
private:
    int turn_id;
public:
    BotzoneMsgHandler() {
        turn_id = 0;
    }

    int get_turn_id() const {
        return turn_id;
    }

    // Receive pointer to a Board member function set
    int receive_and_parse(BoardCallback cb) {
        /*
        {
            "requests" : [
                "Judge request in Turn 1", // 第 1 回合 Bot 从平台获取的信息（request），具体格式依游戏而定
                "Judge request in Turn 2", // 第 2 回合 Bot 从平台获取的信息（request），具体格式依游戏而定
                ...
            ],
            "responses" : [
                "Bot response in Turn 1", // 第 1 回合 Bot 输出的信息（response），具体格式依游戏而定
                "Bot response in Turn 2", // 第 2 回合 Bot 输出的信息（response），具体格式依游戏而定
                ...
            ],
            "data" : "saved data", // 上回合 Bot 保存的信息，最大长度为100KB【注意不会保留在 Log 中】
            "globaldata" : "globally saved data", // 来自上次对局的、Bot 全局保存的信息，最大长度为100KB【注意不会保留在 Log 中】
            "time_limit" : "", // 时间限制
            "memory_limit" : "" // 内存限制
        }
         */
#ifndef _NOT_BOTZONE_LONGRUN
        int ret = receive_once(cb);
#else
        int ret = receive_all(std::move(cb));
#endif // _NOT_BOTZONE_LONGRUN
        if (ret == JSON_PARSING_ERROR) {
            exit(-1);
        }
        turn_id++;
        return turn_id;
    }
};

std::vector<Point> enemy_decision_history;
std::vector<Point> self_decision_history;

int receive_all(const BoardCallback &cb) {
    // Receive and parse the json input from botzone
    string str;
    getline(std::cin, str);
    Json::Reader reader;
    Json::Value input;
    if (!reader.parse(str, input)) {
        LOG_ERROR("Failed to parse input json");
        return JSON_PARSING_ERROR;
    }
    int x, y;
    int turnID = static_cast<int>(input["responses"].size());
    std::for_each(input["requests"].begin(), input["requests"].end(), [&](Json::Value &request) {
        x = request["x"].asInt();
        y = request["y"].asInt();
        if (x == -1 && handOrder == LATEHAND)
            handOrder = EARLYHAND;
        enemy_decision_history.push_back({x, y});
        cb({x, y}, ENEMY);
    });
    std::for_each(input["responses"].begin(), input["responses"].end(), [&](Json::Value &response) {
        x = response["x"].asInt();
        y = response["y"].asInt();
        self_decision_history.push_back({x, y});
        cb({x, y}, SELF);
    });
    if (enemy_decision_history[0].x == -1) {
        enemy_decision_history.clear();
        self_decision_history.clear();
    }
    return turnID;
}

int receive_once(const BoardCallback &cb) {
    string str;
    getline(std::cin, str);
    Json::Reader reader;
    Json::Value input;
    if (!reader.parse(str, input)) {
        LOG_ERROR("Failed to parse input json");
        return JSON_PARSING_ERROR;
    }
    int x, y;
    // If input contains "requests" field, it means the first round with conventional IO
    if (input.isMember("requests")) {
        x = input["requests"][0]["x"].asInt();
        y = input["requests"][0]["y"].asInt();
        if (x == -1 && handOrder == LATEHAND)
            handOrder = EARLYHAND;
        enemy_decision_history.push_back({x, y});
        cb({x, y}, ENEMY);
        return OK;
    }
    //
    x = input["x"].asInt();
    y = input["y"].asInt();
    enemy_decision_history.push_back({x, y});
    cb({x, y}, ENEMY);
    return OK;
}

// TODO function ptr for search function
typedef std::function<int(BoardBase, int, BoardState)> SearchFunc;

#ifndef _NOT_BOTZONE_LONGRUN
bool game_over = false;

int main() {
    // Initialize the board
    BoardBase board;
    // Loop receive input and calculate action till end of game
    BotzoneMsgHandler msgHandler;
    while (!game_over) {
        // Dynamic bind member function to pointer std::bind(&BoardBase::set, &board, std::placeholders::_1, std::placeholders::_2);
        int turnID = msgHandler.receive_and_parse([ObjectPtr = &board](auto &&PH1, auto &&PH2) { ObjectPtr->set(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2)); });
        if (turnID == 0) {
            // Initialize your bot here
        }

        // Calculate your action here
        //以下为随机策略
        std::vector<Point> available_list; //合法位置表

        for (int i = 0; i < 9; i++)
            for (int j = 0; j < 9; j++)
                if (board.judgeAvailable(i, j, SELF))
                    available_list.push_back({i, j});

        // Generate a random number with uniform distribution
        std::default_random_engine generator(static_cast<unsigned>(std::chrono::system_clock::now().time_since_epoch().count()));
        std::uniform_int_distribution<int> distribution(0, static_cast<int>(available_list.size() - 1));
        int random_number = distribution(generator);
        Point result = available_list[random_number];
        // Send your action to stdout
        Json::Value output;
        output["response"]["x"] = result.x;
        output["response"]["y"] = result.y;
        self_decision_history.push_back(result);
        board.set(result, SELF);
        Json::FastWriter writer;
        std::cout << writer.write(output) << std::endl;

        // If you want to keep running, send KEEP_RUNNING_CMD to stdout
        std::cout << KEEP_RUNNING_CMD << std::endl;
    }
}

#else
int main() {
    srand((unsigned) time(0));
    memset(board, 0, sizeof(board));
    Json::Value action;
#pragma region DecideAction
    // 做出决策存为myAction
    int turnID = receive();
    //以下为随机策略
    std::vector<int> available_list; //合法位置表

    for (int i = 0; i < 9; i++)
        for (int j = 0; j < 9; j++)
            if (judgeAvailable(i, j, x == -1 ? 1 : -1))
                available_list.push_back(i * 9 + j);
    int result = available_list[rand() % available_list.size()];

    action["x"] = result / 9;
    action["y"] = result % 9;

#pragma endregion
    Json::Value ret;
#pragma region Output
    // 输出决策JSON
    /*
     {
        "response" : "response msg", // Bot 此回合的输出信息（response）
        "debug" : "debug info", // 调试信息，将被写入log，最大长度为1KB
        "data" : "saved data" // Bot 此回合的保存信息，将在下回合输入【注意不会保留在 Log 中】
        "globaldata" : "globally saved data" // Bot 的全局保存信息，将会在下回合输入，对局结束后也会保留，下次对局可以继续利用【注意不会保留在 Log 中】
    }
     */
    ret["response"] = action;
    ret["data"] = ""; // 可以存储一些前述的信息，在整个对局中使用
    Json::FastWriter writer;
    std::cout << writer.write(ret) << std::endl;
    return 0;
#pragma endregion
}
#endif // _NOT_BOTZONE_LONGRUN