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
long mytimeFunc();
int casosHijo(int pid, int descfile, int in_background, int status);
int casosPipe(int pid, int fd[2], int descfile, int desc_dup, int n_commands, int i, int in_background, char ***argvv);

void siginthandler(int param){
	printf("****  Saliendo del MSH **** \n");
	//signal(SIGINT, siginthandler);
	exit(0);
}

// Timer 
pthread_t timer_thread;
unsigned long  mytime = 0;

void* timer_run ( ){
	while (1){
		usleep(1000);
		mytime++;
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
				mytimeFunc();

			// Para ejecutar un solo comando a través del hijo. No necesariamente utilizar pipes.
			if (command_counter == 1) { 
				descfile = 0;
            	pid = fork();
				
				/* Una vez creado el proceso hijo, se revisan los diferentes casos de creación
				 * con la función casosHijo. */
				if((ret = casosHijo(pid, descfile, in_background, status)) != 0)
					return (-1);	
			} 

			// En caso de que se tenga más de un comando. Se crean pipes. 
			// Descriptores de input y output de los pipes.
            int fd[2];	
			int pid, desc_dup;	

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

				//[RENOMBRAR] Se regresa el valor de descfile a 0.
				descfile = 0;	
				// Procedemos a crear un proceso para cada comand y se regresa el valor de ret a 0.
				ret = 0;
				pid = fork();

				/* Una vez creado el proceso hijo, se revisan los diferentes casos de creación
				 * con la función casosPipe. */
				if((ret = casosPipe(pid, fd, descfile, desc_dup, n_commands, i, in_background, argvv)) != 0)
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
 * @param descfile
 * @param in_background
 * @param status
 * @return int 
 */
int casosHijo(int pid, int descfile, int in_background, int status){
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
						
				// Se abre o crea el nuevo fichero.
				if ((descfile = open(filev[2], O_TRUNC | O_WRONLY | O_CREAT, 0644)) < 0) {
					perror("Error: Problema al abrir el nuevo fichero de error.\n");
					return (-1);
				}
			}

			/* Para que el hijo ejecute el comando. Solamente toma lo de [0] porque 
			 * en este caso se considera que se tiene un solo comando. */
			if (execvp(argv_execvp[0], argv_execvp) < 0) {
				perror("Error: Problema al ejecutar el comando en el proceso hijo.\n");
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
 * @param fd[2]
 * @param descfile
 * @param desc_dup
 * @param n_commands
 * @param i
 * @param in_background
 * @param argvv
 * @return int
 */
int casosPipe(int pid, int fd[2], int descfile, int desc_dup, int n_commands, int i, int in_background, char ***argvv){
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
			// Se verifica si el fichero de redirección de errores existe.
            if (strcmp(filev[2], "0") != 0) {
				if((close(2)) < 0) { // Para cerrar el fichero de redirección de errores.
					perror("Error: Problema al cerrar el fichero descriptor de errores.\n");
					return (-1);
				}

				// Se abre o crea el nuevo fichero.
				if ((descfile = open(filev[2], O_TRUNC | O_WRONLY | O_CREAT, 0644)) < 0) {
					perror("Error: Problema al abrir el nuevo fichero de error.\n");
					return (-1);
				}
            }

			/* En el caso de que se tenga redirección a un nuevo fichero de input y sea la 
			 * primera iteración, se procede a cerrar el default y se abre el otro fichero.*/
            if (i == 0 && strcmp(filev[0], "0") != 0) {
				if((close(0)) <0){
					perror("Error: Problema al cerrar el descriptor default de input.\n");
					return (-1);
				}
					
				if ((descfile = open(filev[0], O_RDWR, 0644)) < 0) {
					perror("Error: Problema al abrir el nuevo fichero de input.\n");
					return (-1);
				}
            }
			
			// Si el caso anterior no aplica, el nuevo fichero de input se considera un duplicado del anterior.
			if((close(0)) < 0) {
				perror("Error: Problema al cerrar el descriptor default de input.\n");
				return (-1);
			}
		
			// Se procede a duplicar el primer descriptor.
			if (dup(desc_dup) < 0) {
				perror("Error: Problema al duplicar el nuevo input.\n");
				return (-1);
			}
			
			// Se cierra el primer descriptor.
			if((close(desc_dup)) < 0){
				perror("Error: Problema al cerrar el antiguo input.\n");
				return (-1);
			}

			// Para verificar si no es el último proceso.
            if (i != n_commands - 1) {
				// Para cerrar el output default.
				if((close(1)) < 0) { 
					perror("Error: Problema al cerrar el fichero de output.\n");
					return (-1);
				}
		
				// Se duplica el output del pipe para que el siguiente lo pueda utilizar. 
				if (dup(fd[1]) < 0) {
					perror("Error: Problema al duplicar el output del pipe.\n");
					return (-1);
				}
				
				// Para cerrar el input del pipe. 
				if ((close(fd[0])) < 0) {
					perror("Error: Problema al cerrar el input del pipe.\n");
					return (-1);
				}
					
				// Para cerrar el output del pipe.
				if ((close(fd[1])) < 0) {
					perror("Error: Problema al cerrar el output del pipe.\n");
					return (-1);
				}
            }

			/* En caso de que sea el último proceso, se revisa el output del fichero de 
			 * redirección. Después se cierra el antiguo fichero. */
			if (strcmp(filev[1], "0") != 0) {
				if((close(1)) <0) {
					perror("Error: Problema al cerrar el fichero de output.\n");
					return (-1);
				}

				// Se abre o crea el nuevo fichero de output.
				if ((descfile = open(filev[1], O_TRUNC | O_WRONLY | O_CREAT, 0644)) < 0) {
					perror("Error: Problema al abrir el nuevo fichero de output.\n");
					return (-1);
				}
            }

			// Revisar si se está ejecutando en el background.
			getCompleteCommand(argvv, i);
			
			// Para imprimir el pid del proceso hijo. 
			if (in_background == 1) {
				pid = getpid();
	            printf("[%d]\n", pid);
	        }

			// Se ejecuta el comando.
			if (execvp(argv_execvp[0], argv_execvp) < 0) {
				perror("[Error: Problema al ejecutar el comando.\n");
				return (-1);
			}
						
			break;

		// Caso para el proceso padre. 
		default:
			// Para cerrar el descriptor de input.
			if ((close(desc_dup)) < 0) {
				perror("Error: Problema al cerrar el descriptor input.\n");
				return (-1);
			}

			
			break;
	}// Fin del switch.

	return 0;
}// Fin de casosPipe.

// [FALTA]
int mycalc(){
	//
}

// [FALTA]
long mytimeFunc(){
	//
}