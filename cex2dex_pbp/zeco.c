#include <pspsdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <openssl/aes.h>

typedef struct {
	u8	buf1[8]; // 0
	u8	buf2[8]; // 8
	u8	buf3[8]; // 0x10
} SomeStructure;

u8 idskey0[16] = {
	0x47, 0x5E, 0x09, 0xF4, 0xA2, 0x37, 0xDA, 0x9B,
	0xEF, 0xFF, 0x3B, 0xC0, 0x77, 0x14, 0x3D, 0x8A
};

typedef struct U64 {
	u32 low;
	u32 high;
} U64;

uint64_t g_fuseid = 0;

u8 key0buf[512];
u8 key1buf[512];
u8 keysbuf[1024];

int CreateSS(SomeStructure *ss) {
	U64 fuse;
	int i;

	memcpy(&fuse, &g_fuseid, 8);

	memset(ss->buf1, 0, 8);
	memset(ss->buf2, 0xFF, 8);

	memcpy(ss->buf3, &fuse.high, 4);
	memcpy(ss->buf3+4, &fuse.low, 4);

	for (i = 0; i < 4; i++) {
		ss->buf1[3-i] = ss->buf3[i];
		ss->buf1[7-i] = ss->buf3[4+i];
	}

	return 0;
}

void P1(u8 *buf) {
	u8 m[0x40]; // sp
	int i, j;
	char rr[5] = { 0x01, 0x0F, 0x36, 0x78, 0x40 }; // sp+0x50
	u8 ss[5]; // sp+0x40

	memset(m, 0, 0x40);
	memcpy(m, buf, 0x3C);

	for (i = 0; i < 0x3C; i++) {
		for (j = 0; j < 5; j++) {
			u8 x;
			u32 y;
			char c;

			x = 0;
			c = rr[j];
			y = m[i];
			
			while (c != 0) {
				if (c & 1) {
					x ^= y;
				}

				y = (y << 1);

				if ((y & 0x0100)) {
					y ^= 0x11D;
				}

				c /= 2;
			}

			ss[j] = x;
		}

		for (j = 0; j < 5; j++) {
			m[i+j] ^= ss[j];
		}
	}

	for (i = 0x3C; i < 0x40; i++) {
		buf[i] = m[i];
		m[i] = 0;
	}
}

void GenerateSigncheck(SomeStructure *ss, int *b, u8 *out) {
	AES_KEY ctx, ctx2; // sp+0x20
	int i, j;
	u8 sg_key1[0x10], sg_key2[0x10]; // sp, sp+0x10

	AES_set_encrypt_key(idskey0, 128, &ctx);
	AES_set_decrypt_key(idskey0, 128, &ctx2);

	for (i = 0; i < 16; i++) {
		sg_key1[i] = sg_key2[i] = ss->buf1[i % 8];
	}

	for (i = 0; i < 3; i++) {
		AES_encrypt(sg_key2, sg_key2, &ctx);
		AES_decrypt(sg_key1, sg_key1, &ctx2);
	}

	AES_set_encrypt_key(sg_key2, 128, &ctx);

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			AES_encrypt(sg_key1, sg_key1, &ctx);
		}

		memcpy(out+(i*16), sg_key1, 0x10);
	}

	memcpy(out+0x30, ss->buf1, 8);
	memcpy(out+0x38, b, 4);

	P1(out);
}

int EncryptRegion(u8 *scheck, u8 *in, u32 size, u8 *out) {
	const int n = 3; // param t0
	int a;
	int i, j;
	u8	k1[0x10], k2[0x10]; // sp, sp+0x110
	u8 *r;
	AES_KEY ctx; // (fp=sp+0x10)
	int v = size / 16;

	a = (size & 0x0F);

	if (a != 0 || (size == 0 && (n & 1))) {
		if ((n & 2) == 0)
			return -1;

		v++;
		r = k1;
		
		for (i = 0; i < a; i++) {
			k1[i] = in[size-a+i];
		}

		k1[i++] = 0x80;

		for (; i < 0x10; i++) {
			k1[i] = 0;
		}

		a = 0x10;
	} else {
		r = (in+(v*16))-0x10;
	}

	AES_set_encrypt_key(scheck, 128, &ctx);

	if ((n & 1)) {
		memset(out, 0, 16);
	} else if (v > 0) {
		AES_encrypt(out, out, &ctx);
	}

	for (i = 0; i < (v-1); i++) {
		for (j = 0; j < 16; j++) {
			out[j] ^= in[i*16+j];
		}

		AES_encrypt(out, out, &ctx);
	}	

	if (v > 0) {
		for (i = 0; i < 0x10; i++) {
			out[i] ^= r[i];
		}
	}

	if (!(n & 2)) {
		return 0;
	}

	memset(k2, 0, 0x10);
	AES_encrypt(k2, k2, &ctx);

	a = a ? 2 : 1;
	
	for (i = 0; i < a; i++) {
		u32 x = ((char)k2[0] < 0);
		
		for (j = 0; j < 15; j++) {
			u32 t = ((k2[j+1] >> 7) | (k2[j] << 1));
			k2[j] = t;
		}

		k2[15] <<= 1;

		if (x == 1) {
			int t = (int)k2[15] ^ 0xFFFFFF87;
			k2[15] = t;
		}
	}

	for (i = 0; i < 0x10; i++) {
		out[i] ^= k2[i];
	}

	AES_encrypt(out, out, &ctx);

	return 0;
}

void zeco(uint64_t fuseid, uint8_t *buffer, uint8_t *crtbuf) {
	int i;
	g_fuseid = fuseid;
	unsigned char buf2[0xA8];
	memcpy(buf2, buffer, sizeof(buf2));

	SomeStructure ss;

	CreateSS(&ss);

	uint8_t buf3[0x10];

	int b=0;
	u32 c[0x40/4];

	AES_KEY ctx;

	GenerateSigncheck(&ss, &b, (void *)c);

	AES_set_encrypt_key((void *)(c+8), 128, &ctx);

	for (i = 0; i < 2; i++) {
		AES_encrypt((void *)(c+4), (void *)(c+4), &ctx);
	}

	for (i = 0; i < 3; i++) {
		AES_encrypt((u8 *)c, (u8 *)c, &ctx);
	}

	EncryptRegion((void *)c, buf2, 0xA8, buf3);

	uint8_t certificate_final[0xB8];

	memcpy(certificate_final, buf2, 0xA8);

	memcpy(certificate_final + 0xA8, buf3, 0x10);

	memcpy(crtbuf, certificate_final, sizeof(certificate_final));
}