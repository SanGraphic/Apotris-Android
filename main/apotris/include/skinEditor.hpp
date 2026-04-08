#pragma once

#include "platform.hpp"
#include "scene.hpp"

const static int xoffset = 15 - 8;
const static int yoffset = 10 - 8;

class Cursor {
public:
    int x = 0;
    int y = 0;
    OBJ_ATTR* sprite;

    void show(bool mini) {
        int sx = (x + mini) * 16 + xoffset * 8;
        int sy = (y + mini) * 16 + yoffset * 8 +
                 8 * (savefile->settings.aspectRatio != 0);

        sprite = &obj_buffer[20];
        sprite_unhide(sprite, 0);
        sprite_set_attr(sprite, ShapeSquare, 1, 712, 15, 0);
        sprite_set_pos(sprite, sx, sy);
    }

    bool move(int dx, int dy, bool mini) {
        bool sound = false;
        bool moved = false;

        if (dx) {
            if (x + dx >= 0 && x + dx <= (7 - (mini * 2))) {
                x += dx;
                sfx(SFX_SHIFT2);
                moved = true;
                sound = true;
            }
        }
        if (dy) {
            if (y + dy >= 0 && y + dy <= 7 - (mini * 2)) {
                y += dy;
                moved = true;
                if (!sound)
                    sfx(SFX_SHIFT2);
            }
        }

        return moved;
    }

    Cursor() {}
};

class Move {
public:
    int x = 0;
    int y = 0;

    int previousValue = 0;
    int currentValue = 0;
    int tool = 0;

    Move(int _x, int _y, int previous, int current, int _tool) {
        x = _x;
        y = _y;

        previousValue = previous;
        currentValue = current;

        tool = _tool;
    }
};

class Board {
public:
    int board[8][8];
    Cursor cursor;

    std::list<Move> history;

    std::list<std::tuple<int, int>> queue;

    void set(int value, bool record) {
        if (record)
            history.push_back(
                Move(cursor.x, cursor.y, board[cursor.y][cursor.x], value, 1));

        if (history.size() > 10)
            history.pop_front();

        board[cursor.y][cursor.x] = value;
        int s = (cursor.x) * 4;
        customSkin->data[cursor.y] =
            (customSkin->data[cursor.y] & ~(0xf << (s))) |
            ((value + (value == 5)) << (s));
    }

    void fill(int value, int record) {
        int previous = board[cursor.y][cursor.x];
        if (value == previous)
            return;
        if (record)
            history.push_back(Move(cursor.x, cursor.y, previous, value, 2));

        int x, y;

        queue.push_back(std::make_tuple(cursor.x, cursor.y));

        do {
            std::tuple<int, int> coords = queue.front();
            x = std::get<0>(coords);
            y = std::get<1>(coords);

            board[y][x] = value;
            int s = (x) * 4;
            customSkin->data[y] = (customSkin->data[y] & ~(0xf << (s))) |
                                  ((value + (value == 5)) << (s));

            if (y - 1 >= 0 && board[y - 1][x] == previous) {
                auto n = std::make_tuple(x, y - 1);
                if (std::find(queue.begin(), queue.end(), n) == queue.end())
                    queue.push_back(n);
            }
            if (y + 1 <= 7 && board[y + 1][x] == previous) {
                auto n = std::make_tuple(x, y + 1);
                if (std::find(queue.begin(), queue.end(), n) == queue.end())
                    queue.push_back(n);
            }
            if (x - 1 >= 0 && board[y][x - 1] == previous) {
                auto n = std::make_tuple(x - 1, y);
                if (std::find(queue.begin(), queue.end(), n) == queue.end())
                    queue.push_back(n);
            }
            if (x + 1 <= 7 && board[y][x + 1] == previous) {
                auto n = std::make_tuple(x + 1, y);
                if (std::find(queue.begin(), queue.end(), n) == queue.end())
                    queue.push_back(n);
            }

            queue.pop_front();
        } while (!queue.empty());
    }

