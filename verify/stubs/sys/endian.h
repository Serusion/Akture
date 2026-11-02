/*
 * <sys/endian.h> for Frama-C, whose libc does not have it.  Same
 * functions as on NetBSD and FreeBSD, typed on bytes so that the WP
 * typed memory model applies, with their contracts.
 */

#ifndef _VERIFY_SYS_ENDIAN_H_
#define _VERIFY_SYS_ENDIAN_H_

#include <stdint.h>

/*@
  requires \valid_read(p + (0 .. 3));
  assigns \nothing;
*/
static inline uint32_t
le32dec(const uint8_t *p)
{

	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
	    (uint32_t)p[3] << 24;
}

/*@
  requires \valid(p + (0 .. 3));
  assigns p[0 .. 3];
*/
static inline void
le32enc(uint8_t *p, uint32_t v)
{

	p[0] = (uint8_t)(v & 0xff);
	p[1] = (uint8_t)((v >> 8) & 0xff);
	p[2] = (uint8_t)((v >> 16) & 0xff);
	p[3] = (uint8_t)(v >> 24);
}

/*@
  requires \valid_read(p + (0 .. 1));
  assigns \nothing;
*/
static inline uint16_t
be16dec(const uint8_t *p)
{

	return (uint16_t)((uint16_t)p[0] << 8 | p[1]);
}

/*@
  requires \valid(p + (0 .. 1));
  assigns p[0 .. 1];
*/
static inline void
be16enc(uint8_t *p, uint16_t v)
{

	p[0] = (uint8_t)(v >> 8);
	p[1] = (uint8_t)(v & 0xff);
}

#endif /* !_VERIFY_SYS_ENDIAN_H_ */
