#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>

#include "chrxUplink.hpp"
#include "buffer.h"
#include "errors.h"
#include "output_formating.h"
#include <unistd.h>

chrxUplink::chrxUplink(Config &config,
                  const string& unique_name,
                  bufferContainer &buffer_container) :
                  KotekanProcess(config, unique_name, buffer_container,
                                 std::bind(&chrxUplink::main_thread, this)) {

    vis_buf = get_buffer("chrx_in_buf");
    register_consumer(vis_buf, unique_name.c_str());
    gate_buf = get_buffer("gate_in_buf");
    register_consumer(gate_buf, unique_name.c_str());
}

chrxUplink::~chrxUplink() {
}

void chrxUplink::apply_config(uint64_t fpga_seq) {
    char hostname[1024];
    string s_port;    

    if (!config.update_needed(fpga_seq))
        return;

    _collection_server_ip = config.get_string(unique_name, "collection_server_ip");
    gethostname(hostname, 1024);
   
    string s_hostname(hostname);
    string lastNum = s_hostname.substr(s_hostname.length() - 2, 2);
    s_port = "410" + lastNum;
    
    _collection_server_port = stoi(s_port);  //config.get_int(unique_name, "collection_server_port");
    _enable_gating = config.get_bool(unique_name, "enable_gating");
}

// TODO make this more robust to network errors.
void chrxUplink::main_thread() {
    apply_config(0);

    int buffer_ID = 0;
    uint8_t * vis_frame = NULL;
    uint8_t * gate_frame = NULL;

    // Connect to server.
    struct sockaddr_in ch_acq_addr;

    int tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_fd == -1) {
        ERROR("Could not create socket, errno: %d", errno);
        return;
    }

    bzero(&ch_acq_addr, sizeof(ch_acq_addr));
    ch_acq_addr.sin_family = AF_INET;
    ch_acq_addr.sin_addr.s_addr = inet_addr(_collection_server_ip.c_str());
    ch_acq_addr.sin_port = htons(_collection_server_port);

    if (connect(tcp_fd, (struct sockaddr *)&ch_acq_addr, sizeof(ch_acq_addr)) == -1) {
        ERROR("Could not connect to collection server, error: %d", errno);
    }
    INFO("Connected to collection server on: %s:%d",
         _collection_server_ip.c_str(),
         _collection_server_port);

    // Wait for, and transmit, full buffers.
    while(!stop_thread) {

        // This call is blocking!
        vis_frame = wait_for_full_frame(vis_buf, unique_name.c_str(), buffer_ID);
        if (vis_frame == NULL) break;

        // INFO("Sending TCP frame to ch_master. frame size: %d", vis_buf->frame_size);

        ssize_t bytes_sent = send(tcp_fd, vis_frame, vis_buf->frame_size, 0);
        if (bytes_sent <= 0) {
            ERROR("Could not send frame to chrx, error: %d", errno);
            break;
        }
        if (bytes_sent != vis_buf->frame_size) {
            ERROR("Could not send all bytes: bytes sent = %d; frame_size = %d",
                    (int)bytes_sent, vis_buf->frame_size);
            break;
        }
        INFO("Finished sending frame to chrx");

        if (_enable_gating) {
            DEBUG("Getting gated buffer");
            gate_frame = wait_for_full_frame(gate_buf, unique_name.c_str(), buffer_ID);
            if (gate_frame == NULL) break;

            DEBUG("Sending gated buffer");
            bytes_sent = send(tcp_fd, gate_frame, gate_buf->frame_size, 0);
            if (bytes_sent <= 0) {
                ERROR("Could not send gated date frame to ch_acq, error: %d", errno);
                break;
            }
            if (bytes_sent != gate_buf->frame_size) {
                ERROR("Could not send all bytes in gated data frame: bytes sent = %d; frame_size = %d",
                        (int)bytes_sent, gate_buf->frame_size);
                break;
            }
            INFO("Finished sending gated data frame to chrx");
            mark_frame_empty(gate_buf, unique_name.c_str(), buffer_ID);
        }

        mark_frame_empty(vis_buf, unique_name.c_str(), buffer_ID);

        buffer_ID = (buffer_ID + 1) % vis_buf->num_frames;
    }
}
