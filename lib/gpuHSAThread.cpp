#include "gpuHSAThread.hpp"
#include "unistd.h"
#include "vdif_functions.h"
#include "fpga_header_functions.h"
#include "KotekanProcess.hpp"

#include <iostream>
#include <sys/time.h>

using namespace std;

double e_time(void){
    static struct timeval now;
    gettimeofday(&now, NULL);
    return (double)(now.tv_sec  + now.tv_usec/1000000.0);
}

gpuHSAThread::gpuHSAThread(Config& config_, bufferContainer &host_buffers_,
        uint32_t gpu_id_):
    KotekanProcess(config_, std::bind(&gpuHSAThread::main_thread, this)),
    host_buffers(host_buffers_),
    gpu_id(gpu_id_)
{
    apply_config(0);

    final_signals.resize(_gpu_buffer_depth);

    device = new gpuHSADeviceInterface(config, gpu_id);
    factory = new gpuHSACommandFactory(config, *device, host_buffers);
}

void gpuHSAThread::apply_config(uint64_t fpga_seq) {
    (void)fpga_seq;
    _gpu_buffer_depth = config.get_int("/gpu/buffer_depth");
}

gpuHSAThread::~gpuHSAThread() {
    delete factory;
    delete device;
}

void gpuHSAThread::main_thread()
{

    vector<gpuHSAcommand *> &commands = factory->get_commands();

    // Start with the first GPU frame;
    int gpu_frame_id = 0;
    bool first_run = true;

    sleep(5);

    for(;;) {

        // Wait for all the required preconditions
        // This is things like waiting for the input buffer to have data
        // and for there to be free space in the output buffers.
        for (int i = 0; i < commands.size(); ++i) {
            commands[i]->wait_on_precondition(gpu_frame_id);
        }

        // We make sure we aren't using a gpu frame that's currently in-flight.
        final_signals[gpu_frame_id].wait_for_free_slot();

        hsa_signal_t signal;
        signal.handle = 0;
        INFO("Adding commands to GPU[%d][%d] queues", gpu_id, gpu_frame_id);

        for (int i = 0; i < commands.size(); i++) {
            // Feed the last signal into the next operation
            signal = commands[i]->execute(gpu_frame_id, 0, signal);
        }
        final_signals[gpu_frame_id].set_signal(signal);

        if (first_run) {
            results_thread_handle = std::thread(&gpuHSAThread::results_thread, std::ref(*this));

            // Requires Linux, this could possibly be made more general someday.
            // TODO Move to config
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            for (int j = 4; j < 12; j++)
                CPU_SET(j, &cpuset);
            pthread_setaffinity_np(results_thread_handle.native_handle(),
                                    sizeof(cpu_set_t), &cpuset);
            first_run = false;
        }

        gpu_frame_id = (gpu_frame_id + 1) % _gpu_buffer_depth;
    }
    // TODO Make the exiting process actually work here.
    results_thread_handle.join();
}

void gpuHSAThread::results_thread() {

    vector<gpuHSAcommand *> &commands = factory->get_commands();

    // Start with the first GPU frame;
    int gpu_frame_id = 0;

    for(;;) {
        // Wait for a signal to be completed
        INFO("Waiting for signal for gpu[%d], frame %d, time: %f", gpu_id, gpu_frame_id, e_time());
        final_signals[gpu_frame_id].wait_for_signal();
        INFO("Got final signal for gpu[%d], frame %d, time: %f", gpu_id, gpu_frame_id, e_time());

        for (int i = 0; i < commands.size(); ++i) {
            commands[i]->finalize_frame(gpu_frame_id);
        }
        INFO("Finished finalizing frames for gpu[%d][%d]", gpu_id, gpu_frame_id);

        final_signals[gpu_frame_id].reset();

        gpu_frame_id = (gpu_frame_id + 1) % _gpu_buffer_depth;
    }
}
