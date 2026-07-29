#include "os.h"
#include <mp.h>
#include <libsec.h>

uchar key[] = "Jefe";
uchar data[] = "what do ya want for nothing?";

void
main(void)
{
	int i;
	uchar hash[MD5dlen];

	hmac_md5(data, strlen((char*)data), key, 4, hash, nil);
	for(i=0; i<MD5dlen; i++)
		HOSTED_API(print)("%2.2x", hash[i]);
	HOSTED_API(print)("\n");
	HOSTED_API(print)("750c783e6ab0b503eaa86e310a5db738\n");
}
