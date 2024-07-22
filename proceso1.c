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

int main(int argc, char *argv[]){
    // verificar si hay argumentos
    if (argc >= 2){

        // Crear dos tuberias unidireccionales sin nombre

        int tuberia1[2];
        int tuberia2[2];
        if (pipe(tuberia1) < 0){
            perror("Error al crear la tuberia sin nombre");
            return 1;
        }
        if (pipe(tuberia2) < 0){
            perror("Error al crear la tuberia sin nombre");
            return 1;
        }

        // Escritura del parametro en la tuberia1

        int size = strlen(argv[1]) + 1;
        write(tuberia1[1], argv[1], size);

        // Crear proceso 2

        pid_t pid;
        pid = fork();
        if (pid < 0){
            perror("Fallo fork");
            return 1;
        } else if (pid == 0) {

            // Proceso 2

            // Lee la tubería1

            close(tuberia1[1]);

            char ruta[size];
            read(tuberia1[0], ruta, size );

            close(tuberia1[0]);
            close(tuberia2[0]);

            // comprobar si existe un archivo

            struct stat buffer;
            if (stat(ruta, &buffer) < 0){

                // Escribir en la tuberia2 el mensaje
                // Si no se pudo abrir el semaforo es porque p3 no lo ha
                char msg[] = "No se encuentra el archivo a ejecutar";
                int sizeMsg = strlen(msg) + 1;

                printf("La longitud del msg original es de: %d\n", sizeMsg);

                // Primero enviamos la longitud del mensaje
                write(tuberia2[1], &sizeMsg, sizeof(int));
                write(tuberia2[1], msg , strlen(msg));

                close(tuberia2[1]);
                return -1;


            }else {// si el archivo si existe procede a:

                // abrir semaforos

                const char sp3[] = "/SEMP3";
                const char sp2[] = "/SEMP2";
                sem_t *semp3 = sem_open(sp3, O_RDWR, 0666, 0);
                sem_t *semp2 = sem_open(sp2, O_RDWR, 0666, 0);

                if (semp3 == SEM_FAILED || semp2 == SEM_FAILED) {

                    // Si no se pudo abrir el semaforo es porque p3 no lo ha
                    char msg[] = "Proceso p3 no parece estar en ejecucion";
                    int sizeMsg = strlen(msg) + 1;

                    // Primero enviamos la longitud del mensaje

                    write(tuberia2[1], &sizeMsg, sizeof(int));
                    write(tuberia2[1], msg ,strlen(msg));

                    close(tuberia2[1]);
                    return -1;

                }

                // abrir memoria compartida
                const int SIZE = 4096;
                const char *name = "/memoria";
                int fd;
                char *ptr;
                fd = shm_open(name, O_RDWR, 0666);

                if (fd == -1) {

                    // Si no se pudo abrir la memoria es porque p3 no la ha creado
                    char msg[] = "Proceso p3 no parece estar en ejecucion";
                    int sizeMsg = strlen(msg) + 1;

                    // Primero enviamos la longitud del mensaje

                    write(tuberia2[1], &sizeMsg, sizeof(int));
                    write(tuberia2[1], msg ,strlen(msg));
                    return -1;


                }

                // mapeo memoria compartida

                ptr = mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
                if (ptr == MAP_FAILED) {
                    perror("Error MAP_FAILED");
                    return -1;
                }


                // escribir la ruta en el area de memoria compartida

                sprintf(ptr, "%s", ruta);
                ptr += strlen(ruta);

                // bloqueo p2 y desbloqueo p3

                sem_post(semp3);
                sem_wait(semp2);

                // p3 desbloqueó p2

                char * output = (char *) ptr;
                ptr += strlen(output);
                int sizeOutput = strlen(output) + 1;
                // Output que estaba en la memoria compartida se pone en tuberia2 para el proceso 1

                write(tuberia2[1], &sizeOutput, sizeof(int));
                write(tuberia2[1], output, strlen(output));

                close(tuberia2[1]);

            }
        } else{

            // Proceso 1

            close(tuberia1[0]);
            close(tuberia1[1]);

            // Espera a que el proceso 2 termine

            wait(NULL);

            // lee los datos de la tuberia 2

            close(tuberia2[1]);


            // primero recibimos la longitud del mensaje u output
            int sizeMsg;

            read(tuberia2[0], &sizeMsg, sizeof(int));


            // luego recibimos el mensaje u output
            char msg[sizeMsg];
            read(tuberia2[0], msg, sizeof(msg));
            printf("\n%s\n", msg);

            printf("Proceso 1 terminado\n");

        }
    } else{
        printf("Uso: p1 /ruta/al/ejecutable\n");
        return 1;
    }
    return 0;
}