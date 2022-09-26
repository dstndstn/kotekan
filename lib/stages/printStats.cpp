#include "printStats.hpp"

#include "Config.hpp"          // for Config
#include "StageFactory.hpp"    // for REGISTER_KOTEKAN_STAGE, StageMakerTemplate
#include "buffer.h"            // for mark_frame_empty, register_consumer, wait_for_full_frame
#include "bufferContainer.hpp" // for bufferContainer
#include "kotekanLogging.hpp"  // for DEBUG
#include "util.h"              // for hex_dump

#include <atomic>      // for atomic_bool
#include <cstdint>     // for int32_t
#include <exception>   // for exception
#include <regex>       // for match_results<>::_Base_type
#include <stdexcept>   // for runtime_error
#include <vector>      // for vector
#include <visUtil.hpp> // for frameID, modulo


using kotekan::bufferContainer;
using kotekan::Config;
using kotekan::Stage;

REGISTER_KOTEKAN_STAGE(printStats);

STAGE_CONSTRUCTOR(printStats) {

    // Register as consumer on buffer
    in_buf = get_buffer("in_buf");
    register_consumer(in_buf, unique_name.c_str());

    // Get some configuration options
    datatype = config.get<std::string>(unique_name, "datatype");
    if (!((datatype == "float") || (datatype == "int32")))
        throw std::runtime_error("StatsPrinter: unknown 'datatype' " + datatype + " (allowed: 'float', 'int32')");
}

printStats::~printStats() {}

void printStats::main_thread() {

    frameID frame_id(in_buf);

    while (!stop_thread) {

        uint8_t* frame = wait_for_full_frame(in_buf, unique_name.c_str(), frame_id);
        if (frame == nullptr)
            break;

        DEBUG("printStats: Got buffer {:s}[{:d}]", in_buf->buffer_name, frame_id);

        float avg = 0;
        float var = 0;
        int i,n;
        if (datatype == "float") {
            float* data = (float*)frame;
            n = in_buf->frame_size / sizeof(float);
            for (i=0; i<n; i++)
                avg += data[i];
            avg /= n;
            for (i=0; i<n; i++)
                var += (data[i] - avg)*(data[i] - avg);
            var /= (n-1);
        } else if (datatype == "int32") {
            int32_t* data = (int32_t*)frame;
            n = in_buf->frame_size / sizeof(int32_t);
            for (i=0; i<n; i++)
                avg += data[i];
            avg /= n;
            for (i=0; i<n; i++)
                var += (data[i] - avg)*(data[i] - avg);
            var /= (n-1);
        }
        INFO("StatsPrinter: frame {:d}: mean {:f}, std {:f}",
             frame_id, avg, sqrt(var));

        mark_frame_empty(in_buf, unique_name.c_str(), frame_id);
        frame_id++;
    }
}
