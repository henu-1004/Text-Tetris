#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64)
    #define OS_WINDOWS
    #include <conio.h>
    #include <windows.h>
#else
    #define OS_POSIX
    #include <unistd.h>
    #include <termios.h>
    #include <sys/time.h>
    #include <sys/select.h>
#endif

#define LEFT    0
#define RIGHT   1
#define DOWN    2
#define ROTATE  3

#define I_BLOCK 0
#define T_BLOCK 1
#define S_BLOCK 2
#define Z_BLOCK 3
#define L_BLOCK 4
#define J_BLOCK 5
#define O_BLOCK 6

#define GAME_START 0
#define GAME_END   1

#define ROW 21
#define COL 10
#define BLOCK_SIZE 4
#define RECORD_FILE "tetris_records.txt"
#define LINE_SCORE 100

static const char i_block[4][4][4] = {
    {{1,1,1,1},{0,0,0,0},{0,0,0,0},{0,0,0,0}},
    {{0,0,0,1},{0,0,0,1},{0,0,0,1},{0,0,0,1}},
    {{0,0,0,0},{0,0,0,0},{0,0,0,0},{1,1,1,1}},
    {{1,0,0,0},{1,0,0,0},{1,0,0,0},{1,0,0,0}}
};
static const char t_block[4][4][4] = {
    {{1,0,0,0},{1,1,0,0},{1,0,0,0},{0,0,0,0}},
    {{1,1,1,0},{0,1,0,0},{0,0,0,0},{0,0,0,0}},
    {{0,0,1,0},{0,1,1,0},{0,0,1,0},{0,0,0,0}},
    {{0,0,0,0},{0,1,0,0},{1,1,1,0},{0,0,0,0}}
};
static const char s_block[4][4][4] = {
    {{1,0,0,0},{1,1,0,0},{0,1,0,0},{0,0,0,0}},
    {{0,1,1,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},
    {{0,1,0,0},{0,1,1,0},{0,0,1,0},{0,0,0,0}},
    {{0,0,0,0},{0,1,1,0},{1,1,0,0},{0,0,0,0}}
};
static const char z_block[4][4][4] = {
    {{0,1,0,0},{1,1,0,0},{1,0,0,0},{0,0,0,0}},
    {{1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
    {{0,0,1,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}},
    {{0,0,0,0},{1,1,0,0},{0,1,1,0},{0,0,0,0}}
};
static const char l_block[4][4][4] = {
    {{1,0,0,0},{1,0,0,0},{1,1,0,0},{0,0,0,0}},
    {{1,1,1,0},{1,0,0,0},{0,0,0,0},{0,0,0,0}},
    {{0,1,1,0},{0,0,1,0},{0,0,1,0},{0,0,0,0}},
    {{0,0,0,0},{0,0,1,0},{1,1,1,0},{0,0,0,0}}
};
static const char j_block[4][4][4] = {
    {{0,1,0,0},{0,1,0,0},{1,1,0,0},{0,0,0,0}},
    {{1,0,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
    {{1,1,0,0},{1,0,0,0},{1,0,0,0},{0,0,0,0}},
    {{0,0,0,0},{0,0,1,0},{1,1,1,0},{0,0,0,0}}
};
static const char o_block[4][4][4] = {
    {{1,1,0,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},
    {{1,1,0,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},
    {{1,1,0,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},
    {{1,1,0,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}}
};
static const char (*block_list[7])[4][4] = {
    i_block, t_block, s_block, z_block, l_block, j_block, o_block
};

typedef struct {
    char name[30];
    long point;
    int year, month, day, hour, min, rank;
} result;

static char tetris_table[ROW][COL];
static int block_number, next_block_number, block_rot, cur_x, cur_y;
static int game_state;
static long score, best_score;

int display_menu(void);
int game_start(void);
void search_result(void);
void print_result(void);
void load_best_score(void);
void save_record(const result *r);
result *load_all_records(int *count);
void assign_ranks(result *recs, int count);
int cmp_point(const void *a, const void *b);
void init_board(void);
void draw_board(void);
int can_move(int nx, int ny, int nr);
void fix_block(void);
void clear_lines(void);
void spawn_block(void);

/* 플랫폼 추상화 */
#ifdef OS_POSIX
static struct termios orig_term;
void reset_terminal(void) { tcsetattr(STDIN_FILENO, TCSANOW, &orig_term); }
void set_conio_terminal(void) {
    struct termios new_term;
    tcgetattr(STDIN_FILENO, &orig_term);
    new_term = orig_term;
    new_term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_term);
}
#else
void reset_terminal(void) {}
void set_conio_terminal(void) {}
#endif

void sleep_ms(int ms) {
#ifdef OS_WINDOWS
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

int kbhit(void) {
#ifdef OS_WINDOWS
    return _kbhit();
#else
    struct timeval tv = {0L, 0L};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
#endif
}

#ifdef OS_WINDOWS
#else
char getch(void) {
    char c;
    if (read(STDIN_FILENO, &c, 1) < 0) return 0;
    return c;
}
#endif

void clear_screen(void) {
#ifdef OS_WINDOWS
    system("cls");
#else
    (void)system("clear");
#endif
}

long get_millisec(void) {
#ifdef OS_WINDOWS
    return GetTickCount();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

void init_board(void) {
    for (int i = 0; i < ROW; i++) {
        for (int j = 0; j < COL; j++) {
            if (i == ROW - 1) {
                tetris_table[i][j] = '-';
            } else if (j == 0 || j == COL - 1) {
                tetris_table[i][j] = '|';
            } else {
                tetris_table[i][j] = ' ';
            }
        }
    }
}

void draw_board(void) {
    clear_screen();
    printf("Score: %ld   Best: %ld\n", score, best_score);

    for (int j = 0; j < COL; j++) {
        putchar('-');
    }
    putchar('\n');

    for (int i = 0; i < ROW; i++) {
        for (int j = 0; j < COL; j++) {
            int printed = 0;
            for (int y = 0; y < BLOCK_SIZE; y++) {
                for (int x = 0; x < BLOCK_SIZE; x++) {
                    if (block_list[block_number][block_rot][y][x] &&
                        cur_y + y == i && cur_x + x == j) {
                        putchar('#');
                        printed = 1;
                    }
                }
            }
            if (!printed) {
                putchar(tetris_table[i][j]);
            }
        }
        putchar('\n');
    }

    printf("\nNext Block:\n");
    for (int y = 0; y < BLOCK_SIZE; y++) {
        printf("  ");
        for (int x = 0; x < BLOCK_SIZE; x++) {
            putchar(block_list[next_block_number][0][y][x] ? '#' : ' ');
        }
        putchar('\n');
    }
}

int can_move(int nx, int ny, int nr) {
    for (int y = 0; y < BLOCK_SIZE; y++) {
        for (int x = 0; x < BLOCK_SIZE; x++) {
            if (block_list[block_number][nr][y][x]) {
                int tx = nx + x, ty = ny + y;
                if (tx < 0 || tx >= COL || ty < 0 || ty >= ROW) {
                    return 0;
                }
                if (tetris_table[ty][tx] != ' ') {
                    return 0;
                }
            }
        }
    }
    return 1;
}

void fix_block(void) {
    for (int y = 0; y < BLOCK_SIZE; y++) {
        for (int x = 0; x < BLOCK_SIZE; x++) {
            if (block_list[block_number][block_rot][y][x]) {
                tetris_table[cur_y + y][cur_x + x] = '#';
            }
        }
    }
}

// 라인 삭제 및 점수 계산
void clear_lines(void) {
    for (int i = ROW - 2; i >= 0; i--) {
        int full = 1;
        for (int j = 1; j < COL - 1; j++) {
            if (tetris_table[i][j] != '#') {
                full = 0;
                break;
            }
        }
        if (full) {
            score += LINE_SCORE;
            if (score > best_score) {
                best_score = score;
            }
            for (int k = i; k > 0; k--) {
                for (int j = 1; j < COL - 1; j++) {
                    tetris_table[k][j] = tetris_table[k - 1][j];
                }
            }
            i++;
        }
    }
}

void spawn_block(void) {
    block_number = next_block_number;
    next_block_number = rand() % 7;
    block_rot = 0;
    cur_x = (COL - BLOCK_SIZE) / 2;
    cur_y = 0;
    if (!can_move(cur_x, cur_y, block_rot)) {
        game_state = GAME_END;
    }
}

void load_best_score(void) {
    int count;
    result *all = load_all_records(&count);
    best_score = 0;
    for (int i = 0; i < count; i++) {
        if (all[i].point > best_score) {
            best_score = all[i].point;
        }
    }
    free(all);
}

void save_record(const result *r) {
    FILE *f = fopen(RECORD_FILE, "a");
    if (!f) return;
    fprintf(f, "%s %ld %04d-%02d-%02d %02d:%02d\n",
        r->name, r->point,
        r->year, r->month, r->day,
        r->hour, r->min);
    fclose(f);
}

result *load_all_records(int *count) {
    FILE *f = fopen(RECORD_FILE, "r");
    if (!f) {
        *count = 0;
        return NULL;
    }
    result *arr = NULL;
    int cap = 0, n = 0;
    while (!feof(f)) {
        result tmp;
        if (fscanf(f, "%29s %ld %d-%d-%d %d:%d",
                   tmp.name, &tmp.point,
                   &tmp.year, &tmp.month, &tmp.day,
                   &tmp.hour, &tmp.min) == 7) {
            tmp.rank = 0;
            if (n >= cap) {
                cap = cap ? cap * 2 : 8;
                arr = realloc(arr, cap * sizeof(result));
            }
            arr[n++] = tmp;
        }
    }
    fclose(f);
    *count = n;
    return arr;
}

int cmp_point(const void *a, const void *b) {
    return (int)(((result*)b)->point - ((result*)a)->point);
}

void assign_ranks(result *recs, int count) {
    qsort(recs, count, sizeof(result), cmp_point);
    for (int i = 0; i < count; i++) {
        if (i > 0 && recs[i].point == recs[i-1].point) {
            recs[i].rank = recs[i-1].rank;
        } else {
            recs[i].rank = i + 1;
        }
    }
}

void search_result(void) {
    clear_screen();
    printf("Search by name: ");
    char query[30];
    if (scanf("%29s", query) != 1) {
        query[0] = '\0';
    }
    int n;
    result *all = load_all_records(&n);
    if (!all) {
        printf("No records.\n");
        getch();
        return;
    }
    assign_ranks(all, n);
    int found = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(query, all[i].name) == 0) {
            printf("%s %ld %04d-%02d-%02d %02d:%02d Rank:%d\n",
                   all[i].name, all[i].point,
                   all[i].year, all[i].month, all[i].day,
                   all[i].hour, all[i].min, all[i].rank);
            found = 1;
        }
    }
    if (!found) {
        printf("No matching record.\n");
    }
    free(all);
    printf("Press any key...");
    getch();
}

void print_result(void) {
    clear_screen();
    int n;
    result *all = load_all_records(&n);
    if (!all) {
        printf("No records.\n");
        getch();
        return;
    }
    assign_ranks(all, n);
    for (int i = 0; i < n; i++) {
        printf("%s %ld %04d-%02d-%02d %02d:%02d Rank:%d\n",
               all[i].name, all[i].point,
               all[i].year, all[i].month, all[i].day,
               all[i].hour, all[i].min, all[i].rank);
    }
    free(all);
    printf("Press any key...");
    getch();
}

int display_menu(void) {
    int menu;
    while (1) {
        clear_screen();
        printf("\nText Tetris\n");
        printf("1) Game Start\n");
        printf("2) Search history\n");
        printf("3) Record Output\n");
        printf("4) QUIT\n");
        printf("Select: ");
        if (scanf("%d", &menu) == 1 && menu >= 1 && menu <= 4) {
            return menu;
        }
        while (getchar() != '\n');
    }
}

//게임 시작
int game_start(void) {
    set_conio_terminal();
    srand((unsigned)time(NULL));
    score = 0;
    next_block_number = rand() % 7;
    init_board();
    game_state = GAME_START;
    load_best_score();
    spawn_block();

    long last_fall_time = get_millisec();
    const int fall_interval = 500;  // 자동 낙하 주기

    while (game_state == GAME_START) {
        draw_board();

        if (kbhit()) {
            char c = getch();
            if (c == 'j' || c == 'J') {
                if (can_move(cur_x - 1, cur_y, block_rot)) {
                    cur_x--;
                }
            } else if (c == 'l' || c == 'L') {
                if (can_move(cur_x + 1, cur_y, block_rot)) {
                    cur_x++;
                }
            } else if (c == 'k' || c == 'K') {
                if (can_move(cur_x, cur_y + 1, block_rot)) {
                    cur_y++;
                } else {
                    fix_block();
                    clear_lines();
                    spawn_block();
                    last_fall_time = get_millisec();
                }
            } else if (c == 'i' || c == 'I') {
                int nr = (block_rot + 1) % 4;
                if (can_move(cur_x, cur_y, nr)) {
                    block_rot = nr;
                }
            } else if (c == 'a' || c == 'A') {  
                while (can_move(cur_x, cur_y + 1, block_rot)) {
                    cur_y++;
                }
                fix_block();
                clear_lines();
                spawn_block();
                last_fall_time = get_millisec();
            } else if (c == 'p' || c == 'P') {
                break;
            }
        }

        // 자동 낙하 (입력과 독립적으로)
        long now = get_millisec();
        if (now - last_fall_time >= fall_interval) {
            if (can_move(cur_x, cur_y + 1, block_rot)) {
                cur_y++;
            } else {
                fix_block();
                clear_lines();
                spawn_block();
            }
            last_fall_time = now;
        }

        sleep_ms(50);
    }

    draw_board();
    printf("\nGame Over! Your score: %ld\nEnter name: ", score);
    result r;
    if (scanf("%29s", r.name) != 1) {
        strcpy(r.name, "NONAME");
    }
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    r.point = score;
    r.year  = lt->tm_year + 1900;
    r.month = lt->tm_mon + 1;
    r.day   = lt->tm_mday;
    r.hour  = lt->tm_hour;
    r.min   = lt->tm_min;
    save_record(&r);

    reset_terminal();
    printf("Record saved. Press any key to return to menu...");
    getch();
    return 1;
}

int main(void) {
    int menu = 1;
    while (menu) {
        menu = display_menu();
        if (menu == 1) {
            menu = game_start();
        } else if (menu == 2) {
            search_result();
        } else if (menu == 3) {
            print_result();
        } else {
            break;
        }
    }
    return 0;
}
