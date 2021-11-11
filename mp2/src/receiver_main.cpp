/* 
 * File:   receiver_main.c
 * Author: 
 *
 * Created on
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#include <tcp.hpp>
#include <array>
#include <unordered_map>
#include <memory>
#include <iostream>

struct sockaddr_in si_me, si_other;
int s, slen;

void diep(char *s) {
    perror(s);
    exit(1);
}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    
    slen = sizeof (si_other);


    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("bind");


	/* Now receive data and send acknowledgements */
	FILE *fp;
	fp = fopen(destinationFile, "wb");
	if (fp == NULL) {
	    diep("fopen");
    }

    auto payload = std::make_shared<TCP::Payload>();

    /* SYN 3 way handshake */
    while (1) {
        if (recvfrom(s, payload.get(), sizeof(TCP::Payload), 0, (struct sockaddr *)&si_other, (socklen_t *)&slen) < 0) {
            diep("recvfrom");
        }
        if (payload->type == TCP::SYN) {
            std::cout << "recieved SYN\n";
            break;
        }
    }
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &TCP::tout, sizeof(TCP::tout)) < 0) {
        diep("setsockopt");
    }
    while (1) {
        payload->type = TCP::ACKSYN;
        if (sendto(s, payload.get(), sizeof(TCP::Payload), 0, (struct sockaddr *)&si_other, slen) < 0) {
            diep("sendto");
        }
        std::cout << "sent ACKSYN\n";
        if (recvfrom(s, payload.get(), sizeof(TCP::Payload), 0, (struct sockaddr *)&si_other, (socklen_t *)&slen) < 0
            && errno != EWOULDBLOCK) {
            diep("recvfrom");
        }
        if (payload->type == TCP::ACK) {
            break;
        }
    }
    std::cout << "recieved ACK\n";
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &TCP::no_tout, sizeof(TCP::no_tout)) < 0) {
        diep("setsockopt");
    }

    /* data receive loop */
    std::unordered_map<size_t, std::shared_ptr<TCP::Payload>> cache;
    size_t packet_needed = 0;
    while (1) {
        if (recvfrom(s, payload.get(), sizeof(TCP::Payload), 0, (struct sockaddr *)&si_other, (socklen_t *)&slen) < 0) {
            diep("recvfrom");
        }
        if (payload->type == TCP::DATA) {
            std::cout << "P" << payload->idx << "\n";
            // if (payload->idx >= packet_needed) {
                cache[payload->idx] = payload;
            // }
            while (cache.find(packet_needed) != cache.end()) {
                fwrite(cache[packet_needed].get(), 1, cache[packet_needed]->size, fp);
                cache.erase(packet_needed);
                packet_needed++;
            }
            if (packet_needed > 0) {
                payload = std::make_shared<TCP::Payload>();
                payload->type = TCP::ACK;
                payload->idx = packet_needed - 1;
                if (sendto(s, payload.get(), sizeof(TCP::Payload), 0, (struct sockaddr *)&si_other, slen) < 0) {
                    diep("sendto");
                }
                std::cout << "ACK" << payload->idx << "\n";
            }
        } else if (payload->type == TCP::FIN) {
            std::cout << "recieved FIN\n";
            fclose(fp);
            /* FIN 3 way handshake */
            if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &TCP::tout, sizeof(TCP::tout)) < 0) {
                diep("setsockopt");
            }
            while (1) {
                payload = std::make_shared<TCP::Payload>();
                payload->type = TCP::ACKFIN;
                if (sendto(s, payload.get(), sizeof(TCP::Payload), 0, (struct sockaddr *)&si_other, slen) < 0) {
                    diep("sendto");
                }
                std::cout << "sent ACKFIN\n";
                if (recvfrom(s, payload.get(), sizeof(TCP::Payload), 0, (struct sockaddr *)&si_other, (socklen_t *)&slen) < 0
                    && errno != EWOULDBLOCK) {
                    diep("huh");
                }
                if (payload->type == TCP::ACK) {
                    break;
                }
            }
        }
    }
    std::cout << "recieved ACK\n";

    close(s);
	printf("%s received.", destinationFile);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}

