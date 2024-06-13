#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

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
        char *token = strtok(input, " ");  //phan cach bang cac khoan trang, cung tuc la khi ta nhap ls>out.txt may se khong hieu   
        while (token != NULL) {
            args[argc++] = token;
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
            if (execvp(args[0], args) < 0) {
                perror("execvp failed");
            }
            return 0;
        } else { // Đây là tiến trình cha
            waitpid(pid, NULL, 0); // Đợi tiến trình con hoàn thành
        }
    }
    return 0;
}
