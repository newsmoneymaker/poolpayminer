#include "verus.h"
#include "crypto/verus_hash.h"

void verushash2_2_init()
{
	CVerusHashV2::init();
}

void verushash2_2(const unsigned char *data, size_t len, unsigned char *out)
{
	CVerusHashV2 hasher(SOLUTION_VERUSHHASH_V2_2);
	hasher.Write(data, len);
	hasher.Finalize2b(out);
}
