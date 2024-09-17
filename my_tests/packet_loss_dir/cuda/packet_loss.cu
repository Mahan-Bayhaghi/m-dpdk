#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <cuda_runtime.h>

#define TX_RING_SIZE 512
#define NUM_MBUFS (16 * 1024)
#define MBUF_CACHE_SIZE 32
#define BURST_SIZE 256
#define TOTAL_PACKETS (10612 - 1)

static const struct rte_eth_conf port_conf_default = {
    .rxmode = {.max_lro_pkt_size = RTE_ETHER_MAX_LEN}
};

// Define ping-pong buffer structure
#define PING_PONG_SIZE 256
struct ping_pong_buffer {
    struct rte_mbuf *pkts[PING_PONG_SIZE];
    uint16_t count;
};

// Function to perform Host to Device (HD) transfer
void host_to_device(struct ping_pong_buffer *buffer, char *d_buffer, size_t buffer_size) {
    // Allocate memory for packet data
    size_t total_data_size = 0;
    for (int i = 0; i < buffer->count; i++) {
        total_data_size += rte_pktmbuf_pkt_len(buffer->pkts[i]);
    }
    if (total_data_size > buffer_size) {
        printf("Error: Not enough device memory\n");
        return;
    }

    // Copy packet data to a host buffer
    char *h_buffer = (char *)malloc(total_data_size);
    char *cur_ptr = h_buffer;
    for (int i = 0; i < buffer->count; i++) {
        size_t pkt_len = rte_pktmbuf_pkt_len(buffer->pkts[i]);
        rte_memcpy(cur_ptr, rte_pktmbuf_mtod(buffer->pkts[i], const void*), pkt_len);
        cur_ptr += pkt_len;
    }

    // Copy data from host to device (H2D)
    cudaMemcpy(d_buffer, h_buffer, total_data_size, cudaMemcpyHostToDevice);

    // Free the host buffer
    free(h_buffer);

    // Process or send packets on the device (can be replaced with CUDA kernel launch)
    printf("Transferred %lu bytes from Host to Device\n", total_data_size);

    // Reset the ping-pong buffer after transfer
    for (int i = 0; i < buffer->count; i++) {
        rte_pktmbuf_free(buffer->pkts[i]);
    }
    buffer->count = 0;
}

// Deep copy mbuf into the ping-pong buffer
struct rte_mbuf *deep_copy_mbuf(struct rte_mbuf *src_mbuf, struct rte_mempool *mbuf_pool) {
    struct rte_mbuf *dst_mbuf = rte_pktmbuf_alloc(mbuf_pool); // Allocate a new mbuf
    if (dst_mbuf == NULL) {
        printf("Mbuf allocation failed\n");
        return NULL;
    }
    // Copy packet data and metadata (header + payload)
    if (rte_pktmbuf_pkt_len(src_mbuf) > rte_pktmbuf_tailroom(dst_mbuf)) {
        printf("Not enough space in the destination mbuf\n");
        rte_pktmbuf_free(dst_mbuf);
        return NULL;
    }
    // Copy the packet payload
    rte_memcpy(rte_pktmbuf_mtod(dst_mbuf, void*),
               rte_pktmbuf_mtod(src_mbuf, const void*),
               rte_pktmbuf_pkt_len(src_mbuf));
    // Set the length and metadata
    rte_pktmbuf_pkt_len(dst_mbuf) = rte_pktmbuf_pkt_len(src_mbuf);
    rte_pktmbuf_data_len(dst_mbuf) = rte_pktmbuf_data_len(src_mbuf);
    return dst_mbuf;
}

int main(int argc, char *argv[]) {
    int RX_RING_SIZE;
    int sleep_ms;
    FILE *file;
    file = fopen("config", "r");
    if (file == NULL) {
        perror("Failed to open config file");
        return 1;
    }
    if (fscanf(file, "%d", &RX_RING_SIZE) != 1 || fscanf(file, "%d", &sleep_ms) != 1) {
        fprintf(stderr, "Failed to read from config file\n");
        fclose(file);
        return 1;
    }
    fclose(file);

    struct rte_mempool *mbuf_pool;
    struct rte_eth_dev_info dev_info;
    struct rte_eth_stats eth_stats;
    uint16_t port_id = 0;

    if (rte_eal_init(argc, argv) < 0)
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS,
                                        MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
                                        rte_socket_id());
    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    if (rte_eth_dev_info_get(port_id, &dev_info) != 0)
        rte_exit(EXIT_FAILURE, "Error getting device info\n");

    if (rte_eth_dev_configure(port_id, 1, 1, &port_conf_default) != 0)
        rte_exit(EXIT_FAILURE, "Cannot configure device\n");

    if (rte_eth_rx_queue_setup(port_id, 0, RX_RING_SIZE, rte_eth_dev_socket_id(port_id), NULL, mbuf_pool) != 0)
        rte_exit(EXIT_FAILURE, "Cannot setup RX queue\n");

    if (rte_eth_tx_queue_setup(port_id, 0, TX_RING_SIZE, rte_eth_dev_socket_id(port_id), NULL) != 0)
        rte_exit(EXIT_FAILURE, "Cannot setup TX queue\n");

    if (rte_eth_dev_start(port_id) != 0)
        rte_exit(EXIT_FAILURE, "Cannot start device\n");

    if (rte_eth_promiscuous_enable(port_id) != 0)
        rte_exit(EXIT_FAILURE, "Cannot enable promiscuous mode\n");

    printf("Port %u initialized and started\n", port_id);

    struct ping_pong_buffer ping = {.count = 0};
    struct ping_pong_buffer pong = {.count = 0};
    struct ping_pong_buffer *active_buffer = &ping;
    int is_ping_active = 1;

    char *d_buffer;
    size_t buffer_size = PING_PONG_SIZE * RTE_MBUF_DEFAULT_BUF_SIZE;
    cudaMalloc((void**)&d_buffer, buffer_size);

    struct rte_mbuf *bufs[BURST_SIZE];
    uint16_t nb_rx;
    int total_drops = 0;

    while (1) {
        nb_rx = rte_eth_rx_burst(port_id, 0, bufs, BURST_SIZE);

        if (nb_rx > 0) {
            for (int i = 0; i < nb_rx; i++) {
                if (bufs[i] == NULL) {
                    printf("Mbuf allocation failed. Packet dropped.\n");
                    total_drops++;
                } else {
                    struct rte_mbuf *copy = deep_copy_mbuf(bufs[i], mbuf_pool);
                    if (copy == NULL) {
                        total_drops++;
                        continue;
                    }
                    active_buffer->pkts[active_buffer->count] = copy;
                    active_buffer->count++;
                    if (active_buffer->count >= PING_PONG_SIZE) {
                        host_to_device(active_buffer, d_buffer, buffer_size);
                        is_ping_active = !is_ping_active;
                        active_buffer = is_ping_active ? &ping : &pong;
                    }
                }
                rte_pktmbuf_free(bufs[i]);
            }

            rte_eth_stats_get(port_id, &eth_stats);
            if ((eth_stats.ipackets + eth_stats.imissed) >= TOTAL_PACKETS) break;
        }

        rte_delay_ms(sleep_ms);
    }

    printf("Packets received: %lu, Packets dropped at NIC: %lu, Total drops at application layer: %d\n",
           eth_stats.ipackets, eth_stats.imissed, total_drops);

    rte_eth_dev_stop(port_id);
    rte_eth_dev_close(port_id);
    cudaFree(d_buffer);
    printf("Port %u closed\n", port_id);

    return 0;
}
