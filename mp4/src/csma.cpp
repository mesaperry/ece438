#include<stdio.h>
#include<string.h>
#include<stdlib.h>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

namespace {
static int N;
static int L;
static std::vector<int> Rs;
static int M;
static int T;

static int tick = 0;
static int success_ct = 0;

struct Node {
    int id;
    int backoff;
    int collision_ct;
    int transmission_ct;
};
}

int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 2) {
        printf("Usage: ./csma input.txt\n");
        return -1;
    }

    std::ifstream file_in(argv[1]);
    std::string line;

    getline(file_in, line);
    N = std::stoi(line.substr(2));

    getline(file_in, line);
    L = std::stoi(line.substr(2));

    getline(file_in, line);
    std::stringstream iss(line.substr(2));
    int number;
    while (iss >> number) {
        Rs.push_back(number);
    }

    getline(file_in, line);
    M = std::stoi(line.substr(2));

    getline(file_in, line);
    T = std::stoi(line.substr(2));

//    std::cout << N << "\n" << L << "\n";
//    for (auto e : Rs) {
//        std::cout << e << " ";
//    }
//    std::cout << "\n" << M << "\n" << T << "\n";

    std::vector<Node> nodes;
    for (int i = 0; i < N; i++) {
        nodes.push_back({i, i % Rs[0], 0, 0});
    }

    while (tick < T) {
//        for (auto& e : nodes) {
//            std::cout << e.backoff << " ";
//        }
//        std::cout << "\n";
        switch (std::count_if(nodes.begin(), nodes.end(), [](Node& e){ return e.backoff == 0; })) {
            case 0:
                for (auto& node : nodes) {
                    node.backoff--;
                }
                break;
            case 1: {
                Node& tx_node = *std::find_if(nodes.begin(), nodes.end(), [](Node& e){ return e.backoff == 0; });
                success_ct++;
                tx_node.transmission_ct++;
                if (tx_node.transmission_ct == L) {
                    tx_node.backoff = (tx_node.id + tick + 1) % Rs[tx_node.collision_ct];
                    tx_node.transmission_ct = 0;
                }
                break;
            }
            default:
                for (auto& node : nodes) {
                    if (node.backoff == 0) {
                        node.collision_ct++;
                        node.collision_ct %= M;
                        node.backoff = (node.id + tick + 1) % Rs[node.collision_ct];
                    }
                }
                break;
        }
        tick++;
    }

//    std::cout << success_ct << " " << T << "\n";

    std::ofstream file_out;
    file_out.open("output.txt");
    file_out << std::fixed << std::setprecision(2);
    file_out << (static_cast<float>(success_ct) / T);
    file_out.close();

    return 0;
}

