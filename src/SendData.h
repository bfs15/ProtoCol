#ifndef SENDDATA_H
#define SENDDATA_H

#include <math.h>

#include "Window.h"

void print_window(Slider* this){
	printf("\n");
	printf("acc=%x; \tstart=%x; \n", this->window.acc, this->window.start);
	int it;
	
	it = this->window.start;
	do{
		printf(" \t%x", it);
		
		it = w_mod(it+1);
	} while(it != w_mod(w_end(&this->window) +1));
	printf("\n");
	
	it = this->window.start;
	do{
		printf(" \t%x", this->window.arr[it].seq);

		it = w_mod(it+1);
	} while(it != w_mod(w_end(&this->window) +1));
	printf("\n");
	
	it = this->window.start;
	do{
		if(this->window.arr[it].error){
			printf(" \tt"); // to send
		} else {
			printf(" \ts"); // sent
		}
		
		it = w_mod(it+1);
	} while(it != w_mod(w_end(&this->window) +1));
	printf("\n");
	
	it = this->window.start;
	do{
		printf(" \t%x", this->window.arr[it].type);
		
		it = w_mod(it+1);
	} while(it != w_mod(w_end(&this->window) +1));
	printf("\n");
	
	it = this->window.start;
	do{
		if(it == this->window.acc){
			printf(" \ta");
		} else {
			printf(" \t ");
		}
		
		it = w_mod(it+1);
	} while(it != w_mod(w_end(&this->window) +1));
	printf("\n");
}

packet next_packet(Slider* this, FILE* stream){
	packet msg;
	msg.error = true;
	set_seq(this, &msg);
	
	msg.data_p = (uint8_t*)malloc(data_max);
	msg.size = fread(msg.data_p, 1, data_max, stream);
	
	if(msg.size > 0){
		msg.type = data;
	} else {
		if(DEBUG_W)printf("msg.size = fread returned %x bytes, assumed eof\n", msg.size);
		msg.type = end;
		msg.data_p = NULL;
	}
	return msg;
}

/**
 * @brief fill window from to w_end, from always gets filled
 */
long fill_window(Slider* this, int from, FILE* stream, bool* sent, bool* eof){
	long sentBytes = 0;
	// 3 4 5 6	receives nack 5, given: start = 0; [0].seq == 3
	// 7 8 5 6	start = 2; from 0 to <start, new packets into window
	int i = from;
	do {
		// clear sent msgs
		if(this->window.arr[i].data_p != NULL){
			sentBytes += this->window.arr[i].size;
		}
		free(this->window.arr[i].data_p);
		this->window.arr[i].data_p = NULL;
		if(this->window.arr[i].type == end){
			*sent = true;
			break;
		}
		// queue next
		if( ! *eof){
			this->window.arr[i] = next_packet(this, stream);
			if(this->window.arr[i].type == end){
				*eof = true;
			}
		} else { // no message to fill in
			this->window.arr[i].type = invalid;
			this->window.arr[i].seq = i_to_seq(&this->window, i);
		}
		
		i = w_mod(i+1);
	} while (i != w_mod(w_end(&this->window) +1));
	
	return sentBytes;
}
/**
 * @brief sets window messages as sent
 */
void set_sent(Window* this){
	if(DEBUG_W)printf("Setting sent >\n");
	int it = this->start;
	do {
		if(this->arr[it].type == invalid){
			break;
		}
		this->arr[it].error = false;
		
		it = w_mod(it+1);
	} while(it != w_mod(w_end(this) +1));
}
/**
 * @brief sends unsent messages
 */
void send_window(Slider* this){
	if(DEBUG_W)printf("Sending window >\n");
	int it = this->window.start;
	do {
		if(this->window.arr[it].type == invalid){
			break;
		}
		if(this->window.arr[it].error){
			if(DEBUG_W)print(this->window.arr[it]);
			send_msg(this->sock, this->window.arr[it], this->buf);
		}
		
		it = w_mod(it+1);
	} while(it != w_mod(w_end(&this->window) +1));
	if(DEBUG_W)printf("> sent\n\n");
}
/**
 * @brief handles acks and nacks, setting the start of the window and nacked messages as unsent
 * @return true if window needs to be filled with new messages
 */
int handle_response(Window* this, packet response, bool* ended){
	this->acc = this->start;
	int response_seq = response.data_p[0];
	
	switch(response.type){
		case nack:
			at_seq(this, response_seq)->error = true;
			// 3 4 5 6	receives nack 5, given: start = 0; [0].seq == 3
			// 7 8 5 6	start = 2; from 0 to <start, new packets into window
			this->start = seq_to_i(this, response_seq);
			// 3 4 5 6	receives nack 3; start = 0; no new packets
			if(this->acc == this->start){
				return false;
			}
			break;
			
		case ack:
			// 3 4 5 6	receives ack 5, given: start = 0; [0].seq == 3
			// 7 8 9 6	start = 3; from 0 to <start, new packets into window
			// 7 8 9 A	receives ack 6;	start = 0; from 0 to <0
			this->start = w_mod(seq_to_i(this, response_seq) +1);
			break;
		case error:
			errno = (*(int*)response.data_p);
		default:
			fprintf(stderr, "WARN: received a response that isn't ack nor nack in data transfer\n");
			*ended = true;
			return false;
	}
	
	return true;
}
/**
 * @brief Sends data from file stream until EOF
 */
long send_data(Slider* this, FILE* stream){
	packet response;
	long sentData = 0;
	w_init(&this->window, this->rseq);
	
	this->window.acc = this->window.start;
	
	bool fill;
	bool sent = false;
	bool eof = false;
	// startup
	sentData += fill_window(this, this->window.acc, stream, &sent, &eof);
	
	while(!sent){
		if(DEBUG_W)print_window(this);
		send_window(this);
		/**/
		// with timeout
		int buf_n = sl_recv(this, &response, TIMEOUT);
		if(buf_n < 1 || !isResponse(response)){
			continue; // timeout or invalid reply, send window again
		}
		/*DEBUG*
		printf("Receive reply? ");
		int reply;
		if(scanf("%x", &reply) < 0) fprintf(stderr, "scan error\n");
		if(reply < 1){
			continue;
		}
		read_msg(&response);
		/**/
		set_sent(&this->window);
		
		if(DEBUG_W)printf("handling response\n");
		if(DEBUG_W)print(response);
		
		fill = handle_response(&this->window, response, &sent);
		
		if(fill){
			sentData += fill_window(this, this->window.acc, stream, &sent, &eof);
			printf("Current sent bytes: %ld\n", sentData);
		}
	}
	
	if(DEBUG_W && (this->sseq != seq_mod(w_back(&this->window).seq +1))){
		fprintf(stderr, "WARN: End of send_data, seqs should be equal %x != %x\n", this->sseq, seq_mod(w_back(&this->window).seq +1));
	}
	
	return sentData;
}

#endif