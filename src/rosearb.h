#ifndef _ROSE_ARB_H
#define _ROSE_ARB_H

#include <riscv/abstract_device.h>
#include <riscv/dts.h>
#include <riscv/sim.h>
#include <fesvr/term.h>
#include <fdt/libfdt.h>

#include <sys/socket.h>
#include <netinet/in.h> // for sockaddr_in
#include <netdb.h> // for gethostbyname 
#include <unistd.h> // for close

#define ROSE_BUF_SIZE      1024
#define ROSE_WORD_SIZE     4

#define ROSE_INT_ID        3
#define ROSE_SIZE          0x1000

#define ROSE_BASE_ADDR     (0x2000)
#define ROSE_STATUS_ADDR   (0x00)
#define ROSE_TX_DATA_ADDR  (0x08)
#define ROSE_RX_DATA_ADDR  (0x10)

#define ROSE_GET_TX_READY(status)  (status & 0x1)
#define ROSE_GET_RX_VALID(status)  (status & 0x2)

class rose_arb_t : public abstract_device_t {
public:
	rose_arb_t(abstract_interrupt_controller_t *intctrl, reg_t int_id) {
    this->intctrl = intctrl;
    this->interrupt_id = int_id;
    setup_socket();
  }

  ~rose_arb_t() {
    if (sync_sockfd >= 0) {
      close(sync_sockfd);
    }
  }

  bool load(reg_t addr, size_t len, uint8_t* bytes) override;
  bool store(reg_t addr, size_t len, const uint8_t* bytes) override;
  void tick(reg_t UNUSED rtc_ticks) override;

private: 
  // variables for socket connection
  int sync_sockfd, data_sockfd, sync_portno, data_portno, n;
  char *hostname;
  struct sockaddr_in sync_serveraddr;
  struct sockaddr_in data_serveraddr;
  struct hostent *server;

  // variable for device interrupt, UNUSED for now
	reg_t interrupt_id;
	abstract_interrupt_controller_t *intctrl;
	
  // FIFOs for TX and RX
  std::queue<uint8_t> rx_fifo;
  std::queue<uint8_t> tx_fifo;

  // status bits
  bool tx_ready = true;
  bool rx_valid = false;

  // Buffer for temporarily storing rx data
  char buf[ROSE_WORD_SIZE];

  // Read one entry from RX FIFO
	uint32_t read_rxfifo() {
    if (!rx_fifo.size()) {
      printf("[ROSEARB]: Attempt to read from empty RX FIFO\n");
      abort();
    }
    uint8_t r = rx_fifo.front();
    rx_fifo.pop();
    return r;
  }

  // Write one entry to TX FIFO
  uint32_t write_txfifo(uint8_t data) {
    tx_fifo.push(data);
    return 0;
  }
	
  // initialize the socket connection
	void setup_socket(){
    this->hostname = "localhost";
    this->sync_portno = 10001;
    this->data_portno = 60002;

    /* socket: create the socket */
    this->sync_sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (this->sync_sockfd < 0)
    {
        perror("ERROR opening socket");
        exit(0);
    }

    /* gethostbyname: get the server's DNS entry */
    this->server = gethostbyname(hostname);
    if (this->server == NULL)
    {
        fprintf(stderr, "ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *)&this->sync_serveraddr, sizeof(this->sync_serveraddr));
    this->sync_serveraddr.sin_family = AF_INET;
    bcopy((char *)this->server->h_addr,
          (char *)&this->sync_serveraddr.sin_addr.s_addr, this->server->h_length);
    this->sync_serveraddr.sin_port = htons(this->sync_portno);

    /* connect: create a connection with the server */
    while (connect(this->sync_sockfd, (const sockaddr *)&this->sync_serveraddr, sizeof(this->sync_serveraddr)) < 0)
        ;
    printf("Connected to sync server!\n");
  }

  ssize_t net_write(int fd, const void *buf, size_t count) {
      return send(fd, buf, count, 0);
  }

  ssize_t net_read(int fd, void *buf, size_t count) {
    return recv(fd, buf, count, 0);
  }

  uint32_t get_status(){
    uint32_t status = 0;
    // set tx_ready
    status |= 0x1;
    // set rx_valid
    if (rx_fifo.size() > 0) {
      status |= 0x2;
    }
    return status;
  }
};

#endif