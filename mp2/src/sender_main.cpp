/* 
 * File:   sender_main.c
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
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

#include <tcp.hpp>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <fstream>
#include <iostream>

struct sockaddr_in si_other;
int s, slen;

void diep(char *s) {
    perror(s);
    exit(1);
}

namespace {
    enum State { SlowStart, CongestionAvoidance, FastRecovery } state;

    std::unordered_map<size_t, std::shared_ptr<TCP::Payload>> cache;

    void sendPacket(size_t idx, std::ifstream& fin, bool only_new) {
        if (cache.find(idx) == cache.end()) {
            auto& chunk = cache[idx];
            chunk = std::make_shared<TCP::Payload>();
            fin.read(chunk->data.data(), sizeof(chunk->data));
            chunk->size = fin.gcount();
            chunk->idx = idx;
            chunk->type = TCP::DATA;
            if (sendto(s, chunk.get(), sizeof(TCP::Payload), 0, (struct sockaddr *)&si_other, slen) < 0) {
                diep("sendto");
            }
        } else if (!only_new) {
            if (sendto(s, cache[idx].get(), sizeof(TCP::Payload), 0, (struct sockaddr *)&si_other, slen) < 0) {
                diep("sendto");
            }
        }
    }
}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    //Open the file
    FILE *fp;
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

	/* Determine how many bytes to transfer */

    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

	/* Send data and receive acknowledgements on s*/ 
    auto payload = std::make_shared<TCP::Payload>();

    /* SYN 3 way handshake */
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &TCP::tout, sizeof(TCP::tout)) < 0) {
        diep("setsockopt");
    }
    while (1) {
        payload->type = TCP::SYN;
        if (sendto(s, payload.get(), sizeof(TCP::Payload), 0, (struct sockaddr *)&si_other, slen) < 0) {
            diep("sendto");
        }
        std::cout << "sent SYN\n";
        if (recvfrom(s, payload.get(), sizeof(TCP::Payload), 0, (struct sockaddr *)&si_other, (socklen_t *)&slen) < 0
            && errno != EWOULDBLOCK) {
            diep("recvfrom");
        }
        if (payload->type == TCP::ACKSYN) {
            break;
        }
    }
    std::cout << "recieved ACKSYN\n";
    payload->type = TCP::ACK;
    if (sendto(s, payload.get(), sizeof(TCP::Payload), 0, (struct sockaddr *)&si_other, slen) < 0) {
        diep("sendto");
    }
    std::cout << "sent ACK\n";

    /* data receive loop */
    // FILE *fp;
    // fp = fopen(filename, "rb");
    std::ifstream fin(filename, std::ifstream::binary);

    state = SlowStart;
    float cw = 1;
    int sst = 64;
    int dupack = 0;
    size_t window_bottom = 0;
    std::unordered_set<size_t> pkts_rec;
    while (!fin.eof()) {
        auto num_bytes = recvfrom(s, payload.get(), sizeof(TCP::Payload), 0, (struct sockaddr *)&si_other, (socklen_t *)&slen);
        if (num_bytes < 0) {
            if (errno == EWOULDBLOCK) {
                /* timeout */
                state = SlowStart;
                sst = cw / 2;
                cw = 1;
                dupack = 0;
                sendPacket(window_bottom, fin, false);
            } else {
                diep("recvfrom");
            }
        } else if (payload->type == TCP::ACK) {
            pkts_rec.insert(payload->idx);
            if (payload->idx == window_bottom - 1) {
                /* dupACK */
                dupack++;
                if (state == FastRecovery) {
                    sendPacket(window_bottom+cw, fin, true);
                    cw++;
                } else if (dupack == 3) {
                    state = FastRecovery;
                    const int old_top = window_bottom + cw - 1;
                    sst = cw / 2;
                    cw = sst + 3;
                    sendPacket(window_bottom, fin, false);
                    for (int i = old_top; i < window_bottom + int(cw); i++) {
                        sendPacket(i, fin, true);
                    }
                }
            } else {
                /* newACK */
                dupack = 0;
                const int old_top = window_bottom + cw - 1;
                while (pkts_rec.find(window_bottom) != pkts_rec.end()) {
                    window_bottom++;
                }
                switch (state) {
                case SlowStart:
                    cw++;
                    break;
                case CongestionAvoidance:
                    cw += 1 / int(cw);
                    break;
                case FastRecovery:
                    cw = sst;
                    state = CongestionAvoidance;
                    break;
                }
                for (int i = old_top; i < window_bottom + int(cw); i++) {
                    sendPacket(i, fin, false);
                }
            }
        } else if (payload->type == TCP::ACKSYN) {
            payload = std::make_shared<TCP::Payload>();
            payload->type = TCP::ACK;
            if (sendto(s, payload.get(), sizeof(TCP::Payload), 0, (struct sockaddr *)&si_other, slen) < 0) {
                diep("sendto");
            }
            std::cout << "sent ACK\n";
        }
        if (state == SlowStart && cw >= sst) {
            state = CongestionAvoidance;
        }
    }

    /* FIN 3 way handshake */
    while (1) {
        payload->type = TCP::FIN;
        if (sendto(s, payload.get(), sizeof(TCP::Payload), 0, (struct sockaddr *)&si_other, slen) < 0) {
            diep("sendto");
        }
        std::cout << "sent FIN\n";
        if (recvfrom(s, payload.get(), sizeof(TCP::Payload), 0, (struct sockaddr *)&si_other, (socklen_t *)&slen) < 0
            && errno != EWOULDBLOCK) {
            diep("recvfrom");
        }
        if (payload->type == TCP::ACKFIN) {
            break;
        }
    }
    std::cout << "recieved ACKFIN\n";
    payload->type = TCP::ACK;
    if (sendto(s, payload.get(), sizeof(TCP::Payload), 0, (struct sockaddr *)&si_other, slen) < 0) {
        diep("sendto");
    }
    std::cout << "sent ACK\n";

    fclose(fp);
    printf("Closing the socket\n");
    close(s);
    return;

}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);



    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);


    return (EXIT_SUCCESS);
}


