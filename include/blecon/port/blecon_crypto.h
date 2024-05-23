/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "stdint.h"
#include "stddef.h"
#include "blecon/blecon_defs.h"
#include "blecon/blecon_error.h"
#include "blecon/blecon_memory.h"

// Security
struct blecon_crypto_t;
struct blecon_crypto_aead_cipher_t;

struct blecon_crypto_fn_t {
    void (*setup)(struct blecon_crypto_t* crypto);
    void (*get_random)(struct blecon_crypto_t* crypto, uint8_t* random, size_t sz);
    uint32_t (*get_random_integer)(struct blecon_crypto_t* crypto, uint32_t max);
    bool (*generate_x25519_keypair)(struct blecon_crypto_t* crypto, uint8_t* private_key, uint8_t* public_key);
    bool (*x25519_dh)(struct blecon_crypto_t* crypto, const uint8_t* private_key, const uint8_t* peer_public_key, uint8_t* shared_secret);
    bool (*hkdf_sha256)(struct blecon_crypto_t* crypto, const uint8_t* secret, size_t secret_sz, const uint8_t* salt, size_t salt_sz, uint8_t* output, size_t output_sz);
    struct blecon_crypto_aead_cipher_t* (*aead_cipher_new)(struct blecon_crypto_t* crypto, const uint8_t* key, bool encrypt_ndecrypt);
    void (*aead_cipher_enc_auth)(struct blecon_crypto_aead_cipher_t* cipher, 
                const uint8_t* nonce, size_t nonce_sz, 
                const uint8_t* plaintext, size_t plaintext_sz, 
                const uint8_t* additional_data, size_t additional_data_sz,
                uint8_t* ciphertext_mac);
    bool (*aead_cipher_dec_auth)(struct blecon_crypto_aead_cipher_t* cipher, 
                const uint8_t* nonce, size_t nonce_sz, 
                const uint8_t* ciphertext_mac, size_t ciphertext_mac_sz,
                const uint8_t* additional_data, size_t additional_data_sz,
                uint8_t* plaintext);
    void (*aead_cipher_free)(struct blecon_crypto_aead_cipher_t* cipher);
    void (*sha256_compute)(struct blecon_crypto_t* crypto, const uint8_t* in, size_t in_sz, uint8_t* hash);
    bool (*sha256_verify)(struct blecon_crypto_t* crypto, const uint8_t* in, size_t in_sz, const uint8_t* hash);
};

struct blecon_crypto_t {
    const struct blecon_crypto_fn_t* fns;
};

struct blecon_crypto_aead_cipher_t {
    struct blecon_crypto_t* crypto;
};

static inline void blecon_crypto_init(struct blecon_crypto_t* crypto, const struct blecon_crypto_fn_t* fns) {
    crypto->fns = fns;
}

static inline void blecon_crypto_setup(struct blecon_crypto_t* crypto) {
    crypto->fns->setup(crypto);
}

static inline bool blecon_crypto_generate_x25519_keypair(struct blecon_crypto_t* crypto, uint8_t* private_key, uint8_t* public_key) {
    return crypto->fns->generate_x25519_keypair(crypto, private_key, public_key);
}

static inline void blecon_crypto_get_random(struct blecon_crypto_t* crypto, uint8_t* random, size_t sz) {
    return crypto->fns->get_random(crypto, random, sz);
}

static inline uint32_t blecon_crypto_get_random_integer(struct blecon_crypto_t* crypto, uint32_t max) {
    return crypto->fns->get_random_integer(crypto, max);
}

static inline bool blecon_crypto_x25519_dh(struct blecon_crypto_t* crypto, const uint8_t* private_key, const uint8_t* peer_public_key, uint8_t* shared_secret) {
    return crypto->fns->x25519_dh(crypto, private_key, peer_public_key, shared_secret);
}

static inline bool blecon_crypto_hkdf_sha256(struct blecon_crypto_t* crypto, const uint8_t* secret, size_t secret_sz, const uint8_t* salt, size_t salt_sz, uint8_t* output, size_t output_sz) {
    return crypto->fns->hkdf_sha256(crypto, secret, secret_sz, salt, salt_sz, output, output_sz);
}

static inline void blecon_crypto_aead_cipher_init(struct blecon_crypto_aead_cipher_t* cipher, struct blecon_crypto_t* crypto) {
    cipher->crypto = crypto;
}

static inline struct blecon_crypto_aead_cipher_t* blecon_crypto_aead_cipher_new(struct blecon_crypto_t* crypto, const uint8_t* key, bool encrypt_ndecrypt) {
    return crypto->fns->aead_cipher_new(crypto, key, encrypt_ndecrypt);
}

static inline void blecon_crypto_aead_cipher_enc_auth(struct blecon_crypto_aead_cipher_t* cipher, 
            const uint8_t* nonce, size_t nonce_sz, 
            const uint8_t* plaintext, size_t plaintext_sz, 
            const uint8_t* additional_data, size_t additional_data_sz,
            uint8_t* ciphertext_mac) {
                return cipher->crypto->fns->aead_cipher_enc_auth(cipher, nonce, nonce_sz, plaintext, plaintext_sz, 
                    additional_data, additional_data_sz,
                    ciphertext_mac);
            }

static inline bool blecon_crypto_aead_cipher_dec_auth(struct blecon_crypto_aead_cipher_t* cipher, 
            const uint8_t* nonce, size_t nonce_sz, 
            const uint8_t* ciphertext_mac, size_t ciphertext_mac_sz,
            const uint8_t* additional_data, size_t additional_data_sz,
            uint8_t* plaintext) {
                return cipher->crypto->fns->aead_cipher_dec_auth(cipher, nonce, nonce_sz, ciphertext_mac, ciphertext_mac_sz, 
                    additional_data, additional_data_sz,
                    plaintext);
            }

static inline void blecon_crypto_aead_cipher_free(struct blecon_crypto_aead_cipher_t* cipher) {
    cipher->crypto->fns->aead_cipher_free(cipher);
}

static inline void blecon_crypto_sha256_compute(struct blecon_crypto_t* crypto, const uint8_t* in, size_t in_sz, uint8_t* hash) {
    crypto->fns->sha256_compute(crypto, in, in_sz, hash);
}

static inline bool blecon_crypto_sha256_verify(struct blecon_crypto_t* crypto, const uint8_t* in, size_t in_sz, const uint8_t* hash) {
    return crypto->fns->sha256_verify(crypto, in, in_sz, hash);
}

#ifdef __cplusplus
}
#endif
