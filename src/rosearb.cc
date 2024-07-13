#include "rosearb.h"

bool rose_arb_t::load(reg_t addr, size_t len, uint8_t* bytes){
    uint32_t r = 0;
    switch (addr) {
        case ROSE_RX_DATA_ADDR: r = read_rxfifo(); break;
        case ROSE_STATUS_ADDR:  r = get_status();  break;
        default: printf("[ROSEARB]FAULTY_LOAD -- ADDR=0x%lx LEN=%lu\n", addr, len); abort();
    } 
    memcpy(bytes, &r, len);
    return true;
}

bool rose_arb_t::store(reg_t addr, size_t len, const uint8_t* bytes){
    switch (addr) {
        case ROSE_TX_DATA_ADDR: write_txfifo(*bytes); return true;
        default: printf("[ROSEARB]FAULTY_STORE -- ADDR=0x%lx LEN=%lu\n", addr, len); abort();
    }
}

void rose_arb_t::tick(reg_t UNUSED rtc_ticks){
    // if any word is in TX FIFO, send it
    while (tx_fifo.size() > 0){
        uint8_t data = tx_fifo.front();
        net_write(sync_sockfd, &data, ROSE_WORD_SIZE);
        tx_fifo.pop();
    }
 
    int n;
    // if any packet is in socket, read it
    do {
        bzero(buf, ROSE_BUF_SIZE);
        n = net_read(sync_sockfd, buf, ROSE_WORD_SIZE);
        if (n > 0) {
            uint8_t data = buf[0];
            rx_fifo.push(data);
        }
    } while (n > 0);
}
