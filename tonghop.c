#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>

#define MAX_LINE 80 /* The maximum length command */
#define HISTORY_SIZE 10 /* The maximum number of commands to store in history */

pid_t child_pid = -1; // Lưu trữ PID của tiến trình con
char history[HISTORY_SIZE][MAX_LINE]; // Mảng lưu lịch sử các lệnh
int history_count = 0; // Đếm số lệnh đã lưu vào lịch sử

void handle_sigint(int sig) {
    if (child_pid != -1) {
        kill(child_pid, SIGINT); // Gửi tín hiệu SIGINT tới tiến trình con
    }
}

void parse_command(char *input, char **args, int *argc, int *has_pipe, char **args_pipe, char **input_file, char **output_file) {
    *argc = 0;
    *has_pipe = 0;
    *input_file = NULL;
    *output_file = NULL;
    char *token = strtok(input, " ");
    while (token != NULL) {
        if (strcmp(token, "|") == 0) {
            *has_pipe = 1;
            args[*argc] = NULL;
            *argc = 0;
            token = strtok(NULL, " ");
            while (token != NULL) {
                args_pipe[*argc] = token;
                token = strtok(NULL, " ");
                (*argc)++;
            }
            args_pipe[*argc] = NULL;
            return;
        } else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, " ");
            if (token != NULL) {
                *output_file = token;
            } else {
                fprintf(stderr, "No output file specified\n");
                return;
            }
        } else if (strcmp(token, "<") == 0) {
            token = strtok(NULL, " ");
            if (token != NULL) {
                *input_file = token;
            } else {
                fprintf(stderr, "No input file specified\n");
                return;
            }
        } else {
            args[(*argc)++] = token;
        }
        token = strtok(NULL, " ");
    }
    args[*argc] = NULL;
}

void add_to_history(const char *input) {
    if (history_count < HISTORY_SIZE) {
        strcpy(history[history_count], input);
        history_count++;
    } else {
        for (int i = 1; i < HISTORY_SIZE; i++) {
            strcpy(history[i - 1], history[i]);
        }
        strcpy(history[HISTORY_SIZE - 1], input);
    }
}

void get_from_history(int index, char *buffer) {
    if (index >= 0 && index < history_count) {
        strcpy(buffer, history[index]);
    } else {
        buffer[0] = '\0';
    }
}

