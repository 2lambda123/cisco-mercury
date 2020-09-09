// g++ -Wall driver_os_identifier.cc -o driver_os_identifier ../parser.c -lz

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <zlib.h>
#include <vector>

#include "os_identifier.h"


unsigned char jbuf[] = "{\"fingerprints\":{\"tls\":\"(0303)(00ffc02cc02bc024c023c00ac009c008c030c02fc028c027c014c013c012009d009c003d003c0035002f000a)((0000)(000a00080006001700180019)(000b00020100)(000d000e000c050104010201050304030203)(3374)(00100030002e0268320568322d31360568322d31350568322d313408737064792f332e3106737064792f3308687474702f312e31)(000500050100000000)(0012))\"},\"tls\":{\"client\":{\"version\":\"0303\",\"random\":\"58ec0e8814ec73ee485e09e3cbb4c05779f1c4673ed534335cb9d027f2a7cbac\",\"session_id\":\"a8201677af1768be3750ed52790188168b0fa976e315434f638e81e9724803cd\",\"cipher_suites\":\"00ffc02cc02bc024c023c00ac009c008c030c02fc028c027c014c013c012009d009c003d003c0035002f000a\",\"compression_methods\":\"00\",\"server_name\":\"static.criteo.net\",\"fingerprint\":\"(0303)(00ffc02cc02bc024c023c00ac009c008c030c02fc028c027c014c013c012009d009c003d003c0035002f000a)((0000)(000a00080006001700180019)(000b00020100)(000d000e000c050104010201050304030203)(3374)(00100030002e0268320568322d31360568322d31350568322d313408737064792f332e3106737064792f3308687474702f312e31)(000500050100000000)(0012))\"}},\"src_ip\":\"10.41.32.146\",\"dst_ip\":\"74.119.117.74\",\"protocol\":6,\"src_port\":60034,\"dst_port\":443,\"event_start\":1491865224.241034}";

#define FP_BUFFER_SIZE 512
#define FP_TYPE_BUFFER_SIZE 32
#define SRC_IP_BUFFER_SIZE 64
#define EVENT_START_BUFFER_SIZE 32


int main(int argc, char *argv[]) {
    os_analysis_init("../../resources");

    struct parser d{jbuf, jbuf + sizeof(jbuf)};
    struct mercury_record r{d};

    char fp_buffer[FP_BUFFER_SIZE];
    char fp_type_buffer[FP_TYPE_BUFFER_SIZE];
    char src_ip_buffer[SRC_IP_BUFFER_SIZE];
    char event_start_buffer[EVENT_START_BUFFER_SIZE];

    snprintf(fp_buffer, FP_BUFFER_SIZE, "%.*s", (int)r.fingerprint.length(), r.fingerprint.data);
    snprintf(fp_type_buffer, FP_TYPE_BUFFER_SIZE, "%.*s", (int)r.fp_type.length(), r.fp_type.data);
    snprintf(src_ip_buffer, SRC_IP_BUFFER_SIZE, "%.*s", (int)r.src_ip.length(), r.src_ip.data);
    snprintf(event_start_buffer, EVENT_START_BUFFER_SIZE, "%.*s", (int)r.event_start.length(), r.event_start.data);
//    double event_start = atof(event_start_buffer);

    if (os_fp_types.find(fp_type_buffer) != os_fp_types.end()) {
//        printf("%s\n",fp_type_buffer);
//        printf("%s\n",fp_buffer);
//        printf("%s\n",src_ip_buffer);
//        printf("%f\n\n",event_start);

        update_host_data(fp_type_buffer, fp_buffer, src_ip_buffer);
        update_host_data(fp_type_buffer, fp_buffer, src_ip_buffer);
    }

    classify_all_samples();

    return 0;
}