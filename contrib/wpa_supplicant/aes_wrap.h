/*
 * AES-based functions
 *
 * - AES Key Wrap Algorithm (128-bit KEK) (RFC3394)
 * - One-Key CBC MAC (OMAC1) hash with AES-128
 * - AES-128 CTR mode encryption
 * - AES-128 EAX mode encryption/decryption
 * - AES-128 CBC
 *
 * Copyright (c) 2003-2005, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#ifndef AES_WRAP_H
#define AES_WRAP_H

int aes_wrap(const u8 *kek, int n, const u8 *plain, u8 *cipher);
int aes_unwrap(const u8 *kek, int n, const u8 *cipher, u8 *plain);
int omac1_aes_128(const u8 *key, const u8 *data, size_t data_len, u8 *mac);
int aes_128_encrypt_block(const u8 *key, const u8 *in, u8 *out);
int aes_128_ctr_encrypt(const u8 *key, const u8 *nonce,
			u8 *data, size_t data_len);
int aes_128_eax_encrypt(const u8 *key, const u8 *nonce, size_t nonce_len,
			const u8 *hdr, size_t hdr_len,
			u8 *data, size_t data_len, u8 *tag);
int aes_128_eax_decrypt(const u8 *key, const u8 *nonce, size_t nonce_len,
			const u8 *hdr, size_t hdr_len,
			u8 *data, size_t data_len, const u8 *tag);
int aes_128_cbc_encrypt(const u8 *key, const u8 *iv, u8 *data,
			size_t data_len);
int aes_128_cbc_decrypt(const u8 *key, const u8 *iv, u8 *data,
			size_t data_len);

#endif /* AES_WRAP_H */
