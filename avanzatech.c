

SYSCALL_DEFINE6(my_avanzatech, int,  number, char __user *, buffer, size_t,  length, char __user *, dest_buffer, char __user *, username, size_t dest_len){

    char *temp_buffer; // Buffer to store user data temporarily
    char *response; // Buffer to store the response
    char username_buf[64]; // Maximum size for username (arbitrary limit)
    long cubed_value; // To store cubed value
    long result; // To store result of operations

    // 1. Validate buffer sizes
    if (length > 1024 || dest_len > sizeof(username_buf) - 1) { // Ensure safe size
        return -EINVAL; // Invalid size
    }

    // 2. Allocate memory for temporary buffers in kernel space
    temp_buffer = kmalloc(length, GFP_KERNEL);
    if (!temp_buffer) {
        return -ENOMEM; // Error if memory allocation fails
    }

    response = kmalloc(256, GFP_KERNEL);
    if (!response) {
        kfree(temp_buffer);
        return -ENOMEM; // Error if memory allocation fails
    }

    // 3. Copy data from user space to kernel space
    if (copy_from_user(temp_buffer, buffer, length)) {
        kfree(temp_buffer);
        kfree(response);
        return -EFAULT; // Copy error
    }

    // 4. Copy username from user space to kernel space
    if (copy_from_user(username_buf, username, dest_len)) {
        kfree(temp_buffer);
        kfree(response);
        return -EFAULT; // Copy error
    }

    // Ensure null termination of the username
    username_buf[dest_len < sizeof(username_buf) ? dest_len : sizeof(username_buf) - 1] = '\0';

    // 5. Logic to cube the number
    cubed_value = (long)number * number * number;

    // 6. Create the response message
    scnprintf(response, 256, "Hi %s, the cube of %d is %ld", username_buf, number, cubed_value);

    // 7. Copy the response back to user space
    result = copy_to_user(dest_buffer, response, strlen(response) + 1);
    if (result) {
        kfree(temp_buffer);
        kfree(response);
        return -EFAULT; // Copy error
    }

    // 8. Free memory
    kfree(temp_buffer);
    kfree(response);

    return 0; // Success

}