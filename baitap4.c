#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>

#define MAX_LINE 80 /* The maximum length command */

void parse_command(char *input, char **args, int *argc, int *has_pipe, char **args_pipe) {  //**la de truyen con tro 
    *argc = 0;
    *has_pipe = 0;
    char *token = strtok(input, " ");
    while (token != NULL) {
        if (strcmp(token, "|") == 0) {
            *has_pipe = 1;
            args[*argc] = NULL;
            *argc = 0;
            token = strtok(NULL, " ");  //lay lenh phia sau | 
            while (token != NULL) {
                args_pipe[*argc] = token; // luu dich den cua pipe 
                token = strtok(NULL, " ");
                (*argc)++;
            }
            args_pipe[*argc] = NULL;
            return;
        }
        args[(*argc)++] = token;
        token = strtok(NULL, " ");
    }
    args[*argc] = NULL;
}

int main(void) {
    char *args[MAX_LINE/2 + 1]; /* command line arguments */
    char *args_pipe[MAX_LINE/2 + 1]; /* command line arguments for pipe */
    int should_run = 1; /* flag to determine when to exit program */

    while (should_run) { 
        printf("it007sh> "); 
        fflush(stdout);

        // Đọc lệnh từ người dùng
        char input[MAX_LINE];
        if (fgets(input, MAX_LINE, stdin) == NULL) {
            perror("fgets failed");
            continue;
        }

        // Loại bỏ ký tự newline nếu có
        input[strcspn(input, "\n")] = '\0';

        int argc = 0, has_pipe = 0;
        parse_command(input, args, &argc, &has_pipe, args_pipe);

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
