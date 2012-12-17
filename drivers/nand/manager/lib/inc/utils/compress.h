#ifndef __COMPRESS_H__
#define __COMPRESS_H__

static inline int compress(unsigned int inlen,unsigned char * inbuf,unsigned int *outlen,unsigned char *outbuf)
{
	memcpy(outbuf,inbuf,inlen);
	*outlen = inlen;

	return 0;
}

static inline int decompress(unsigned int inlen,unsigned char *inbuf,unsigned int *outlen,unsigned char *outbuf)
{
	memcpy(outbuf,inbuf,inlen);
	*outlen = inlen;
	return 0;
}


#endif
