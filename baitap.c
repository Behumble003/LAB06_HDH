#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>

#define MAX_LINE 80  // Độ dài tối đa của lệnh
#define HISTORY_SIZE 10 // Kích thước của lịch sử lệnh

struct termios orig_termios;
char history[HISTORY_SIZE][MAX_LINE];
int history_count = 0;
int history_index = 0;

void disableRawMode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void add_to_history(const char *command) {
    if (history_count < HISTORY_SIZE) {
        strcpy(history[history_count], command);
        history_count++;
    } else {
        for (int i = 1; i < HISTORY_SIZE; i++) {
            strcpy(history[i - 1], history[i]);
        }
        strcpy(history[HISTORY_SIZE - 1], command);
    }
    history_index = history_count;
}

void print_prompt(void) {
    printf("it007sh> ");
    fflush(stdout);
}

int main(void) {
    enableRawMode();
    char input[MAX_LINE];
    int input_index = 0;
    char c;

    while (1) {
        print_prompt();
        input_index = 0;
        memset(input, 0, sizeof(input));

        while (1) {
            ssize_t nread = read(STDIN_FILENO, &c, 1);
            if (nread == -1 && errno != EAGAIN) {
                perror("read");
                exit(EXIT_FAILURE);
            } else if (nread == 0) {
                continue;
            }

            if (c == '\n') {
                input[input_index] = '\0';
                break;
            } else if (c == 127) { // Xử lý phím backspace
                if (input_index > 0) {
                    input_index--;
                    printf("\b \b");
                    fflush(stdout);
                }
            } else if (c == 27) { // Xử lý phím mũi tên
                char seq[3];
                if (read(STDIN_FILENO, &seq[0], 1) == 1 && read(STDIN_FILENO, &seq[1], 1) == 1) {
                    if (seq[0] == '[') {
                        if (seq[1] == 'A') { // Mũi tên lên
                            if (history_index > 0) {
                                history_index--;
                                strcpy(input, history[history_index]);
                                input_index = strlen(input);
                                printf("\33[2K\r");
                                print_prompt();
                                printf("%s", input);
                                fflush(stdout);
                            }
                        } else if (seq[1] == 'B') { // Mũi tên xuống
                            if (history_index < history_count - 1) {
                                history_index++;
                                strcpy(input, history[history_index]);
                                input_index = strlen(input);
                                printf("\33[2K\r");
                                print_prompt();
                                printf("%s", input);
                                fflush(stdout);
                            } else if (history_index == history_count - 1) {
                                history_index++;
                                input_index = 0;
                                input[0] = '\0';
                                printf("\33[2K\r");
                                print_prompt();
                                fflush(stdout);
                            }
                        }
                    }
                }
            } else {
                input[input_index++] = c;
                printf("%c", c);
                fflush(stdout);
            }
        }

        // Thêm lệnh vào lịch sử nếu lệnh không rỗng
        if (input_index > 0) {
            add_to_history(input);
        }

        // Nếu người dùng nhập "exit", thoát khỏi shell
        if (strcmp(input, "exit") == 0) {
            break;
        }

        // Fork một tiến trình con để thực thi lệnh
        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            continue;
        } else if (pid == 0) {
            // Tiến trình con
            char *args[MAX_LINE / 2 + 1];
            int arg_count = 0;
            char *token = strtok(input, " ");
            while (token != NULL && arg_count < (MAX_LINE / 2 + 1)) {
                args[arg_count++] = token;
                token = strtok(NULL, " ");
            }
            args[arg_count] = NULL;
            if (execvp(args[0], args) == -1) {
                perror("execvp failed");
            }
            exit(EXIT_FAILURE); // Nếu execvp thất bại, thoát tiến trình con
        } else {
            // Tiến trình cha
            int status;
            waitpid(pid, &status, 0); // Chờ tiến trình con hoàn thành
        }
    }

    return 0;
}
