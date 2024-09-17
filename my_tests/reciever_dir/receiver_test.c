#include <rte_ethdev.h>
#include <rte_eal.h>
#include <rte_mbuf.h>
#include <arpa/inet.h>  // For converting IPs to readable format

#define RX_RING_SIZE 2048
#define TX_RING_SIZE 1024
#define NUM_MBUFS (8192-1)  // optimum value is 2^q - 1
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

// Ethernet header structure
struct ether_hdr {
    uint8_t dst_addr[6]; // Destination MAC address
    uint8_t src_addr[6]; // Source MAC address
    uint16_t ether_type; // Protocol type (IP, ARP, etc.)
};

// Function to convert MAC address to human-readable format
void print_mac_addr(const uint8_t *mac_addr) {
    printf("%02X:%02X:%02X:%02X:%02X:%02X", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
}

// Initialize a port for packet reception
static int port_init(uint16_t port, struct rte_mempool *mbuf_pool) {
    struct rte_eth_conf port_conf = {0};
    struct rte_eth_rxconf rxq_conf;
    struct rte_eth_txconf txq_conf;

    if (!rte_eth_dev_is_valid_port(port)) {
        return -1;
    }

    int retval = rte_eth_dev_configure(port, 1, 1, &port_conf);
    if (retval != 0) {
        return retval;
    }

    retval = rte_eth_rx_queue_setup(port, 0, RX_RING_SIZE, rte_eth_dev_socket_id(port), NULL, mbuf_pool);
    if (retval < 0) {
        return retval;
    }

    retval = rte_eth_tx_queue_setup(port, 0, TX_RING_SIZE, rte_eth_dev_socket_id(port), NULL);
    if (retval < 0) {
        return retval;
    }

    retval = rte_eth_dev_start(port);
    if (retval < 0) {
        return retval;
    }

    rte_eth_promiscuous_enable(port);

    return 0;
}

int main(int argc, char *argv[]) {
    struct rte_mempool *mbuf_pool;
    uint16_t port_id = 0;

    if (rte_eal_init(argc, argv) < 0) {
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
    }

    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS, MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (mbuf_pool == NULL) {
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
    }
    rte_mempool_list_dump(stdout);
    
    printf("rte_mempool_avail_count(mbfu_pool): %u\n", rte_mempool_avail_count(mbuf_pool));
    printf("rte_mempool_in_use_count(mbfu_pool): %u\n", rte_mempool_in_use_count(mbuf_pool));


    if (port_init(port_id, mbuf_pool) != 0) {
        rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu16 "\n", port_id);
    }

    struct rte_eth_link link;
    rte_eth_link_get_nowait(port_id, &link);
    printf("Link status: %s\n", link.link_status ? "UP" : "DOWN");

    while (1) {
        struct rte_mbuf *bufs[BURST_SIZE];
        const uint16_t nb_rx = rte_eth_rx_burst(port_id, 0, bufs, BURST_SIZE);

        if (nb_rx > 0) {
            printf("Received %u packets\n", nb_rx);

            for (uint16_t i = 0; i < nb_rx; i++) {
                struct rte_mbuf *pkt = bufs[i];
                rte_pktmbuf_dump(stdout, pkt, 1000);
                printf("rte_mempool_in_use_count(mbfu_pool): %u\n", rte_mempool_in_use_count(mbuf_pool));

                struct ether_hdr *eth_hdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);

                // Print basic packet info
                printf("Packet %u: length = %u bytes\n", i + 1, rte_pktmbuf_pkt_len(pkt));
                printf("  Source MAC: ");
                print_mac_addr(eth_hdr->src_addr);
                printf("\n  Destination MAC: ");
                print_mac_addr(eth_hdr->dst_addr);
                printf("\n  EtherType: 0x%04X\n", ntohs(eth_hdr->ether_type));

                // Optional: print raw data from the packet
                uint8_t *data = rte_pktmbuf_mtod_offset(pkt, uint8_t *, sizeof(struct ether_hdr));
                printf("  Packet data: ");
                for (uint32_t j = 0; j < 16 && j < rte_pktmbuf_pkt_len(pkt); j++) {
                    printf("%02X ", data[j]);
                }
                printf("\n");

                // Free the buffer after processing
                rte_pktmbuf_free(pkt);
            }
        }
    }

    return 0;
}