int main(void) {
    char *args[MAX_LINE / 2 + 1]; /* command line arguments */
    char *args_pipe[MAX_LINE / 2 + 1]; /* command line arguments for pipe */
    int should_run = 1; /* flag to determine when to exit program */
    int history_index = -1; // Chỉ số hiện tại trong lịch sử lệnh

    // Thiết lập bộ xử lý tín hiệu SIGINT
    signal(SIGINT, handle_sigint);

    while (should_run) {
        printf("it007sh> ");
        fflush(stdout);

        char input[MAX_LINE] = {0};
        int pos = 0;
        char ch;
        struct termios oldt, newt;

        // Lấy cài đặt terminal hiện tại và điều chỉnh để đọc từng ký tự một
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);

        while (1) {
            ch = getchar();

            if (ch == '\n') {
                input[pos] = '\0';
                putchar(ch);
                break;
            } else if (ch == 127) { // Xóa ký tự (phím Backspace)
                if (pos > 0) {
                    pos--;
                    printf("\b \b");
                    fflush(stdout);
                }
            } else if (ch == 27) { // Phím ESC
                getchar(); // Bỏ qua ký tự [
                ch = getchar();
                if (ch == 'A') { // Mũi tên lên
                    if (history_index < history_count - 1) {
                        history_index++;
                        get_from_history(history_count - 1 - history_index, input);
                        printf("\r\33[Kit007sh> %s", input);
                        pos = strlen(input);
                        fflush(stdout);
                    }
                } else if (ch == 'B') { // Mũi tên xuống
                    if (history_index > 0) {
                        history_index--;
                        get_from_history(history_count - 1 - history_index, input);
                        printf("\r\33[Kit007sh> %s", input);
                        pos = strlen(input);
                        fflush(stdout);
                    } else if (history_index == 0) {
                        history_index--;
                        printf("\r\33[Kit007sh> ");
                        fflush(stdout);
                        pos = 0;
                    }
                } else if (ch == 'C') { // Mũi tên phải
                    // Di chuyển con trỏ sang phải nếu có ký tự
                    if (pos < strlen(input)) {
                        pos++;
                        printf("\033[C");
                    }
                } else if (ch == 'D') { // Mũi tên trái
                    // Di chuyển con trỏ sang trái nếu không ở đầu dòng
                    if (pos > 0) {
                        pos--;
                        printf("\033[D");
                    }
                }
            } else {
                if (pos < MAX_LINE - 1) {
                    input[pos++] = ch;
                    putchar(ch);
                    fflush(stdout);
                }
            }
        }

        // Khôi phục cài đặt terminal gốc
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

        if (pos > 0) {
            add_to_history(input);
            history_index = -1; // Đặt lại chỉ số lịch sử
        }

        int argc = 0, has_pipe = 0;
        char *input_file = NULL;
        char *output_file = NULL;
        parse_command(input, args, &argc, &has_pipe, args_pipe, &input_file, &output_file);

        // Kiểm tra lệnh đặc biệt "exit" để thoát khỏi shell
        if (argc > 0 && strcmp(args[0], "exit") == 0) {
            should_run = 0;
            continue;
        }

        if (has_pipe) {
            int pipefd[2];
            if (pipe(pipefd) == -1) {
                perror("pipe failed");
                continue;
            }

            pid_t pid1 = fork();
            if (pid1 < 0) {
                perror("fork failed");
                continue;
            }

            if (pid1 == 0) { // Tiến trình con 1
                close(pipefd[0]); // Đóng đầu đọc của đường ống
                dup2(pipefd[1], STDOUT_FILENO); // Chuyển hướng đầu ra của tiến trình con 1 tới đầu ghi của đường ống
                close(pipefd[1]);
                if (input_file) {
                    int fd_in = open(input_file, O_RDONLY);
                    if (fd_in < 0) {
                        perror("open failed");
                        return 1;
                    }
                    dup2(fd_in, STDIN_FILENO);
                    close(fd_in);
                }
                if (execvp(args[0], args) < 0) {
                    perror("execvp failed");
                    return 1;
                }
            }

            pid_t pid2 = fork();
            if (pid2 < 0) {
                perror("fork failed");
                continue;
            }

            if (pid2 == 0) { // Tiến trình con 2
                close(pipefd[1]); // Đóng đầu ghi của đường ống
                dup2(pipefd[0], STDIN_FILENO); // Chuyển hướng đầu vào của tiến trình con 2 tới đầu đọc của đường ống
                close(pipefd[0]);
                if (output_file) {
                    int fd_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd_out < 0) {
                        perror("open failed");
                        return 1;
                    }
                    dup2(fd_out, STDOUT_FILENO);
                    close(fd_out);
                }
                if (execvp(args_pipe[0], args_pipe) < 0) {
                    perror("execvp failed");
                    return 1;
                }
            }

            close(pipefd[0]);
            close(pipefd[1]);
            waitpid(pid1, NULL, 0); // Đợi tiến trình con 1 hoàn thành
            waitpid(pid2, NULL, 0); // Đợi tiến trình con 2 hoàn thành

        } else {
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork failed");
                continue;
            }

            if (pid == 0) { // Tiến trình con
                if (output_file) {
                    int fd_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd_out < 0) {
                        perror("open failed");
                        return 1;
                    }
                    dup2(fd_out, STDOUT_FILENO);
                    close(fd_out);
                }
                if (input_file) {
                    int fd_in = open(input_file, O_RDONLY);
                    if (fd_in < 0) {
                        perror("open failed");
                        return 1;
                    }
                    dup2(fd_in, STDIN_FILENO);
                    close(fd_in);
                }
                if (execvp(args[0], args) < 0) {
                    perror("execvp failed");
                    return 1;
                }
            } else { // Tiến trình cha
                waitpid(pid, NULL, 0); // Đợi tiến trình con hoàn thành
            }
        }
    }
    return 0;
}
