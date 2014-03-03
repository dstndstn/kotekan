
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include "file_write.h"
#include "buffers.h"
#include "errors.h"
#include "chrx.h"
#include "output_formating.h"
#include "error_correction.h"

#define NUM_FRAMES_PER_PACKET 4

void push_corr_frame(struct chrx_acq_t *self, const char *new_vis,
                     const uint8_t *new_flag, unsigned int fpga_count,
                     struct timeval time, int32_t link_num, int32_t push,
                     int actual_num_freq )
{
    int n, page;

    n = self->n_freq * self->n_corr;
    page = get_frame_page(self);

    // Record the FPGA counter and the CPU time.

    self->frame[page].timestamp[0].fpga_count = fpga_count;
    self->frame[page].timestamp[0].cpu_s = time.tv_sec;
    self->frame[page].timestamp[0].cpu_us = time.tv_usec;

    shuffle_data_to_frequency_major_output_16_element_with_triangle_conversion_skip_8(actual_num_freq,
                                                                                      (int *)new_vis,
                                                                                      self->frame[page].vis,
                                                                                      link_num );

    // For thread-safety, lock writing to the frame while we increment the frame
    // page counter.
    // Only push once we have the data from all links.
    // TODO if we are missing data for some links, this won't work. 
    if (push) {
        pthread_mutex_lock(&self->frame_lock);
        if (++self->frame_page >= self->n_frame_page)
            self->frame_page = 0;
        pthread_mutex_unlock(&self->frame_lock);

        page = get_frame_page(self);
        // Clear visibility and flag buffers.
        memset((void *)self->vis_sum, 0, n * sizeof(double complex));
        memset((void *)self->frame[page].vis_flag, 0,
               n * sizeof(struct vis_flag_t));

        self->n_fpga_sample = 0;
    }

  return;
}

void file_write_thread(void * arg)
{
    struct file_write_thread_arg * args = (struct file_write_thread_arg *) arg;

    int bufferID = -1;

    int useableBufferIDs[1] = {0};
    int link_id = 0;

    int bad_frames[args->num_links];

    /// HDF5 File setup

    struct chrx_acq_t chrx; 

    chrx_acq_init(&chrx);

    chrx.path_prefix = "results/";

    int n = chrx.n_freq * chrx.n_corr;

    // Initialisations.
    chrx.frame_page = 0;
    n = chrx.n_freq * chrx.n_corr;
    for (int i = 0; i < chrx.n_frame_page; i++) {
       memset((void *)chrx.frame[i].vis, 0, n * sizeof(complex_int_t));
       memset((void *)chrx.frame[i].vis_flag, 0, n * sizeof(struct vis_flag_t));
    }

    chrx.running = 1;

    // Create thread for writing frames to disc.
    pthread_create(&chrx.disc_loop, NULL, disc_thread, &chrx);

    /// END HDF5 File setup

    for (;;) {

        // This call is blocking.
        bufferID = getFullBufferFromList(args->buf, useableBufferIDs, 1);

        // Check if the producer has finished, and we should exit.
        if (bufferID == -1) {
            chrx.running = 0;
            pthread_join(chrx.disc_loop, NULL);

            int ret;
            pthread_exit((void *) &ret);
        }

        // TODO Check that this is valid data.
        uint32_t fpga_seq_number = get_fpga_seq_num(args->buf, bufferID);
        struct timeval frame_start_time = get_first_packet_recv_time(args->buf, bufferID);
        int data_id = get_buffer_data_ID(args->buf, bufferID);

        struct ErrorMatrix * error_matrix = get_error_matrix(args->buf, bufferID);

        link_id = bufferID % args->num_links;

        reorganize_32_to_16_feed_GPU_Correlated_Data(args->actual_num_freq,
                                                     args->actual_num_elements,
                                                     (int *)args->buf->data[bufferID] );

        bad_frames[link_id] = error_matrix->bad_frames;

        int32_t push = 0;
        if (link_id + 1 == args->num_links) {
            push = 1;
            if (args->num_links == 7) {
                INFO("Pushing correlator frame; ID = %d; FPGA_seq_number = %d; num_bad_frames = [ %d %d %d %d %d %d %d ]",
                     data_id, fpga_seq_number, bad_frames[0], bad_frames[1], bad_frames[2],
                     bad_frames[3], bad_frames[4], bad_frames[5], bad_frames[6]);
            }
            if (args->num_links == 8) {
                INFO("Pushing correlator frame; ID = %d; FPGA_seq_number = %d; num_bad_frames = [ %d %d %d %d %d %d %d %d ]",
                     data_id, fpga_seq_number, bad_frames[0], bad_frames[1], bad_frames[2],
                     bad_frames[3], bad_frames[4], bad_frames[5], bad_frames[6], bad_frames[7]);
            }
        }

        push_corr_frame(&chrx, args->buf->data[bufferID], NULL, fpga_seq_number*NUM_FRAMES_PER_PACKET,
                        frame_start_time, link_id, push,
                        args->actual_num_freq);

        release_info_object(args->buf, bufferID);
        markBufferEmpty(args->buf, bufferID);

        useableBufferIDs[0] = (useableBufferIDs[0] + 1) % args->buf->num_buffers;
    }

}
