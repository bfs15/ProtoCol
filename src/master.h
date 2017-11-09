#ifndef MASTER_H
#define MASTER_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "Protocol.h"
#include "Socket.h"
#include "SlidingWindow.h"

#define COMMAND_HIST_SIZE 64
#define COMMAND_BUF_SIZE 256

void parse(packet msg){
	
}

void console (char** commands, int* lastCom, packet* msg, char* filename) {
	while(msg->type == invalid){
		*lastCom = mod(*lastCom +1, COMMAND_HIST_SIZE);
		printf(" $\n"); // TODO print current remote dir
		result = scanf("%[^\n]%*c", commands[*lastCom]);
		printf("result = %d\n", result);
		printf("command: %s\n", commands[*lastCom]);
		// filename is not a copy of command, it points to the same memory
		msg->type = command_to_type(commands[*lastCom], &filename);
	}
}

int master(char* device){
	Slider slider;
	slider_init(&slider, device);
	uint64_t rec_bytes;
	char commands[COMMAND_HIST_SIZE][COMMAND_BUF_SIZE];
	char* filename;
	packet response;
	packet msg;
	int result;
	int lastCom = 0;
	//int com_i = 0;
	char fout[COMMAND_BUF_SIZE];
	
	// TODO: first thing when starting, send "cd ." command until you get a response
	// server will send on the data_p the dir after the cd command (current dir on server)
	// curr dir will be printed on the console (function below)
	
	/* TODO change scanf to getch
	 * read char by char and append them to command, if its \n, append "\0"
	 * goal: if up arrow pressed, printf("\33[2K\r"); erases current written line
	 * go back 1 in command history:
	 * com_i = mod(com_i -1, COMMAND_HIST_SIZE)
	 * printf("%s", command[com_i]);
	 * https://stackoverflow.com/questions/10463201/getch-and-arrow-codes */
	console(commands, &lastCom, &msg, filename);
	while(msg.type != end){
		// check if type is invalid
		msg.size = strlen(filename)+1; // +1 null-terminator
		msg.data_p = malloc(msg.size);
		memcpy(msg.data_p, filename, msg.size);
		
		/*DEBUG*/
		sl_send(&slider, &msg);
		result = sl_recv(&slider, &response, TIMEOUT);
		if(result < 1){ // timed out
			console(commands, &lastCom, &msg, filename);
			continue;
		}
		/**
		read_msg(&response);
		slider.rseq = seq_mod(slider.rseq +1);
		/**/
		
		printf("< response:\n");
		print(response);
		
		if(response.type == tam){
			memcpy(fout, msg.data_p, msg.size);
			strcat(fout, ".out");
			FILE* stream = fopen(fout, "wb");
			
			int file_size_B = *(uint64_t*)response.data_p;
			
//			if(has space on current dir){ TODO
				msg = NIL_MSG;
				msg.type = ok;
				sl_send(&slider, &msg);
				rec_bytes = receive_data(&slider, stream, file_size_B);
				if(rec_bytes > 0){
					printf("\nbytes transfered = %lu\n", rec_bytes);
				} else {
					printf("receive data failed: wrong response. Try again\n");
				}
//			} TODO
			fclose(stream);
		} else {
			// TODO: if error, print on screen appropriately for each type of error
			// might not be error, just wrong type of response
			printf("command failed on server\n");
		}
		
		console(commands, &lastCom, &msg, filename);
	}
	
	return 0;
}

#endif