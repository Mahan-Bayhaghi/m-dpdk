#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_errno.h>
#include <stdio.h>
#include <stdlib.h>

// #define RX_RING_SIZE 2048   // should be: <= 4096, >= 64, and a product of 32
#define TX_RING_SIZE 512    // we are not transmitting anything so not that important
#define NUM_MBUFS (16 * 1024)  //  you can not create mempools with size less than 64 mbufs !
#define MBUF_CACHE_SIZE 32
#define BURST_SIZE 256
#define TOTAL_PACKETS (10612 - 1)

static const struct rte_eth_conf port_conf_default = {
    .rxmode = {.max_lro_pkt_size = RTE_ETHER_MAX_LEN}
};

///////////////////// Define ping-pong buffer structure /////////////////////
#define PING_PONG_SIZE 256
struct ping_pong_buffer {
    struct rte_mbuf *pkts[PING_PONG_SIZE];
    uint16_t count;
};

// Function to simulate Host to Device transfer
void host_to_device(struct ping_pong_buffer *buffer) {
    // printf("Host to Device transfer: Transferring %u packets\n", buffer->count);
    // Simulate processing or sending to another device
    for (int i = 0; i < buffer->count; i++) {
        rte_pktmbuf_free(buffer->pkts[i]);
    }
    buffer->count = 0; // Reset the buffer after processing
}
// Deep copy mbuf into the ping pong buffer
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

    ///////////////////// Read config from file /////////////////////
    int RX_RING_SIZE;
    int sleep_ms;
    FILE *file;
    // Open the config file for reading
    file = fopen("config", "r");
    if (file == NULL) {
        perror("Failed to open config file");
        return 1;
    }
    // Read the first line into rx
    if (fscanf(file, "%d", &RX_RING_SIZE) != 1) {
        fprintf(stderr, "Failed to read rx from config file\n");
        fclose(file);
        return 1;
    }
    if (fscanf(file, "%d", &sleep_ms) != 1) {
        fprintf(stderr, "Failed to read ms from config file\n");
        fclose(file);
        return 1;
    }
    fclose(file);

    ///////////////////// Initializing mempool and eth device /////////////////////
    struct rte_mempool *mbuf_pool;
    struct rte_eth_dev_info dev_info;
    struct rte_eth_stats eth_stats;
    uint16_t port_id = 0;

    // Initialize the Environment Abstraction Layer (EAL)
    if (rte_eal_init(argc, argv) < 0)   // the cmdline inputs are solely EAL arguments
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	printf("rte_eth_dev_count_total = %"PRIu16"\n", rte_eth_dev_count_total());

    // Createing a  mempool

    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS,
                                        MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
                                        rte_socket_id());
    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    // Initialize the Ethernet device
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
    
    // our test pcap file may have not captured packets with valid MAC addresses
    // to overcome this problem, enable promiscuous mode 
    if (rte_eth_promiscuous_enable(port_id) !=0)
        rte_exit(EXIT_FAILURE, "Cannot enable promiscuous mode\n");
    printf("Testing with ping pong size=%d\n", PING_PONG_SIZE);
    printf("Port %u initialized and started\n", port_id);

    ///////////////////// Initialize ping-pong buffers /////////////////////
    struct ping_pong_buffer ping = {.count = 0};
    struct ping_pong_buffer pong = {.count = 0};
    struct ping_pong_buffer *active_buffer = &ping; // Start with the ping buffer
    int is_ping_active = 1; // Flag to track active buffer

    ///////////////////// Waiting for burst to arrive /////////////////////
    struct rte_mbuf *bufs[BURST_SIZE];
    uint16_t nb_rx;
    int total_drops = 0;
    
    int nb_ports = rte_eth_dev_count_avail();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");
    else
        printf("nb_ports: %d\n", nb_ports);

    while (1) { // keep waiting for burst to arrive
        nb_rx = rte_eth_rx_burst(port_id, 0, bufs, BURST_SIZE);

        if (nb_rx > 0) {
            printf("Received %u packets\n", nb_rx);
            // TODO: simply copy the packets to a ping pong data structure. when the data structure is full, perform a Host to Device
            for (int i = 0; i < nb_rx; i++) {
                if (bufs[i] == NULL) {
                    printf("Mbuf allocation failed. Packet dropped.\n");
                    total_drops++;
                } else{
                    // Deep copy the mbuf into the active ping-pong buffer
                    struct rte_mbuf *copy = deep_copy_mbuf(bufs[i], mbuf_pool);
                    if (copy == NULL) {
                        total_drops++;
                        continue;
                    }
                    active_buffer->pkts[active_buffer->count] = copy;
                    active_buffer->count++;
                    // If active buffer is full, perform HD transfer and switch buffer
                    if (active_buffer->count >= PING_PONG_SIZE) {
                        host_to_device(active_buffer);
                        is_ping_active = !is_ping_active; // Toggle ping-pong buffer
                        active_buffer = is_ping_active ? &ping : &pong;
                    }
                }
                // Free mbuf after processing
                rte_pktmbuf_free(bufs[i]);
            }
            // Retrieve NIC stats
            rte_eth_stats_get(port_id, &eth_stats);
            // printf("Packets received: %lu, Packets dropped at NIC: %lu, Total drops at application layer: %d\n",
                // eth_stats.ipackets, eth_stats.imissed, total_drops);
            if ((eth_stats.ipackets + eth_stats.imissed) >= TOTAL_PACKETS ) break;  // if all packets are received, end the program
        }

        // Sleep for a bit to simulate processing delay
        // not nessecary if HD is implemented 
        rte_delay_ms(sleep_ms);
    }

    printf("Packets received: %lu, Packets dropped at NIC: %lu, Total drops at application layer: %d\n",
        eth_stats.ipackets, eth_stats.imissed, total_drops);
    rte_eth_dev_stop(port_id);
    rte_eth_dev_close(port_id);
    printf("Port %u closed\n", port_id);

    return 0;
}