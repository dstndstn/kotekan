#include "beamform_phase_data.h"

#include <math.h>
#include <time.h>
#include "errors.h"
#include "buffers.h"

#define D2R 0.01745329252 // pi/180
#define TAU 6.28318530718 // 2*pi

beamform_phase_data::beamform_phase_data(char* param_name):gpu_command(param_name)
{

}

beamform_phase_data::~beamform_phase_data()
{
    free(phases[0]);
    free(phases[1]);
}

void beamform_phase_data::build(Config* param_Config, class device_interface &param_Device)
{
    gpu_command::build(param_Config, param_Device);

    cl_int err;

    beamforming_do_not_track = param_Config->beamforming.do_not_track;
    inst_lat = param_Config->beamforming.instrument_lat;
    inst_long = param_Config->beamforming.instrument_long;
    num_elements = param_Config->processing.num_elements;
    num_local_freq = param_Config->processing.num_local_freq;
    fixed_time = param_Config->beamforming.fixed_time;

    ra = param_Config->beamforming.ra;
    dec = param_Config->beamforming.dec;
    feed_positions = param_Config->beamforming.element_positions;

    last_bankID = -1;

    // Create two phase banks
    phases[0] = (float *) malloc(num_elements * sizeof(float));
    phases[1] = (float *) malloc(num_elements * sizeof(float));

    start_beamform_time = time(NULL); // Current time.
}

cl_event beamform_phase_data::execute(int param_bufferID, class device_interface &param_Device, cl_event param_PrecedeEvent)
{
    gpu_command::execute(param_bufferID, param_Device, param_PrecedeEvent);

    cl_event last_event;
    time_t local_beamform_time;
    stream_id_t local_stream_id;
    uint64_t current_seq;
    // TODO Make this a config file option
    // 390625 == 1 second.
    const uint64_t phase_update_period = 390625;

    // Update the phases only every "phase_update_period"
    current_seq = get_fpga_seq_num(param_Device.getInBuf(), param_bufferID);
    int64_t bankID = (current_seq / phase_update_period) % 2;
    if(bankID != last_bankID) {
        get_delays(phases[bankID], local_beamform_time);

        CHECK_CL_ERROR( clEnqueueWriteBuffer(param_Device.getQueue(0),
                                        param_Device.get_device_phases(bankID),
                                        CL_FALSE,
                                        0,
                                        num_elements * sizeof(float),
                                        (cl_float *)phases[bankID],
                                        1,
                                        &param_PrecedeEvent,
                                        &postEvent[param_bufferID]));

        last_bankID = bankID;
        return postEvent[param_bufferID];
    }
    return param_PrecedeEvent;
}

void beamform_phase_data::get_delays(float * phases, time_t beamform_time)
{
    //inverse speed of light in ns/m
    const double one_over_c = 3.3356;
    //offset of initial lst phase in degrees <-------------- PROBABLY WRONG PLEASE CHECK
    const double phi_0 = 280.46;
    //const double phi_0 = 160.64;
    //rate of change of LST with time in s
    const double lst_rate = 360./86164.09054;
    //UNIX timestamp of J2000 epoch time
    const double j2000_unix = 946728000;

////    const double inst_lat = config->beamforming.instrument_lat;
////    const double inst_long = config->beamforming.instrument_long;

    // This accounts for LST difference between J2000 and UNIX_Time in beamform_time.
    // It was verified with Kiyo's python code ch_util.ephemeris.transit_RA(), which
    // should account for both precession and nutation. Needs to be tested here though.
    double precession_offset = (beamform_time - j2000_unix) * 0.012791 / (365 * 24 * 3600);

    //calculate and modulate local sidereal time
    double lst = phi_0 + inst_long + lst_rate*(beamform_time - j2000_unix) - precession_offset;
    lst = fmod(lst, 360.);

    //convert lst to hour angle
    double hour_angle = lst - ra;
    //if(hour_angle < 0){hour_angle += 360.;}

    //get the alt/az based on the above
    double alt = sin(dec*D2R)*sin(inst_lat*D2R)+cos(dec*D2R)*cos(inst_lat*D2R)*cos(hour_angle*D2R);
    alt = asin(alt);
    double az = (sin(dec*D2R) - sin(alt)*sin(inst_lat*D2R))/(cos(alt)*cos(inst_lat*D2R));
    az = acos(az);
    if(sin(hour_angle*D2R) >= 0){az = TAU - az;}

    //project, determine phases for each element
    //return geometric phase that instrument sees, i.e. -phases will be applied in beamformer
    double projection_angle, effective_angle, offset_distance;
    for(int i = 0; i < num_elements; ++i)
    {
        projection_angle = 90*D2R - atan2(feed_positions[2*i+1],feed_positions[2*i]);
        offset_distance  = cos(alt)*sqrt(feed_positions[2*i]*feed_positions[2*i] + feed_positions[2*i+1]*feed_positions[2*i+1]);
        effective_angle  = projection_angle - az;

        //z = (sin(dec*D2R) - sin(alt)*sin(inst_lat*D2R))/(cos(alt)*cos(inst_lat*D2R));
        phases[i] = TAU*cos(effective_angle)*offset_distance*one_over_c;
    }

    INFO("get_delays: Computed delays: tnow = %d, lat = %f, long = %f, RA = %f, DEC = %f, LST = %f, ALT = %f, AZ = %f", (int)time(NULL), inst_lat, inst_long, ra, dec, lst, alt/D2R, az/D2R);

    return;
}

void beamform_phase_data::cleanMe(int param_BufferID)
{
    gpu_command::cleanMe(param_BufferID);
}

void beamform_phase_data::freeMe()
{
    gpu_command::freeMe();
}
