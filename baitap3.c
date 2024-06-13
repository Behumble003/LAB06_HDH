#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>

#define MAX_LINE 80 /* The maximum length command */

int main(void) {
    char *args[MAX_LINE/2 + 1]; /* command line arguments */
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

        // Tách lệnh và các đối số
        int argc = 0;
        char *token = strtok(input, " ");
        int redirect_output = 0, redirect_input = 0;
        char *output_file = NULL, *input_file = NULL;
        
        while (token != NULL) {
            if (strcmp(token, ">") == 0) {
                redirect_output = 1;
                token = strtok(NULL, " ");
                if (token != NULL) {
                    output_file = token;      //cho output_file tro den out.txt
                }
            } else if (strcmp(token, "<") == 0) {
                redirect_input = 1;
                token = strtok(NULL, " ");
                if (token != NULL) {
                    input_file = token;
                }
            } else {
                args[argc++] = token;
            }
            token = strtok(NULL, " ");
        }
        args[argc] = NULL; // Đặt NULL vào cuối danh sách đối số

        // Kiểm tra lệnh đặc biệt "exit" để thoát khỏi shell
        if (argc > 0 && strcmp(args[0], "exit") == 0) {
            should_run = 0;
            continue;
        }

        // Tạo tiến trình con để thực hiện lệnh
        pid_t pid = fork();
        if (pid < 0) { // Lỗi khi tạo tiến trình con
            perror("fork failed");
            continue;
        } else if (pid == 0) { // Đây là tiến trình con
            // Xử lý chuyển hướng đầu ra
            if (redirect_output) {
                int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    perror("open failed");
                    return 1;
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            // Xử lý chuyển hướng đầu vào
            if (redirect_input) {
                int fd = open(input_file, O_RDONLY);
                if (fd < 0) {
                    perror("open failed");
                    return 1;
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }

            // Thực thi lệnh
            if (execvp(args[0], args) < 0) {   // tham so dau tien chua lenh can thuc thi, tham so sau do chua tham so cho lenh do vd ("ls", "-l")
                perror("execvp failed");
            }
            return 0;
        } else { // Đây là tiến trình cha
            waitpid(pid, NULL, 0); // Đợi tiến trình con hoàn thành
        }
    }
    return 0;
}
