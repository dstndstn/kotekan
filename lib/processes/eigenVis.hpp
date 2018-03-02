#ifndef EIGENVIS_HPP
#define EIGENVIS_HPP

#include "buffer.h"
#include "KotekanProcess.hpp"

/**
 * @class eigenVis
 * @brief Perform eigen factorization of the visibilities
 *
 * This task performs the factorization of the visibility matrix into 
 * ``num_eigenvector`` eigenvectors and eigenvalues and stores them in reserve space
 * in the ``visBuffer``
 *
 * @par Buffers
 * @buffer in_buf The set of buffers coming out the GPU buffers
 *         @buffer_format visBuffer structured
 *         @buffer_metadata visMetadata
 * @buffer out_buf The merged and transformed buffer
 *         @buffer_format visBuffer structured
 *         @buffer_metadata visMetadata
 *
 * @conf  num_elements          Int. The number of elements (i.e. inputs) in the
 *                              correlator data.
 * @conf  block_size            Int. The block size of the packed data.
 * @conf  num_eigenvectors      Int. The number of eigenvectors to be calculated as
 *                              an approximation to the visibilities.
 * @conf  num_diagonals_filled  Int. Number of diagonals to fill with the previouse
 *                              time step's solution prior to factorization. For
 *                              example, setting to 1 will replace the main diagonal
 *                              only. Filled with zero on the first time step.
 *
 * @author Kiyoshi Masui
 */
class eigenVis : public KotekanProcess {

public:
    eigenVis(Config& config,
             const string& unique_name,
             bufferContainer &buffer_container);
    ~eigenVis();
    void apply_config(uint64_t fpga_seq) override;
    void main_thread();
private:
    struct Buffer *input_buffer;
    struct Buffer *output_buffer;

    int32_t num_eigenvectors;
    int32_t num_diagonals_filled;
};

#endif
