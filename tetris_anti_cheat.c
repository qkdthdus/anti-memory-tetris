#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <windows.h>
#include <time.h>
#include <windows.h>
#include <wincrypt.h>
#include <string.h>

#define WIDTH 12   // 벽 포함 가로(10칸 + 양쪽 벽)
#define HEIGHT 22  // 벽 포함 세로(20칸 + 위아래 벽)
#define BLOCK_SIZE 4

int board[HEIGHT][WIDTH];
int encoded_score = 0;
int score_key = 0;
int paused = 0;

// 함수 프로토타입 선언 (한 번씩만)
void check_score_legit(int new_score, int lines_cleared);
void check_score_anomaly(int new_score);
void gotoxy(int x, int y);
void init_board(void);
void print_score(void);
int get_score(void);
void set_score(int s);

// 블록 상태 변수
int cur_block, cur_rot, cur_x, cur_y;

// 테트리스 블록 데이터 (7종류, 4회전, 4x4)
int blocks[7][4][BLOCK_SIZE][BLOCK_SIZE] = {
    // I 블록
    {
        { {0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0} },
        { {0,0,1,0}, {0,0,1,0}, {0,0,1,0}, {0,0,1,0} },
        { {0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0} },
        { {0,1,0,0}, {0,1,0,0}, {0,1,0,0}, {0,1,0,0} }
    },
    // O 블록
    {
        { {0,0,0,0}, {0,1,1,0}, {0,1,1,0}, {0,0,0,0} },
        { {0,0,0,0}, {0,1,1,0}, {0,1,1,0}, {0,0,0,0} },
        { {0,0,0,0}, {0,1,1,0}, {0,1,1,0}, {0,0,0,0} },
        { {0,0,0,0}, {0,1,1,0}, {0,1,1,0}, {0,0,0,0} }
    },
    // T 블록
    {
        { {0,0,0,0}, {1,1,1,0}, {0,1,0,0}, {0,0,0,0} },
        { {0,1,0,0}, {1,1,0,0}, {0,1,0,0}, {0,0,0,0} },
        { {0,1,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} },
        { {0,1,0,0}, {0,1,1,0}, {0,1,0,0}, {0,0,0,0} }
    },
    // S 블록
    {
        { {0,0,0,0}, {0,1,1,0}, {1,1,0,0}, {0,0,0,0} },
        { {1,0,0,0}, {1,1,0,0}, {0,1,0,0}, {0,0,0,0} },
        { {0,0,0,0}, {0,1,1,0}, {1,1,0,0}, {0,0,0,0} },
        { {1,0,0,0}, {1,1,0,0}, {0,1,0,0}, {0,0,0,0} }
    },
    // Z 블록
    {
        { {0,0,0,0}, {1,1,0,0}, {0,1,1,0}, {0,0,0,0} },
        { {0,1,0,0}, {1,1,0,0}, {1,0,0,0}, {0,0,0,0} },
        { {0,0,0,0}, {1,1,0,0}, {0,1,1,0}, {0,0,0,0} },
        { {0,1,0,0}, {1,1,0,0}, {1,0,0,0}, {0,0,0,0} }
    },
    // J 블록
    {
        { {0,0,0,0}, {1,0,0,0}, {1,1,1,0}, {0,0,0,0} },
        { {0,1,1,0}, {0,1,0,0}, {0,1,0,0}, {0,0,0,0} },
        { {0,0,0,0}, {1,1,1,0}, {0,0,1,0}, {0,0,0,0} },
        { {0,1,0,0}, {0,1,0,0}, {1,1,0,0}, {0,0,0,0} }
    },
    // L 블록
    {
        { {0,0,0,0}, {0,0,1,0}, {1,1,1,0}, {0,0,0,0} },
        { {0,1,0,0}, {0,1,0,0}, {0,1,1,0}, {0,0,0,0} },
        { {0,0,0,0}, {1,1,1,0}, {1,0,0,0}, {0,0,0,0} },
        { {1,1,0,0}, {0,1,0,0}, {0,1,0,0}, {0,0,0,0} }
    }
};

// 자기변형 코드용 변수
unsigned char get_score_code[128];
size_t get_score_size = 0;
unsigned char set_score_code[128];
size_t set_score_size = 0;
unsigned char xor_key = 0x5A;

