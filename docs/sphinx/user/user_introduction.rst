************
Introduction
************

So you like the idea of **Kotekan** and want to get started using it? That's what this page is for!

(This assumes you'e got it installed already. If not, see :ref:`compiling`.)

Running kotekan
---------------

**Using systemd (full install)**

To start kotekan

.. code:: bash

    sudo systemctl start kotekan

To stop kotekan

.. code:: bash

    sudo systemctl stop kotekan

**To run in debug mode, run from `ch_gpu/build/kotekan/`**

.. code:: bash

    sudo ./kotekan -c <config_file>.yaml


When installed kotekan's config files are located at /etc/kotekan/

If running with no options then kotekan just stats a rest server, and waits for someone to send it a config in json format on port **12048**.



A Simple Example
----------------
To get things started, let's try running a simple dummy kotekan which generates random numbers, then prints the mean and standard deviation of each frame.

You'll want to create a YAML file containing the relevant config.

We'll make use of two stages, ``testDataGenFloat`` and ``printStats``.

The ``testDataGenFloat`` stage is meant for testing other stages and can generate
constant, random, or other test-pattern data.  We will configure it using
the following YAML:

.. code:: yaml

    gen_data:
      kotekan_stage: testDataGenFloat
      network_out_buf: input_buf
      type: random
      seed: 42
      samples_per_data_set: 1024
      rand_max: 100

where ``gen_data`` is the name we've given to this stage of the
kotekan graph, and it uses the stage named ``testDataGenFloat``.  It
will write its output into the buffer named ``input_buf`` (which we'll
define in a moment), and it is drawing random numbers uniformly in
``[0, 100)``, with a dataset size of 1024 samples.

Now, since ``testDataGenFloat`` is meant for testing purposes, it also
writes some metadata into a metadata buffer, so a minimal working
example is this YAML (available in `config/examples/minimal.yaml`:

.. code:: yaml

    log_level: debug
    cpu_affinity: [1]

    buffer_depth: 10
    sample_length: 1024
    sizeof_float: 4

    gen_data:
      kotekan_stage: testDataGenFloat
      network_out_buf: input_buf
      type: random
      seed: 42
      samples_per_data_set: sample_length
      rand_max: 100

    input_buf:
      kotekan_buffer: standard
      metadata_pool: meta_pool
      num_frames: buffer_depth
      frame_size: sample_length * sizeof_float

    meta_pool:
      kotekan_metadata_pool: chimeMetadata
      num_metadata_objects: 5 * buffer_depth
