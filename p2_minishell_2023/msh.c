/* P2-SSOO-22/23
 * David Roldán Nogal				100451289
 * Ilse Mariana Córdova Sánchez		100501460
 * 
 * Código para un minishell que utiliza la entrada estándar para leer las líneas
 * de mandato a ejecutar. Este intérprete de mandatos utiliza la salida estándar 
 * para presentar el resultado de los comandos por pantalla. También utiliza la
 * salida estandar de error para notificar sobre los mismos.
 * /

// Bibliotecas utilizadas en el código.
#include <stddef.h>         /* NULL */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <math.h>

// Máximo número de comandos.
#define MAX_COMMANDS 8

// Ficheros por si hay redirección.
char filev[3][64];

// Para guardar el segundo parámetro de execvp.
char *argv_execvp[8];

// Declaraciones de funciones que se implementaron.
int mycalc();
long mytime();
int casosHijo(int pid, int descfile, int in_background);
int casosPipe(int pid);

void siginthandler(int param){
	printf("****  Saliendo del MSH **** \n");
	//signal(SIGINT, siginthandler);
	exit(0);
}

// Timer 
pthread_t timer_thread;
unsigned long  mytimeVar = 0;

void* timer_run ( ){
	while (1){
		usleep(1000);
		mytimeVar++;
	}
}

/**
 * Get the command with its parameters for execvp
 * Execute this instruction before run an execvp to obtain the complete command
 * @param argvv
 * @param num_command
 * @return
 */
void getCompleteCommand(char*** argvv, int num_command) {
	// Reset first.
	for(int j = 0; j < 8; j++)
		argv_execvp[j] = NULL;

	int i = 0;
	for ( i = 0; argvv[num_command][i] != NULL; i++)
		argv_execvp[i] = argvv[num_command][i];
}

/*
 * Main shell  Loop  
 */
int main(int argc, char* argv[]){
	/**** Do not delete this code.****/
	int end = 0; 
	int executed_cmd_lines = -1;
	char *cmd_line = NULL;
	char *cmd_lines[10];
	// Variables utilizadas en main.
	int descfile;						// [RENOMBRAR] Descriptor de ficheros para abrirlos. 
	int pid;							// Para identificación de procesos.
	int ret = 0;						// Para guardar el valor que devuelve la función casosHijo();
	int command_counter = 0;
	int n_commands = command_counter;	// [RENOMBRAR] Utilizada en el caso de que se tenga más de un comando para guardar la cantidad.
	int desc_dup;						// ????

	if (!isatty(STDIN_FILENO)) {
		cmd_line = (char*)malloc(100);
		while (scanf(" %[^\n]", cmd_line) != EOF){
			if(strlen(cmd_line) <= 0) return 0;
			cmd_lines[end] = (char*)malloc(strlen(cmd_line)+1);
			strcpy(cmd_lines[end], cmd_line);
			end++;
			fflush (stdin);
			fflush(stdout);
		}
	}

	pthread_create(&timer_thread,NULL,timer_run, NULL);

	/*********************************/

	char ***argvv = NULL;
	int num_commands;

	while (1) {
		int status = 0;
		int command_counter = 0;
		int in_background = 0;
		signal(SIGINT, siginthandler);

		// Prompt 
		write(STDERR_FILENO, "MSH>>", strlen("MSH>>"));

		// Get command
		//********** DO NOT MODIFY THIS PART. IT DISTINGUISH BETWEEN NORMAL/CORRECTION MODE***************
		executed_cmd_lines++;
		if( end != 0 && executed_cmd_lines < end) {
			command_counter = read_command_correction(&argvv, filev, &in_background, cmd_lines[executed_cmd_lines]);
		}
		else if( end != 0 && executed_cmd_lines == end){
			return 0;
		}
		else{
			command_counter = read_command(&argvv, filev, &in_background); //NORMAL MODE
		}
		//************************************************************************************************

		/************************ STUDENTS CODE ********************************/
		if (command_counter > 0) {
			if (command_counter > MAX_COMMANDS){
				printf("Error: Numero máximo de comandos es %d \n", MAX_COMMANDS);
			} else {
				// Print command.
				print_command(argvv, filev, in_background);
			}

			for(int i = 0; i < command_counter; i++)
				getCompleteCommand(argvv, i);
			
			// Para el comando interno mycalc.
			if(strcmp(argv_execvp[0], "mycalc") == 0)
				mycalc();
			
			// Para el comando interno mytime.
			if(strcmp(argv_execvp[0], "mytime") == 0)
				mytime();

			// Para ejecutar un solo comando a través del hijo. No necesariamente utilizar pipes.
			if (command_counter == 1) { 
				descfile = 0;
            	pid = fork();
				
				/* Una vez creado el proceso hijo, se revisan los diferentes casos de creación
				 * con la función casosHijo. */
				if((ret = casosHijo(pid, descfile, in_background)) != 0)
					return (-1);	
			} 

			// En caso de que se tenga más de un comando. Se crean pipes. 
			// Descriptores de input y output de los pipes.
            int fd[2];		
            descfile = 0;

			// Para hacer que el input estandar sea desc_dup [RENOMBRAR] con detección de error. 
			if ((desc_dup = dup(0)) < 0) {
				perror("Error: Descriptor no pudo ser duplicado.\n");
				return (-1);
			}

			// Se crea un pipe para la ejecución de cada comando.
            for (int i = 0; i < n_commands; i++) {
				// Revisamos si es el último comando.  En ese caso no se crea un pipe.
            	if (i != n_commands - 1) {
					// Se crea el pipe con detección de error.
                    if (pipe(fd) < 0) {
                        perror("Error: Problema al crear el pipe.\n");
                        return (-1);
                    }	
                }

				// Procedemos a crear un proceso para cada comand y se regresa el valor de ret a 0.
				ret = 0;
				pid = fork();

				/* Una vez creado el proceso hijo, se revisan los diferentes casos de creación
				 * con la función casosPipe. */
				if((ret = casosPipe(pid)) != 0)
					return (-1);	

			} // Fin del for.
		} // Fin if command counter.
	} // Fin while(1)
	
	return 0;
} // Fin del main.