// get_score의 실제 구현
int __declspec(noinline) get_score_impl() {
    int s = encoded_score ^ score_key;
    if (s < 0 || s > 999999) {
        MessageBox(0, "Cheat Detected!", "Warning", MB_OK);
        exit(1);
    }
    return s;
}
// set_score의 실제 구현
void __declspec(noinline) set_score_impl(int s) {
    score_key = rand();
    encoded_score = s ^ score_key;
}

// 메모리 보호 속성 변경 함수
void make_code_writable(void* addr, size_t size) {
    DWORD old;
    VirtualProtect(addr, size, PAGE_EXECUTE_READWRITE, &old);
}

// get_score 자기변형 래퍼
int get_score(void) {
    int s = encoded_score ^ score_key;
    if (s < 0 || s > 999999) {
        MessageBox(0, "Cheat Detected!", "Warning", MB_OK);
        exit(1);
    }
    return s;
}
// set_score 자기변형 래퍼
void set_score(int s) {
    score_key = rand();
    encoded_score = s ^ score_key;
}

// 프로그램 시작 시 코드 복사/암호화
void setup_selfmodifying() {
    // get_score_impl 함수 코드 복사/암호화
    unsigned char* p1 = (unsigned char*)get_score_impl;
    get_score_size = 64; // 함수 크기(실제 환경에 맞게 조정)
    memcpy(get_score_code, p1, get_score_size);
    for (size_t i = 0; i < get_score_size; i++)
        p1[i] ^= xor_key;
    // set_score_impl 함수 코드 복사/암호화
    unsigned char* p2 = (unsigned char*)set_score_impl;
    set_score_size = 64;
    memcpy(set_score_code, p2, set_score_size);
    for (size_t i = 0; i < set_score_size; i++)
        p2[i] ^= xor_key;
}

// 메인 위쪽에 추가
void gotoxy(int x, int y) {
    COORD pos = { x, y };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
}

// 벽과 빈칸 초기화
void init_board() {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (x == 0 || x == WIDTH-1 || y == HEIGHT-1) // 벽
                board[y][x] = 9;
            else
                board[y][x] = 0;
        }
    }
}

// 점수 출력
void print_score() {
    printf("score: %d\n", get_score());
}

// 게임판 출력
void print_board() {
    // system("cls"); // 이 줄을 삭제 또는 주석처리
    gotoxy(0, 0);
    print_score();
    // 🔽 조작 설명 추가
    printf("Controls: A - Left | D - Right | W - Rotate | S - Soft Drop | Space - Hard Drop | P - Pause\n");
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (board[y][x] == 9)
                printf("#"); // 벽
            else if (board[y][x] == 1)
                printf("*"); // 블록
            else
                printf(" "); // 빈칸
        }
        printf("\n");
    }
    if (paused) {
        printf("\n========== PAUSED ==========\n");
    }
}

// 줄 완성 체크
void check_line() {
    int lines = 0;
    for (int y = 0; y < HEIGHT-1; y++) {
        int full = 1;
        for (int x = 1; x < WIDTH-1; x++) {
            if (board[y][x] == 0) {
                full = 0;
                break;
            }
        }
        if (full) {
            for (int ty = y; ty > 0; ty--) {
                for (int tx = 1; tx < WIDTH-1; tx++) {
                    board[ty][tx] = board[ty-1][tx];
                }
            }
            for (int tx = 1; tx < WIDTH-1; tx++)
                board[0][tx] = 0;
            lines++;
        }
    }
    if (lines > 0) {
        int prev = get_score();
        int add = 0;
        if (lines == 1) add = 100;
        else if (lines == 2) add = 300;
        else if (lines == 3) add = 500;
        else if (lines == 4) add = 800;
        set_score(prev + add);
        check_score_legit(get_score(), lines);
        check_score_anomaly(get_score());
    }
}

void draw_block(int board[HEIGHT][WIDTH], int block_num, int rot, int x, int y, int draw) {
    for (int i = 0; i < BLOCK_SIZE; i++) {
        for (int j = 0; j < BLOCK_SIZE; j++) {
            if (blocks[block_num][rot][i][j]) {
                int board_y = y + i;
                int board_x = x + j;
                if (0 <= board_y && board_y < HEIGHT && 0 <= board_x && board_x < WIDTH) {
                    board[board_y][board_x] = draw ? 1 : 0;
                }
            }
        }
    }
}

