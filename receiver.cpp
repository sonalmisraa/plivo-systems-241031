#include "protocol.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

static int make_socket() {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {  perror("socket");     exit(1);  }
    return fd;
}

static sockaddr_in make_addr(uint16_t port) {
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = inet_addr("127.0.0.1");
    return a;
}

static double env_f(const char* name, double def) {
    const char* v = getenv(name);
    return v ? atof(v) : def;
}

struct Frame {
    std::atomic<bool> ready{false};
    uint8_t payload[PAYLOAD_SIZE];
};

struct GroupState {
    std::mutex m;
    bool data_present[FEC_GROUP_SIZE] = {};
    uint8_t data_payload[FEC_GROUP_SIZE][PAYLOAD_SIZE];
    bool parity_present = false;
    uint8_t parity_payload[PAYLOAD_SIZE];
    int data_count = 0;
    bool recovered = false;
};

int main() {
    double duration_s = env_f("DURATION_S", 30);

    int n_frames = static_cast<int>(duration_s * 1000.0 / 20.0) + 8;
    int n_groups = (n_frames + FEC_GROUP_SIZE - 1) / FEC_GROUP_SIZE + 2;

    std::vector<Frame> frames(n_frames);
    std::vector<GroupState> groups(n_groups);

    int in_fd = make_socket();
    sockaddr_in in_addr = make_addr(47002);

    if (bind(in_fd, reinterpret_cast<sockaddr*>(&in_addr), sizeof(in_addr)) < 0) { perror("bind 47002");
        close(in_fd);
        return 1; }

    int out_fd = make_socket();
    sockaddr_in player_addr = make_addr(47020);

    auto set_frame = [&](uint32_t seq, const uint8_t* payload) {
        if (seq >= static_cast<uint32_t>(n_frames))   return;

        Frame& f = frames[seq];
        bool expected = false;
        if (!f.ready.compare_exchange_strong(expected, true, std::memory_order_acq_rel, std::memory_order_acquire))  return;

        memcpy(f.payload, payload, PAYLOAD_SIZE);

        unsigned char out[4 + PAYLOAD_SIZE];
        uint32_t seq_be = htonl(seq);
        memcpy(out, &seq_be, sizeof(seq_be));
        memcpy(out + 4, f.payload, PAYLOAD_SIZE);

        if (sendto(out_fd, out, sizeof(out), 0,reinterpret_cast<sockaddr*>(&player_addr), sizeof(player_addr)) < 0)perror("send PLAYER");
    };

    auto try_recover = [&](uint16_t group) {
        if (group >= groups.size())  return;

        GroupState& g = groups[group];
        uint8_t recon[PAYLOAD_SIZE];
        int missing_idx = -1;
        {
            std::lock_guard<std::mutex> lk(g.m);
            if (g.recovered || !g.parity_present || g.data_count != FEC_GROUP_SIZE - 1)
                return;

            memcpy(recon, g.parity_payload, PAYLOAD_SIZE);

            for (int i = 0; i < FEC_GROUP_SIZE; i++) {
                if (g.data_present[i]) {
                    for (int b = 0; b < PAYLOAD_SIZE; b++)
                        recon[b] ^= g.data_payload[i][b];
                } else {
                    missing_idx = i;
                }
            }

            if (missing_idx < 0)
                return;

            g.recovered = true;
        }

        uint32_t missing_seq = static_cast<uint32_t>(group) * FEC_GROUP_SIZE +
            static_cast<uint32_t>(missing_idx);
        set_frame(missing_seq, recon);
    };

    unsigned char buf[2048];

    while (true) {
        ssize_t n = recvfrom(in_fd, buf, sizeof(buf), 0, nullptr, nullptr);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("recvfrom");
            break;
        }

        if (n != static_cast<ssize_t>(sizeof(Packet)))
            continue;

        Packet pkt;
        memcpy(&pkt, buf, sizeof(pkt));
        pkt.seq = ntohl(pkt.seq);
        pkt.group = ntohs(pkt.group);

        if (pkt.group >= groups.size()) continue;

        if (pkt.type == DATA) {
            if (pkt.seq >= static_cast<uint32_t>(n_frames))  continue;

            uint16_t expected_group = static_cast<uint16_t>(pkt.seq / FEC_GROUP_SIZE);
            if (pkt.group != expected_group)  continue;

            int idx = static_cast<int>(pkt.seq - static_cast<uint32_t>(pkt.group) * FEC_GROUP_SIZE);
            if (idx < 0 || idx >= FEC_GROUP_SIZE)continue;

            set_frame(pkt.seq, pkt.payload);
            GroupState& g = groups[pkt.group];
            {
                std::lock_guard<std::mutex> lk(g.m);
                if (!g.data_present[idx]) {
                    g.data_present[idx] = true;
                    memcpy(g.data_payload[idx], pkt.payload, PAYLOAD_SIZE);
                    g.data_count++;
                }
            }

            try_recover(pkt.group);
        } else if (pkt.type == PARITY) {
            GroupState& g = groups[pkt.group];
            {
                std::lock_guard<std::mutex> lk(g.m);
                if (!g.parity_present) { g.parity_present = true;
                    memcpy(g.parity_payload, pkt.payload, PAYLOAD_SIZE);   }
            }

            try_recover(pkt.group);
        }
    }

    close(in_fd);
    close(out_fd);
    return 0;
}