    void clear() {
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++)
                board[i][j] = 0;

            customSkin->data[i] = 0;
        }
    }

    void show(bool mini) {

        for (int i = 0; i < 8; i++) {
            int y =
                ((i * 2) + yoffset) + 1 * (savefile->settings.aspectRatio != 0);
            for (int j = 0; j < 8; j++) {
                int x = j * 2 + xoffset;
                if (!mini) {
                    if (board[i][j] != 0) {
                        int n = board[i][j] + 100 - (board[i][j] == 6);
                        setTile(25, x, y, tileBuild(n, false, false, 0));
                        setTile(25, x + 1, y, tileBuild(n, false, false, 0));
                        setTile(25, x, y + 1, tileBuild(n, false, false, 0));
                        setTile(25, x + 1, y + 1,
                                tileBuild(n, false, false, 0));
                    } else {
                        setTile(25, x, y, tileBuild(0x6a, false, false, 15));
                        setTile(25, x + 1, y,
                                tileBuild(0x6b, false, false, 15));
                        setTile(25, x, y + 1,
                                tileBuild(0x6c, false, false, 15));
                        setTile(25, x + 1, y + 1,
                                tileBuild(0x6d, false, false, 15));
                    }
                } else {
                    if (i == 0 || j == 0 || i > 6 || j > 6) {
                        setTile(25, x, y, 0);
                        setTile(25, x + 1, y, 0);
                        setTile(25, x, y + 1, 0);
                        setTile(25, x + 1, y + 1, 0);
                    } else if (board[i - 1][j - 1] != 0) {
                        int n = board[i - 1][j - 1] + 100 -
                                (board[i - 1][j - 1] == 6);
                        setTile(25, x, y, tileBuild(n, false, false, 0));
                        setTile(25, x + 1, y, tileBuild(n, false, false, 0));
                        setTile(25, x, y + 1, tileBuild(n, false, false, 0));
                        setTile(25, x + 1, y + 1,
                                tileBuild(n, false, false, 0));
                    } else {
                        setTile(25, x, y, tileBuild(0x6a, false, false, 15));
                        setTile(25, x + 1, y,
                                tileBuild(0x6b, false, false, 15));
                        setTile(25, x, y + 1,
                                tileBuild(0x6c, false, false, 15));
                        setTile(25, x + 1, y + 1,
                                tileBuild(0x6d, false, false, 15));
                    }
                }
            }
        }

        cursor.show(mini);
    }

    bool undo() {
        if (history.empty())
            return false;

        Move previous = history.back();

        cursor.x = previous.x;
        cursor.y = previous.y;

        switch (previous.tool) {
        case 1:
            set(previous.previousValue, false);
            break;
        case 2:
            fill(previous.previousValue, false);
            break;
        }

        history.pop_back();

        return true;
    }

    void setBoard(TILE* t, int type) {
        if (type == 0) {
            for (int i = 0; i < 8; i++) {
                for (int j = 0; j < 8; j++) {
                    board[i][j] = (t->data[i] >> (j * 4)) & 0xf;
                }
            }
        } else {
            for (int i = 0; i < 8; i++) {
                for (int j = 0; j < 8; j++) {
                    if (i < 6 && j < 6)
                        board[i][j] = (t->data[i] >> ((j) * 4)) & 0xf;
                    else
                        board[i][j] = 0;
                }
            }
        }
    }

    Board() {
        for (int i = 0; i < 8; i++)
            for (int j = 0; j < 8; j++)
                board[i][j] = 0;
    }
};

class EditorScene : public Scene {
    void draw();
    void update();
    bool control();
    void init();
    void deinit();
    Scene* previousScene() { return NULL; };

    int currentColor = 2;
    int currentTool = 1;
    int prevBld;

    u8 das = 0;
    const int arr = 4;
    const int dasMax = 20;

    u8 timer = 0;

    Board board;

    ~EditorScene() { deinit(); }
};
