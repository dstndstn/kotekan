#include "testDataGen.hpp"
#include <random>
#include "errors.h"
#include <unistd.h>
#include <sys/time.h>

testDataGen::testDataGen(Config& config, const string& unique_name,
                         bufferContainer &buffer_container) :
    KotekanProcess(config, unique_name,
                   buffer_container, std::bind(&testDataGen::main_thread, this)) {

    buf = get_buffer("network_out_buf");
    register_producer(buf, unique_name.c_str());
    type = config.get_string(unique_name, "type");
    if (type == "const")
        value = config.get_int(unique_name, "value");
    assert(type == "const" || type == "random");
}

testDataGen::~testDataGen() {

}

void testDataGen::apply_config(uint64_t fpga_seq) {

}

void testDataGen::main_thread() {

    int buf_id = 0;
    int data_id = 0;
    uint64_t seq_num = 0;
    bool finished_seeding_consant = false;
    static struct timeval now;

    for (;;) {
        wait_for_empty_buffer(buf, unique_name.c_str(), buf_id);

        set_data_ID(buf, buf_id, data_id);
        set_fpga_seq_num(buf, buf_id, seq_num);
        // TODO This should be dynamic/config controlled.
        set_stream_ID(buf, buf_id, 0);

        gettimeofday(&now, NULL);
        set_first_packet_recv_time(buf, buf_id, now);

        //std::random_device rd;
        //std::mt19937 gen(rd());
        //std::uniform_int_distribution<> dis(0, 255);
	srand(42);
	unsigned char temp_output;
        for (int j = 0; j < buf->buffer_size; ++j) {
            if (type == "const") {
                if (finished_seeding_consant) break;
                buf->data[buf_id][j] = value;
            } else if (type == "random") {

	      unsigned char new_real;
	      unsigned char new_imaginary;
	      new_real = rand()%16;
	      new_imaginary = rand()%16;
	      temp_output = ((new_real<<4) & 0xF0) + (new_imaginary & 0x0F);
	      buf->data[buf_id][j] = temp_output;
            }
        }
        usleep(83000);
        INFO("Generated a %s test data set in %s[%d]", type.c_str(), buf->buffer_name, buf_id);

        mark_buffer_full(buf, unique_name.c_str(), buf_id);

        buf_id = (buf_id + 1) % buf->num_buffers;
        data_id++;
        seq_num += 32768;
        if (buf_id == 0) finished_seeding_consant = true;
    }
    mark_producer_done(buf, 0);

}