int check_collision(int board[HEIGHT][WIDTH], int block_num, int rot, int x, int y) {
    for (int i = 0; i < BLOCK_SIZE; i++) {
        for (int j = 0; j < BLOCK_SIZE; j++) {
            if (blocks[block_num][rot][i][j]) {
                int board_y = y + i;
                int board_x = x + j;
                if (board_y >= HEIGHT ||
                    board_x < 0 ||
                    board_x >= WIDTH ||
                    board[board_y][board_x]) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

void fix_block(int board[HEIGHT][WIDTH], int block_num, int rot, int x, int y) {
    for (int i = 0; i < BLOCK_SIZE; i++) {
        for (int j = 0; j < BLOCK_SIZE; j++) {
            if (blocks[block_num][rot][i][j]) {
                int board_y = y + i;
                int board_x = x + j;
                if (0 <= board_y && board_y < HEIGHT && 0 <= board_x && board_x < WIDTH) {
                    board[board_y][board_x] = 1;
                }
            }
        }
    }
}

// 새 블록 생성 함수
void new_block() {
    cur_block = rand() % 7;
    cur_rot = 0;
    cur_x = WIDTH / 2 - 2;
    cur_y = 0;
}

int last_score = 0;
void check_score_legit(int new_score, int lines_cleared) {
    static int last_score2 = 0;
    int diff = new_score - last_score2;
    int legit = 0;
    if (lines_cleared == 1 && diff == 100) legit = 1;
    if (lines_cleared == 2 && diff == 300) legit = 1;
    if (lines_cleared == 3 && diff == 500) legit = 1;
    if (lines_cleared == 4 && diff == 800) legit = 1;
    if (!legit && diff > 0) {
        printf("[Cheat Alert]\n");
    }
    last_score2 = new_score;
}

DWORD last_score_time = 0;
void check_score_anomaly(int new_score) {
    static int last_score = 0;
    static DWORD last_time = 0;
    DWORD now = GetTickCount();
    int diff = new_score - last_score;
    int tdiff = now - last_time;
    if (diff > 800 && tdiff < 2000) {
        printf("[Cheat Alert]\n");
    }
    last_score = new_score;
    last_time = now;
}

// 2. TLS 콜백 기반 디버거 감지 (GCC)
volatile int tls_debug_flag = 0;
void tls_callback(void* h, unsigned long reason, void* reserved) {
    if (reason == 1) {
        if (IsDebuggerPresent()) tls_debug_flag = 1;
    }
}
#ifdef __GNUC__
__attribute__((section(".CRT$XLB"))) void (*_tls_cb)(void* h, unsigned long reason, void* reserved) = tls_callback;
#endif

// 3. PEB 직접 접근 + IsDebuggerPresent
int is_debugger_attached() {
    int found = 0;
    // PEB 직접 접근 (GCC/MinGW)
    #ifdef __GNUC__
    void* peb = NULL;
    #if defined(__x86_64__) || defined(_M_X64)
        __asm__ __volatile__(
            "movq %%gs:0x60, %0"
            : "=r"(peb)
        );
    #else
        __asm__ __volatile__(
            "movl %%fs:0x30, %0"
            : "=r"(peb)
        );
    #endif
    if (peb) {
        unsigned char being_debugged = *((unsigned char*)peb + 2);
        if (being_debugged) found = 1;
    }
    #endif
    if (IsDebuggerPresent()) found = 1;
    if (found) printf("[Cheat Alert]\n");
    return found;
}

// 4. 점수 급증 감지, 점수 규칙 위반 감지 등은 기존 방식 유지하되 함수명 난독화
void f1(int new_score) { // 점수 급증 감지
    static int last_score = 0;
    static DWORD last_time = 0;
    DWORD now = GetTickCount();
    int diff = new_score - last_score;
    int tdiff = now - last_time;
    if (diff > 800 && tdiff < 2000) {
        printf("[Cheat Alert]\n");
    }
    last_score = new_score;
    last_time = now;
}

void f2(int new_score, int lines_cleared) { // 점수 규칙 위반 감지
    static int last_score2 = 0;
    int diff = new_score - last_score2;
    int legit = 0;
    if (lines_cleared == 1 && diff == 100) legit = 1;
    if (lines_cleared == 2 && diff == 300) legit = 1;
    if (lines_cleared == 3 && diff == 500) legit = 1;
    if (lines_cleared == 4 && diff == 800) legit = 1;
    if (!legit && diff > 0) {
        printf("[Cheat Alert]\n");
    }
    last_score2 = new_score;
}

void print_gameover() {
    gotoxy(0, HEIGHT + 3);
    printf("\n========== GAME OVER ==========");
    printf("\nFinal score: %d\n", get_score());
    printf("\nPress Enter to exit...");
    // 입력 버퍼 비우기
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
    getchar();
}

// 커서 숨김/표시 함수 추가
void hide_cursor() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(hConsole, &cursorInfo);
}
void show_cursor() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = TRUE;
    SetConsoleCursorInfo(hConsole, &cursorInfo);
}

int main() {
    hide_cursor(); // 커서 숨김
    // 4. 디버거 감지
    if (is_debugger_attached()) {
        MessageBox(0, "Debugger Detected!", "Security Alert", MB_OK);
        show_cursor(); // 커서 다시 보이기
        return 1;
    }

    init_board();
    srand(time(NULL));
    int r = rand();
    set_score(0);
    new_block();
    print_board(); // 게임 시작 직후 한 번 출력

    int tick = 0;
    while (1) {
        if (_kbhit()) {
            char ch = _getch();

            if (ch == 'p' || ch == 'P') {
                paused = !paused;
                print_board();
                continue;
            }
            if (paused) continue;

            // 블록 지우기
            draw_block(board, cur_block, cur_rot, cur_x, cur_y, 0);

            if (ch == 'a') { // 왼쪽
                if (!check_collision(board, cur_block, cur_rot, cur_x - 1, cur_y))
                    cur_x--;
            } else if (ch == 'd') { // 오른쪽
                if (!check_collision(board, cur_block, cur_rot, cur_x + 1, cur_y))
                    cur_x++;
            } else if (ch == 'w') { // 회전
                int next_rot = (cur_rot + 1) % 4;
                if (!check_collision(board, cur_block, next_rot, cur_x, cur_y))
                    cur_rot = next_rot;
            } else if (ch == 's') { // 소프트 드롭(한 칸)
                if (!check_collision(board, cur_block, cur_rot, cur_x, cur_y + 1))
                    cur_y++;
            } else if (ch == ' ') { // 하드 드롭(바닥까지)
                while (!check_collision(board, cur_block, cur_rot, cur_x, cur_y + 1))
                    cur_y++;
                draw_block(board, cur_block, cur_rot, cur_x, cur_y, 1);
                print_board();
                fix_block(board, cur_block, cur_rot, cur_x, cur_y);
                check_line();
                new_block();
                // 게임오버 체크
                if (check_collision(board, cur_block, cur_rot, cur_x, cur_y)) {
                    print_gameover();
                    break;
                }
                draw_block(board, cur_block, cur_rot, cur_x, cur_y, 1);
                print_board();
                continue;
            }

            // 블록 다시 그리기
            draw_block(board, cur_block, cur_rot, cur_x, cur_y, 1);
            print_board();
        }

        // 자동 하강 (속도 느리게: tick > 20)
        if (!paused && tick++ > 25) {
            draw_block(board, cur_block, cur_rot, cur_x, cur_y, 0);
            if (!check_collision(board, cur_block, cur_rot, cur_x, cur_y + 1)) {
                cur_y++;
            } else {
                fix_block(board, cur_block, cur_rot, cur_x, cur_y);
                check_line();
                new_block();
                // 게임오버 체크
                if (check_collision(board, cur_block, cur_rot, cur_x, cur_y)) {
                    print_gameover();
                    break;
                }
            }
            draw_block(board, cur_block, cur_rot, cur_x, cur_y, 1);
            print_board();
            tick = 0;
        }
        Sleep(paused ? 100 : 30);
    }
    show_cursor(); // 커서 다시 보이기
    return 0;
}