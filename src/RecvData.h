#ifndef RECVDATA_H
#define RECVDATA_H

#include "Window.h"

/**
 * @brief if proper, adds message to window and increments window.acc
 * @param msg 
 * @return true if a message was added to the window
 */
int handle_msg(Slider* this, packet msg){
	if(seq_after(msg.seq, w_back(&this->window).seq)){
		fprintf(stderr, "received message ahead of the window\n");
		msg.error = true;
	}
	if( !msg.error ){
		int msg_pos = seq_to_i(&this->window, msg.seq);
		// if hadn't received this msg yet
		if(this->window.arr[msg_pos].error){
			this->window.arr[msg_pos] = msg;
			// update which is the last message received without errors in the sequence
			while( (indexes_remain(&this->window) > 0) && !(this->window.arr[w_mod(this->window.acc + 1)].error) ){
			// [0 _ 2 3 4], acc == 0, receive 1
			// [0 1 2 3 4], acc == 4 == w_end
				this->window.acc = w_mod(this->window.acc +1);
			}
			
			return true;
		}
	}
	
	free(msg.data_p);
	msg.data_p = NULL;
	
	return false;
}

void respond(Slider* this){
	// DEBUG
	printf("response ");
	int one = i_to_seq(&this->window, w_end(&this->window));
	int two = w_back(&this->window).seq;
	int last_seq = last_acc(&this->window).seq;
	if(w_back(&this->window).seq == last_acc(&this->window).seq){
		printf("ack %x\n", last_acc(&this->window).seq);
		send_ack(this, last_acc(&this->window).seq);
	} else {
		printf("nack %x\n", first_err(&this->window).seq);
		send_nack(this, first_err(&this->window).seq);
	}
	// 3 4 5 6	front is [0]=3, acc is [2]=5
	// 7 8 9 6	[0]=5+1 [1]=5+2 [2]=5+3
	// move window only if the first has been ack
	if(!last_acc(&this->window).error){
		int i = 1;
		int it = w_mod(this->window.acc +1);
		do{
			this->window.arr[it].seq = seq_mod(last_acc(&this->window).seq + i);
			// last it of loop will be w_end
			this->window.arr[it].type = invalid;
			this->window.arr[it].error = true;
			
			free(this->window.arr[it].data_p);
			this->window.arr[it].data_p = NULL;
			
			i += 1;
			it = w_mod(it+1);
		} while (it != w_mod(this->window.acc +1));
		this->window.start = w_mod(this->window.acc +1);
	}
	// DEBUG
	print_slider(this);
}

int receive_data(Slider* this, FILE* stream, int data_size){
	packet msg;
	bool ended = false;
	int rec_size = 0;
	w_init(&this->window, this->rseq);
	
	int buf_size;

	while(!ended){
		// DEBUG
		buf_size = 1;
		// buf_size = rec_packet(this->sock, &msg, this->buff, 0);
		while(buf_size > 0){
		//while( indexes_remain(&this->window) > 0 ){
			// block until at least 1 msg
			// DEBUG
			// rec_packet(this->sock, &msg, this->buf, 0);
			printf("Receiving Packet ");
			printf("error = ");
			int result;
			if(scanf("%d", &result) < 0) fprintf(stderr, "scan error\n");
			msg.error = result;
			printf("enter seq = ");
			if(scanf("%x", &result) < 0) fprintf(stderr, "scan error\n");
			msg.seq = result;
			printf("enter size = ");
			if(scanf("%x", &result) < 0) fprintf(stderr, "scan error\n");
			msg.size = result;
			printf("enter type = ");
			if(scanf("%x", &result) < 0) fprintf(stderr, "scan error\n");
			msg.type = result;
			msg.data_p = (uint8_t*)malloc(msg.size);
			for(int i = 0; i < msg.size; i++){
				msg.data_p[i] = i;
			}
			print(msg);
			
			handle_msg(this, msg);
			print_slider(this);
			
			// DEBUG
			printf("next msg buf_size = ");
			if(scanf("%x", &result) < 0) fprintf(stderr, "scan error\n");
			buf_size = result;
//			buf_size = 0;
//			if(indexes_remain(this->window) > 0){
//				buf_size = try_packet(this->sock, &msg, this->buf);
//			}
		}
		
		int msgs_to_write = seq_mod( last_acc(&this->window).seq +1 - w_front(&this->window).seq );
		// has at least one msg to write
		if( msgs_to_write > 0 ){
			printf("started writing, seqs: ");
			int it = this->window.start;
			do {
				if(this->window.arr[it].type == end){
					ended = true;
					break;
				}
				printf("%hx ", this->window.arr[it].seq % 0xf);
				fwrite(this->window.arr[it].data_p, 1, this->window.arr[it].size, stream);
				rec_size += this->window.arr[it].size;
				
				it = w_mod(it + 1);
			} while(it != w_mod(this->window.acc +1));
			printf("\n");
		}
		
		respond(this);
	}
	if(rec_size < data_size){
		fprintf(stderr, "File transfer error, received %x/%x bytes", rec_size, data_size);
	}
	
	this->rseq = seq_mod(last_acc(&this->window).seq +1);
	return rec_size;
}

#endif