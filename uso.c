#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <string.h>


#define SYS_avanzatech 335  // Ensure this number matches syscall_64.tbl

int main() {
    int number = 5;  // The number to cube
    char username[256] = "Alice";  // Username
    char dest_buffer[256];  // Buffer for the response
    size_t length = strlen(username);  // Length of the username
    size_t dest_len = sizeof(dest_buffer);  // Length of the destination buffer

    long ret = syscall(SYS_avanzatech, number, username, length, dest_buffer, username, dest_len);

    if (ret < 0) {
        perror("Error calling the system call");
        return 1;
    }

    printf("Kernel response: %s\n", dest_buffer);
    return 0;
}