/**
 * Función que verifica los diferentes casos que pueden presentarse al tener un
 * solo comando y hacer un fork para tener un proceso hijo.
 * @param pid
 * @return
 */
int casosHijo(int pid, int descfile, int in_background){
	switch(pid) {
		// Caso error en la creación.
		case -1:
			perror("Error: Proceso de creación fallido.\n");
    		return (-1);
			break;
					
		// Caso de fork exitoso. Se recive 0 del hijo.
		case 0:
			// Para el redireccionamiento del input estandar. Verificamos que el fichero exista.
			if (strcmp(filev[0], "0") != 0) {
				// Se cierra el fichero realizando detección de errores.
				if((close(0)) < 0) {
					perror("Error: Cerrado de fichero STD_IN fallido.\n");
					return (-1);
				}
				
				// Se abre el nuevo fichero.
				if ((descfile = open(filev[0], O_RDWR, 0644)) < 0) {
					perror("Error: Problema al abrir el nuevo fichero input.\n");
					return (-1);
				}
			}
					
			// Para el redireccionamiento del output estandar. Verificamos que el fichero exista.
			if (strcmp(filev[1], "0") != 0) {
				// Se cierra el fichero realizando detección de errores.
				if((close(1)) <0){
					perror("Error: Cerrado de fichero STD_OUT fallido.\n");
					return (-1);
				}

			// Se abre el nuevo fichero.
				if ((descfile = open(filev[1], O_TRUNC | O_WRONLY | O_CREAT, 0644)) < 0) {
					perror("Error: Problema al abrir el nuevo fichero output.\n");
					return (-1);
				}
			}

			// Para el redireccionamiento errort estandar. Verificamos que el fichero exista.
			if (strcmp(filev[2], "0") != 0) {
				// Se cierra el fichero realizando detección de errores.
				if((close(2)) <0){
					perror("Error: Cerrado de fichero STD_ERR fallido.\n");
					return (-1);
				}
						
				// Se abre el nuevo fichero.
				if ((descfile = open(filev[2], O_TRUNC | O_WRONLY | O_CREAT, 0644)) < 0) {
					perror("Error: Problema al abrir el nuevo fichero de error.\n");
					return (-1);
				}
			}

			/* Para que el hijo ejecute el comando. Solamente toma lo de [0] porque 
			 * en este caso se considera que se tiene un solo comando. */
			if (execvp(argv_execvp[0], argv_execvp) < 0) {
				perror("[ERROR] There was an error executing command in the child\n");
				return (-1);
			}
			break;

		// Caso default en el que se tiene el proceso padre, entonces descfile debe ser igual a 0. 
		default:
			// Si descfile es diferente de 0.
			if (descfile) {
				if ((close(descfile)) < 0){
					perror("Error: Problema al cerrar el nuevo fichero descriptor.\n");
					return (-1);
				}
			}

			if (in_background == 0){
				while (wait(&status) > 0);
					// Para verificar que el proceso hijo se ejecuta sin problema.
					if (status < 0) {
                        perror("Error: Problema en la ejecución del proceso hijo.\n");
						return (-1);
                	}
				
					// Para imprimir el pid del proceso.
					pid = getpid();
	                printf("PID: %d\n", pid);
					break;
			}
	} 
	
	return 0;
} 

/**
 * Función que verifica los diferentes casos que pueden presentarse al tener
 * pipes, varios comandos y hacer un fork para tener un proceso hijo para 
 * cada comando recibido.
 * @param pid
 * @return
 */
int casosPipe(int pid){
	switch(pid) {
		// Caso error en la creación.
		case -1:
			perror("Error: Proceso de creación fallido.\n");
		
			// Se procede a cerrar los descriptores de read y write creados por el pipe.
			if ((close(fd[0])) < 0) {
				perror("Error: Problema al cerrar el descriptor input.\n");
				return (-1);				
			}
			
			if ((close(fd[1])) < 0) {
				perror("Error: Problema al cerrar el descriptor output.\n");
				return (-1);
			}

			break;

		// Caso de fork exitoso. Se recive 0 del hijo.
		case 0:


	}
}//