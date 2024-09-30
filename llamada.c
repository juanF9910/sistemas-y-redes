#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

#define SYS_avanzatech 548 // Adjust this according to your syscall number

int main() {
    int number = 3;
    char buffer[100];
    char response[100];
    char username[] = "User";
    size_t length = sizeof(buffer);
    size_t dest_len = sizeof(response);

    long result = syscall(SYS_avanzatech, number, buffer, length, response, username, dest_len);

    if (result < 0) {
        perror("syscall");
    } else {
        printf("%s\n", response);
    }
    return 0;
}