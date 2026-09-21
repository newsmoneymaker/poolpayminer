/* poolpayminer: see xla_hash.h */
#include "xla_hash.h"
#include "yespower/yespower.h"
#include "k12/KangarooTwelve.h"

void rx_xla_finish_input_hash(void *tempHash64)
{
    yespower_params_t params;
    params.version = YESPOWER_1_0;
    params.N       = 2048;
    params.r       = 8;
    params.pers    = NULL;
    params.perslen = 0;

    yespower_tls((const uint8_t *) tempHash64, 64, &params, (yespower_binary_t *) tempHash64);
    KangarooTwelve((const unsigned char *) tempHash64, 64, (unsigned char *) tempHash64, 32, 0, 0);
}
