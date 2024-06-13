#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_LINE 80     // Độ dài tối đa của lệnh
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
    history_index = history_count; // Reset history index
}

void print_prompt(void) {
    printf("it007sh> ");
    fflush(stdout);
}

void execute_command(const char *command) {
    const char *cmd = "/bin/sh";
    const char *c_flag = "-c";
    pid_t pid = fork();

    if (pid == 0) {
        // Child process
        const char *argv[] = {cmd, c_flag, command, NULL};
        execvp(cmd, (char *const *)argv);
        _exit(EXIT_FAILURE); // exec never returns unless there is an error
    } else if (pid < 0) {
        // Error forking
        perror("fork");
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
    }
}

void sigint_handler(int sig) {
    (void)sig; // Supress unused parameter warning
    disableRawMode();
    printf("\nTerminated by Ctrl+C\n");
    exit(0);
}

void handle_key_arrow_up(char input[], int *input_index) {
    if (history_index > 0) {
        history_index--;
        strcpy(input, history[history_index]);
        *input_index = strlen(input);
        printf("\33[2K\r"); // Clear the line
        print_prompt();
        printf("%s", input);
    }
}

void handle_key_arrow_down(char input[], int *input_index) {
    if (history_index < history_count - 1) {
        history_index++;
        strcpy(input, history[history_index]);
        *input_index = strlen(input);
        printf("\33[2K\r"); // Clear the line
        print_prompt();
        printf("%s", input);
    } else if (history_index == history_count - 1) {
        history_index++;
        input[0] = '\0';
        *input_index = 0;
        printf("\33[2K\r"); // Clear the line
        print_prompt();
    }
}

int main(void) {
    signal(SIGINT, sigint_handler); // Set up the signal handler for SIGINT
    enableRawMode();
    char input[MAX_LINE];
    int input_index = 0;
    char c;

    while (1) {
        print_prompt();
        input_index = 0;
        memset(input, 0, sizeof(input)); // Initialize input buffer

        while (1) {
            ssize_t nread = read(STDIN_FILENO, &c, 1); // Read a character
            if (nread == -1 && errno != EAGAIN) { // Handle read error
                perror("read");
                exit(EXIT_FAILURE);
            } else if (nread == 0 || c == '\n') { // Handle enter key
                if (input_index > 0) { // If there's something to execute
                    input[input_index] = '\0'; // Null-terminate the input string
                    add_to_history(input);
                    execute_command(input);
                }
                break; // Break after executing
            } else if (c == 127) { // Handle backspace
                if (input_index > 0) {
                    input_index--;
                    printf("\b \b"); // Erase the character
                }
            } else if (c == '\x1b') { // Escape sequence (arrow keys)
                char seq[3];
                if (read(STDIN_FILENO, &seq[0], 1) != 1) break;
                if (read(STDIN_FILENO, &seq[1], 1) != 1) break;

                if (seq[0] == '[') {
                    switch (seq[1]) {
                        case 'A': // Up arrow
                            handle_key_arrow_up(input, &input_index);
                            break;
                        case 'B': // Down arrow
                            handle_key_arrow_down(input, &input_index);
                            break;
                    }
                }
            } else {
                input[input_index++] = c; // Store the character in the input buffer
                printf("%c", c); // Echo the character back to the terminal
            }
        }
    }
}
