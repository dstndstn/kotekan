#ifndef VDIF_STREAM
#define VDIF_STREAM

#include "Config.hpp"
#include "buffer.c"
#include "KotekanProcess.hpp"

class vdifStream : public KotekanProcess {
public:
    vdifStream(Config& config, const string& unique_name,
               bufferContainer &buffer_container);
    virtual ~vdifStream();
    void main_thread();

    virtual void apply_config(uint64_t fpga_seq);

private:
    struct Buffer *buf;

    uint32_t _vdif_port;
    string _vdif_server_ip;

};

#endif