// Trivial Torrent

// TODO: some includes here

#include "file_io.h"
#include "logger.h"

#include "assert.h"
#include <stdio.h>
#include <stdint.h>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <endian.h>
#include <errno.h>
// TODO: hey!? what is this?

/**
 * This is the magic number (already stored in network byte order).
 * See https://en.wikipedia.org/wiki/Magic_number_(programming)#In_protocols
 */
static const uint32_t MAGIC_NUMBER = 0xde1c3233; // = htonl(0x33321cde);

static const uint8_t MSG_REQUEST = 0;
static const uint8_t MSG_RESPONSE_OK = 1;
static const uint8_t MSG_RESPONSE_NA = 2;

enum { RAW_MESSAGE_SIZE = 13 };

struct mssg_t {
        uint64_t b_number;
        uint32_t m_number;
        int mssg_code;
    };


/**
 * Main function.
 */
int main(int argc, char **argv) {

    set_log_level(LOG_DEBUG);
    log_printf(LOG_INFO, "Trivial Torrent (build %s %s) by %s", __DATE__, __TIME__, "J. DOE and J. DOE");

    struct torrent_t torrent;
    struct sockaddr_in clientaddr = {0};
    int s_client;
    struct mssg_t mssg = {0};
    //Client part
    if(argc <= 2)
    {
        char *path = "./torrent_samples/client/test_file"; 
	if (create_torrent_from_metainfo_file (argv[1], &torrent, path) < 0) {
	    perror("Couldnt create metainfo file");
	    return -1;
        }
		
	    for(uint64_t x=0; x<torrent.peer_count;x++)
	    {
	        s_client = socket(AF_INET, SOCK_STREAM, 0);
		if (s_client == -1) {
		    perror("Socket couldnt create");
		    return -1;
		}
		uint32_t p_address = (uint32_t)(torrent.peers[x].peer_address[0]<<24 | torrent.peers[x].peer_address[1]<<16 | torrent.peers[x].peer_address[2]<<8 | torrent.peers[x].peer_address[3]<<0);

		clientaddr.sin_family = AF_INET;
		clientaddr.sin_port = torrent.peers[x].peer_port;
		clientaddr.sin_addr.s_addr = htonl(p_address);
	        socklen_t clientaddr_l = sizeof(clientaddr);
			
		if (connect(s_client, (struct sockaddr*)&clientaddr, clientaddr_l) == -1) {
			perror("Couldnt connect");
			if (close(s_client) < 0) { 
		            perror("Couldnt close socket client");
		        }
			continue;
		}

		char buffer[MAX_BLOCK_SIZE+RAW_MESSAGE_SIZE] = {0};
		for (uint64_t y=0; y<torrent.block_count; y++)
		{
		    
		    //Create and send the block request and later receive it	
		    mssg.b_number = y;
		    mssg.m_number = MAGIC_NUMBER;
		    mssg.mssg_code = MSG_REQUEST;
		    uint32_t m_number_nb = htonl(mssg.m_number);

		    memcpy(&buffer[0], &m_number_nb , sizeof(m_number_nb));
		    memcpy(&buffer[4], &mssg.mssg_code, sizeof(mssg.mssg_code));
		    buffer[5] = (char)(mssg.b_number >> 56);
                    buffer[6] = (char)(mssg.b_number >> 48);
                    buffer[7] = (char)(mssg.b_number >> 40);
               	    buffer[8] = (char)(mssg.b_number >> 32);
              	    buffer[9] = (char)(mssg.b_number >> 24);
                    buffer[10] = (char)(mssg.b_number >> 16);
                    buffer[11] = (char)(mssg.b_number >> 8);
                    buffer[12] = (char)(mssg.b_number >> 0);
		
		    send(s_client, buffer, RAW_MESSAGE_SIZE,0); 
		    recv(s_client, buffer, RAW_MESSAGE_SIZE,MSG_WAITALL);
	 		 			
                    memcpy(&m_number_nb, &buffer[0], sizeof(m_number_nb));
		    memcpy(&mssg.mssg_code, &buffer[4], sizeof(mssg.mssg_code)); 		

		    mssg.b_number = (uint64_t) buffer[5] << 56 | (uint64_t) buffer[6] << 48 | (uint64_t) buffer[7] << 40 | (uint64_t) buffer[8] << 32 | (uint64_t) buffer[9] << 24 | (uint64_t) buffer[10] << 16 | (uint64_t) buffer[11] << 8 | (uint64_t) buffer[12] << 0;

		    if (mssg.mssg_code != MSG_RESPONSE_NA && mssg.mssg_code == MSG_RESPONSE_OK) {//Collect the block and store it
	 	        uint64_t size_bloque = get_block_size(&torrent, y);
	 		struct block_t bloque;
	 		bloque.size = size_bloque;
	 		recv(s_client, &buffer[0], size_bloque, MSG_WAITALL);
	 		memcpy(bloque.data, &buffer, MAX_BLOCK_SIZE);
	 		store_block(&torrent, y, &bloque);
	 	    }
	 		
	        }
			
		if (close(s_client) == -1) {
		    perror("Error closing the socket");
		    return -1;
		}
		
		uint64_t z=0;
		uint64_t t=0;	
		

		while(z < torrent.block_count && t != (uint64_t)-1) {//Search if block exists	
		    if(torrent.block_map[z]) {
		        z++;
		    }
		    else {
		        t = (uint64_t)-1;
		    }
		}
			
		    if (t == 0) {
			break;
		    }
			
	    }
    }
    //Server part 
    else { 

        long port = strtol(argv[2], NULL, 10);
	char *path = "./torrent_samples/server/test_file_server";

 	if (create_torrent_from_metainfo_file (argv[3], &torrent, path) < 0) {
	    perror("Couldnt create metainfo file");
	    return -1;
	}
       	int s_server;
       	struct sockaddr_in serveraddr;
       	struct sockaddr_in s_clientaddr;
       	socklen_t s_clientaddr_l = sizeof(clientaddr);
	socklen_t serveraddr_l = sizeof(serveraddr);
		
	s_server = socket(AF_INET, SOCK_STREAM, 0);
	
        if (s_server == -1) {	
	    perror("Socket of server couldnt created");
	    return -1;
	}
	memset(&serveraddr, 0, sizeof(serveraddr));
        serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons((uint16_t) port);
	serveraddr.sin_addr.s_addr = INADDR_ANY;

       	if (bind(s_server, (struct sockaddr*)&serveraddr, serveraddr_l) == -1) {
            perror("Couldnt BIND");
       	}

       	if(listen(s_server, SOMAXCONN)){
	    perror("Couldnt LISTEN");
       	}
        	
        while(1) {//Accepting all connections

            int sClient_socket = accept(s_server, (struct sockaddr*)&s_clientaddr, &s_clientaddr_l);
            if (sClient_socket < 0) {
                perror("Couldnt ACCEPT");
            }

            int client_pid;
            client_pid = fork();
            if (client_pid == 0) {
                struct block_t bloque;
                char buffer[MAX_BLOCK_SIZE+RAW_MESSAGE_SIZE] = {0};
		bloque.size = MAX_BLOCK_SIZE;
		if (close(s_server) < 0) { 
		    perror("Couldnt close s_server");
		}

            	for(uint64_t x = 0; x < torrent.block_count; x++) {
		    mssg.b_number = x;
		    mssg.m_number = MAGIC_NUMBER;
	 	    mssg.mssg_code = MSG_REQUEST;
		    uint32_t m_number_nb = htonl(mssg.m_number);

		    if (recv(sClient_socket, buffer, RAW_MESSAGE_SIZE, MSG_WAITALL) < 0) { 
                        perror("Couldnt RECV");
		    }
		    memcpy(&m_number_nb, &buffer[0], sizeof(m_number_nb));
		    memcpy(&mssg.mssg_code, &buffer[4], sizeof(mssg.mssg_code)); 		
		    mssg.b_number = (uint64_t) buffer[5] << 56 | (uint64_t) buffer[6] << 48 | (uint64_t) buffer[7] << 40 | (uint64_t) buffer[8] << 32 | (uint64_t) buffer[9] << 24 | (uint64_t) buffer[10] << 16 | (uint64_t) buffer[11] << 8 | (uint64_t) buffer[12] << 0;                
		    
		    if (mssg.mssg_code == MSG_REQUEST) {
		        if (torrent.block_map[x] == 1) {//If wants the block and it exists
			    if (load_block(&torrent, mssg.b_number, &bloque) < 0) {
			        perror("Block coudlnt load");
				return -1;
			    }

			    mssg.mssg_code = MSG_RESPONSE_OK;
			    memcpy(&buffer[0], &m_number_nb , sizeof(m_number_nb));
  			    memcpy(&buffer[4], &mssg.mssg_code, sizeof(mssg.mssg_code));
  			    buffer[5] = (char)(mssg.b_number >> 56);
  			    buffer[6] = (char)(mssg.b_number >> 48);
  			    buffer[7] = (char)(mssg.b_number >> 40);
		 	    buffer[8] = (char)(mssg.b_number >> 32);
  			    buffer[9] = (char)(mssg.b_number >> 24);
  			    buffer[10] = (char)(mssg.b_number >> 16);
  			    buffer[11] = (char)(mssg.b_number >> 8);
  			    buffer[12] = (char)(mssg.b_number >> 0);
			    
			    if (send(sClient_socket, buffer, RAW_MESSAGE_SIZE, 0) < 0) { 
			        perror("Couldnt SEND magic number and block number info");
			    }
			    memcpy(&buffer, bloque.data, bloque.size);
			    if(send(sClient_socket, buffer, MAX_BLOCK_SIZE, 0) < 0){ 
			        perror("Couldnt SEND block data ");
                            }
			}
					            
                        else {//Block doesn't exist

			    mssg.mssg_code = MSG_RESPONSE_NA;
			    memcpy(&buffer[0], &m_number_nb , sizeof(m_number_nb));
			    memcpy(&buffer[4], &mssg.mssg_code, sizeof(mssg.mssg_code));
			    buffer[5] = (char)(mssg.b_number >> 56);
			    buffer[6] = (char)(mssg.b_number >> 48);
			    buffer[7] = (char)(mssg.b_number >> 40);
			    buffer[8] = (char)(mssg.b_number >> 32);
			    buffer[9] = (char)(mssg.b_number >> 24);
			    buffer[10] = (char)(mssg.b_number >> 16);
			    buffer[11] = (char)(mssg.b_number >> 8);
			    buffer[12] = (char)(mssg.b_number >> 0);
								
			    send(sClient_socket, buffer, RAW_MESSAGE_SIZE, 0);
                        }
                           				        
		    }
						
	        }	
		    if (close(sClient_socket) < 0) { 
		        perror("Couldnt close sClient_socket");
		    }
		    exit(0);
	    }
	    else {
	        if (close(sClient_socket) < 0) { 
		    perror("Couldnt close sClient_socket");
		}
	    }
        }
        if (close(s_server) < 0) { 
	    perror("");
	}
    }
    //Free memory
    destroy_torrent(&torrent);
    return 0;
}


