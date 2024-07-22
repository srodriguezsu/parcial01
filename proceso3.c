#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <semaphore.h>

char* slice(char original[]){
    int n = strlen(original);
    static int c = 0;
    static char resultado[50] = "";
    int i;
    for (i = 0; i<n; i++){

        if (original[n-i] == *"/"){
            c=n-i;
            break;
        }
    }
    c++;
    for (c; c<n; c++){
        char str[2];
        str[0] = original[c];
        strcat(resultado, str);
    }



    return resultado;
}

int main(){
    // crear area compartida
    const int size = 4096;
    const char *name = "/memoria";

    int fd;
    char *ptr;

    shm_unlink(name);

    fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (fd == -1){
        perror("Error al crear el area de memoria compartida");
        return -1;
    }
    ftruncate(fd, size);
    ptr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("Error en el MAP");
    }

    /* nombre de los semáforos */
    const char sp3[] = "/SEMP3";
    const char sp2[] = "/SEMP2";

    /* eliminar los semáforos antes de crearlos para evitar
    * valores previos de los mismos en memoria.
    */

    if (sem_unlink(sp3) < 0 || sem_unlink(sp2) < 0 ) {
        perror("Error en sem_unlink()\n");
    }

    /* crear los semáforos: uno para el padre y otro para el hijo */
    sem_t *semp3 = sem_open(sp3, O_CREAT, 0666, 0);
    sem_t *semp2 = sem_open(sp2, O_CREAT, 0666, 0);

    /* chequeo de error en la creación de los semáforos */
    if (semp3 == SEM_FAILED || semp2 == SEM_FAILED) {
        perror("Error en sem_open()\n");
        return 1;
    }

    sem_wait(semp3);

    char* ruta = (char *) ptr;
    ptr += strlen(ruta);





    // Crear tuberia sin nombre
    int fildes[2];
    if (pipe(fildes) < 0){
        perror("Error al crear la tuberia sin nombre");
        return -1;
    }

    pid_t pid;
    pid = fork();
    if (pid < 0){
        perror("Fallo fork");
        return -1;
    }
    else if (pid == 0){
        // uname
        close(fildes[0]);
        dup2(fildes[1], STDOUT_FILENO);
        int cmd = execlp(ruta, slice(ruta), NULL);
        if (cmd){
            perror("No se encuentra el archivo a ejecutar\n");
            return 1;
        }
    }   else {
        // proceso 3
        wait(NULL);
        char output[1024];
        read(fildes[0], output, sizeof(output));
        close(fildes[1]);
        sprintf(ptr,"%s", output);
        sem_post(semp2);
    }
    sem_unlink(sp2);
    sem_unlink(sp3);
    shm_unlink(name);
    return 0;
}