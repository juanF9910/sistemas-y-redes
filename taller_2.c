asmlinkage long avanzatech(int number, char __user *buffer, size_t length, char __user *dest_buffer, char __user *username, size_t dest_len){

    char *temp_buffer; // Buffer temporal para almacenar los datos del usuario
    char *response; // Buffer para almacenar la respuesta
    char username_buf[64]; // Tamaño máximo para el nombre de usuario
    long result;

    // 1. Validar el tamaño del buffer
    if (length > 1024 || dest_len > 256) { // Ejemplo de límites
        return -EINVAL; // Tamaño no válido
    }

    // 2. Asignar memoria para los buffers temporales en el espacio del kernel y verificar si se asignó correctamente   
    temp_buffer = kmalloc(length, GFP_KERNEL); // Asignar memoria para los datos del usuario, GFP_KERNEL es una bandera para indicar 
    //que la memoria es para el kernel
    if (!temp_buffer) {
        return -ENOMEM; // Error si no se puede asignar memoria
    }

    response = kmalloc(256, GFP_KERNEL); // Asignar memoria para la respuesta, se usa 256 como tamaño arbitrario para este ejemplo pero 
    //puede ser cualquier tamaño
    if (!response) {
        kfree(temp_buffer);
        return -ENOMEM; // Error si no se puede asignar memoria
    }

    // 3. Copiar datos del espacio de usuario a espacio del kernel
    if (copy_from_user(temp_buffer, buffer, length)) { //el primer argumento de la función es el buffer temporal en el espacio del kernel, 
    //el segundo argumento es el buffer en el espacio del usuario y el tercer argumento es la longitud de los datos a copiar
        kfree(temp_buffer); // Liberar memoria antes de salir
        kfree(response); // Liberar memoria antes de salir
        return -EFAULT; // Error en la copia
    }

    // 4. Validar el nombre de usuario
    if (copy_from_user(username_buf, username, dest_len)) { //GUARDAMOS EL NOMBRE DE USUARIO EN EL BUFFER username_buf, DESDE EL ESPACIO DE USUARIO 
    //Y DEST_LEN ES EL TAMAÑO DEL BUFFER
        kfree(temp_buffer);
        kfree(response);
        return -EFAULT; // Error en la copia
    }
    username_buf[dest_len] = '\0'; // Asegurarse de que esté terminado en null

    // 5. Lógica para cubicar el número
    long cubed_value = (long)number * number * number;

    // 6. Crear el mensaje de respuesta
    snprintf(response, 256, "Hi %s, the cube of %d is %ld", username_buf, number, cubed_value); //snprintf escribe en el buffer response, la 
        //cadena

    // 7. Copiar la respuesta de vuelta al espacio de usuario
    if (copy_to_user(dest_buffer, response, strlen(response) + 1)) {

        kfree(temp_buffer);
        kfree(response);
        return -EFAULT; // Error en la copia

    // 8. Liberar memoria
    kfree(temp_buffer);
    kfree(response);

    return 0; // Éxito

}
    

