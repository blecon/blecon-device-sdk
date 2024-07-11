/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include "blecon_nrf5_crypto.h"
#include "blecon_nrf5_aead_cipher.h"
#include "blecon/blecon_error.h"

#include "string.h"
#include "nrf_crypto.h"
#include "app_error.h"

// Validate nRF5 SDK Config
#if NRF_CRYPTO_ENABLED != 1
#error "NRF_CRYPTO_ENABLED must be set to 1 in sdk_config.h"
#endif
#if !NRF_CRYPTO_BACKEND_NRF_HW_RNG_ENABLED || NRF_CRYPTO_BACKEND_NRF_HW_RNG_MBEDTLS_CTR_DRBG_ENABLED
#error "NRF_CRYPTO_BACKEND_NRF_HW_RNG_ENABLED must be set to 1 and NRF_CRYPTO_BACKEND_NRF_HW_RNG_MBEDTLS_CTR_DRBG_ENABLED must be set to 0 in sdk_config.h"
#endif

#if !NRF_CRYPTO_RNG_STATIC_MEMORY_BUFFERS_ENABLED || !NRF_CRYPTO_RNG_AUTO_INIT_ENABLED
#error "NRF_CRYPTO_RNG_STATIC_MEMORY_BUFFERS_ENABLED and NRF_CRYPTO_RNG_AUTO_INIT_ENABLED must be set to 1 in sdk_config.h"
#endif

#if !NRF_CRYPTO_BACKEND_OBERON_ENABLED \
    || !NRF_CRYPTO_BACKEND_OBERON_CHACHA_POLY_ENABLED \
    || !NRF_CRYPTO_BACKEND_OBERON_ECC_CURVE25519_ENABLED \
    || !NRF_CRYPTO_BACKEND_OBERON_HASH_SHA256_ENABLED \
    || !NRF_CRYPTO_BACKEND_OBERON_HMAC_SHA256_ENABLED
#error "NRF_CRYPTO_BACKEND_OBERON_ENABLED, NRF_CRYPTO_BACKEND_OBERON_CHACHA_POLY_ENABLED, NRF_CRYPTO_BACKEND_OBERON_ECC_CURVE25519_ENABLED, NRF_CRYPTO_BACKEND_OBERON_HASH_SHA256_ENABLED, and NRF_CRYPTO_BACKEND_OBERON_HMAC_SHA256_ENABLED must be set to 1 in sdk_config.h"
#endif

static void blecon_nrf5_crypto_setup(struct blecon_crypto_t* crypto);
static void blecon_nrf5_crypto_get_random(struct blecon_crypto_t* crypto, uint8_t* random, size_t sz);
static uint32_t blecon_nrf5_crypto_get_random_integer(struct blecon_crypto_t* crypto, uint32_t max);
static bool blecon_nrf5_crypto_generate_x25519_keypair(struct blecon_crypto_t* crypto, uint8_t* private_key, uint8_t* public_key);
static bool blecon_nrf5_crypto_x25519_dh(struct blecon_crypto_t* crypto, const uint8_t* private_key, const uint8_t* peer_public_key, uint8_t* shared_secret);
static bool blecon_nrf5_crypto_hkdf_sha256(struct blecon_crypto_t* crypto, const uint8_t* secret, size_t secret_sz, const uint8_t* salt, size_t salt_sz, uint8_t* output, size_t output_sz);
static struct blecon_crypto_aead_cipher_t* blecon_nrf5_crypto_aead_cipher_new(struct blecon_crypto_t* crypto, const uint8_t* key, bool encrypt_ndecrypt);
static void blecon_nrf5_crypto_aead_cipher_enc_auth(struct blecon_crypto_aead_cipher_t* cipher, 
            const uint8_t* nonce, size_t nonce_sz, 
            const uint8_t* plaintext, size_t plaintext_sz, 
            const uint8_t* additional_data, size_t additional_data_sz,
            uint8_t* ciphertext_mac);
static bool blecon_nrf5_crypto_aead_cipher_dec_auth(struct blecon_crypto_aead_cipher_t* cipher, 
            const uint8_t* nonce, size_t nonce_sz, 
            const uint8_t* ciphertext_mac, size_t ciphertext_mac_sz,
            const uint8_t* additional_data, size_t additional_data_sz,
            uint8_t* plaintext);
static void blecon_nrf5_crypto_aead_cipher_free(struct blecon_crypto_aead_cipher_t* cipher);
static void blecon_nrf5_crypto_sha256_compute(struct blecon_crypto_t* crypto, const uint8_t* in, size_t in_sz, uint8_t* hash);
static bool blecon_nrf5_crypto_sha256_verify(struct blecon_crypto_t* crypto, const uint8_t* in, size_t in_sz, const uint8_t* hash);

static struct blecon_crypto_t _crypto;

struct blecon_crypto_t* blecon_nrf5_crypto_init(void) {
    static const struct blecon_crypto_fn_t crypto_fn = {
        .setup = blecon_nrf5_crypto_setup,
        .get_random = blecon_nrf5_crypto_get_random,
        .get_random_integer = blecon_nrf5_crypto_get_random_integer,
        .generate_x25519_keypair = blecon_nrf5_crypto_generate_x25519_keypair,
        .x25519_dh = blecon_nrf5_crypto_x25519_dh,
        .hkdf_sha256 = blecon_nrf5_crypto_hkdf_sha256,
        .aead_cipher_new = blecon_nrf5_crypto_aead_cipher_new,
        .aead_cipher_enc_auth = blecon_nrf5_crypto_aead_cipher_enc_auth,
        .aead_cipher_dec_auth = blecon_nrf5_crypto_aead_cipher_dec_auth,
        .aead_cipher_free = blecon_nrf5_crypto_aead_cipher_free,
        .sha256_compute = blecon_nrf5_crypto_sha256_compute,
        .sha256_verify = blecon_nrf5_crypto_sha256_verify
    };

    blecon_crypto_init(&_crypto, &crypto_fn);

    return &_crypto;
}

struct blecon_crypto_t* blecon_nrf5_crypto_internal_get(void) {
    return &_crypto;
}

void blecon_nrf5_crypto_setup(struct blecon_crypto_t* crypto) {
    ret_code_t ret_val = nrf_crypto_init();
    blecon_assert(ret_val == NRF_SUCCESS);

    // The RNG module is not explicitly initialized in this module, as
    // NRF_CRYPTO_RNG_AUTO_INIT_ENABLED and NRF_CRYPTO_RNG_STATIC_MEMORY_BUFFERS_ENABLED
    // are enabled in sdk_config.h.
}

void blecon_nrf5_crypto_get_random(struct blecon_crypto_t* crypto, uint8_t* random, size_t sz) {
    ret_code_t ret_val = nrf_crypto_rng_vector_generate(random, sz);
    blecon_assert(ret_val == NRF_SUCCESS);
}

uint32_t blecon_nrf5_crypto_get_random_integer(struct blecon_crypto_t* crypto, uint32_t max) {
    uint32_t result = 0;
    blecon_nrf5_crypto_get_random(crypto, (uint8_t*)&result, sizeof(uint32_t));
    result %= max;
    return result;
}

bool blecon_nrf5_crypto_generate_x25519_keypair(struct blecon_crypto_t* crypto, uint8_t* private_key, uint8_t* public_key) {
    bool success = false;
    nrf_crypto_ecc_key_pair_generate_context_t ctx = {0};
    nrf_crypto_ecc_private_key_t nrf_private_key = {0};
    nrf_crypto_ecc_public_key_t nrf_public_key = {0};

    ret_code_t ret_val = nrf_crypto_ecc_key_pair_generate(&ctx, &g_nrf_crypto_ecc_curve25519_curve_info, &nrf_private_key, &nrf_public_key);
    if(ret_val != NRF_SUCCESS) { goto fail; }

    size_t key_sz = BLECON_X25519_PRIVATE_KEY_SZ;
    ret_val = nrf_crypto_ecc_private_key_to_raw(&nrf_private_key, private_key, &key_sz);
    if(ret_val != NRF_SUCCESS) { goto fail; }
    if(key_sz != BLECON_X25519_PRIVATE_KEY_SZ) { goto fail; }

    key_sz = BLECON_X25519_PUBLIC_KEY_SZ;
    ret_val = nrf_crypto_ecc_public_key_to_raw(&nrf_public_key, public_key, &key_sz);
    if(ret_val != NRF_SUCCESS) { goto fail; }
    if(key_sz != BLECON_X25519_PUBLIC_KEY_SZ) { goto fail; }
    
    success = true;

fail:
    nrf_crypto_ecc_private_key_free(&nrf_private_key);
    nrf_crypto_ecc_public_key_free(&nrf_public_key);
    return success;
}

bool blecon_nrf5_crypto_x25519_dh(struct blecon_crypto_t* crypto, const uint8_t* private_key, const uint8_t* peer_public_key, uint8_t* shared_secret) {
    bool success = false;
    nrf_crypto_ecc_private_key_t nrf_private_key = {0};
    nrf_crypto_ecc_public_key_t nrf_public_key = {0};

    // Load public key
    ret_code_t ret_val = nrf_crypto_ecc_public_key_from_raw(&g_nrf_crypto_ecc_curve25519_curve_info,
                                                  &nrf_public_key,
                                                  peer_public_key, 
                                                  BLECON_X25519_PUBLIC_KEY_SZ);
    if(ret_val != NRF_SUCCESS) { goto fail; }

    // Load private key
    ret_val = nrf_crypto_ecc_private_key_from_raw(&g_nrf_crypto_ecc_curve25519_curve_info,
                                                   &nrf_private_key,
                                                   private_key,
                                                   BLECON_X25519_PRIVATE_KEY_SZ);
    if(ret_val != NRF_SUCCESS) { goto fail; }

    // Perform ECDH exchange
    size_t shared_secret_size = BLECON_X25519_OUT_SECRET_SZ;
    ret_val = nrf_crypto_ecdh_compute(NULL,
                                       &nrf_private_key,
                                       &nrf_public_key,
                                       shared_secret,
                                       &shared_secret_size);
    if(ret_val != NRF_SUCCESS) { goto fail; }

    if(shared_secret_size != BLECON_X25519_OUT_SECRET_SZ) {
        goto fail;
    }

    success = true;
fail:
    nrf_crypto_ecc_private_key_free(&nrf_private_key);
    nrf_crypto_ecc_public_key_free(&nrf_public_key);
    return success;
}

bool blecon_nrf5_crypto_hkdf_sha256(struct blecon_crypto_t* crypto, const uint8_t* secret, size_t secret_sz, const uint8_t* salt, size_t salt_sz, uint8_t* output, size_t output_sz) {
    nrf_crypto_hmac_context_t hkdf_context;
    size_t actual_output_sz = output_sz;
    ret_code_t ret_val = nrf_crypto_hkdf_calculate(&hkdf_context,
                                         &g_nrf_crypto_hmac_sha256_info,
                                         output,
                                         &actual_output_sz,
                                         secret,
                                         secret_sz,
                                         salt,
                                         salt_sz,
                                         NULL,
                                         0,
                                         NRF_CRYPTO_HKDF_EXTRACT_AND_EXPAND);

    return (ret_val == NRF_SUCCESS) && (actual_output_sz == output_sz);
}

struct blecon_crypto_aead_cipher_t* blecon_nrf5_crypto_aead_cipher_new(struct blecon_crypto_t* crypto, const uint8_t* key, bool encrypt_ndecrypt) {
    struct blecon_nrf5_aead_cipher_t* nrf5_cipher = BLECON_ALLOC(sizeof(struct blecon_nrf5_aead_cipher_t));
    if(nrf5_cipher == NULL) {
        blecon_fatal_error();
    }
    blecon_crypto_aead_cipher_init(&nrf5_cipher->cipher, crypto);
	ret_code_t ret_val = nrf_crypto_aead_init(&nrf5_cipher->ctx,
                                   &g_nrf_crypto_chacha_poly_256_info,
                                   (uint8_t*)key); // TODO why does nrf_crypto drop the const qualifier?
    blecon_assert(ret_val == NRF_SUCCESS);
    return &nrf5_cipher->cipher;
}

void blecon_nrf5_crypto_aead_cipher_enc_auth(struct blecon_crypto_aead_cipher_t* cipher, 
            const uint8_t* nonce, size_t nonce_sz, 
            const uint8_t* plaintext, size_t plaintext_sz, 
            const uint8_t* additional_data, size_t additional_data_sz,
            uint8_t* ciphertext_mac) {
                struct blecon_nrf5_aead_cipher_t* nrf5_cipher = (struct blecon_nrf5_aead_cipher_t*) cipher;
                ret_code_t ret_val = nrf_crypto_aead_crypt(&nrf5_cipher->ctx,
                                    NRF_CRYPTO_ENCRYPT,
                                    (uint8_t*)nonce,
                                    nonce_sz,
                                    (uint8_t*)additional_data,
                                    additional_data_sz,
                                    (uint8_t*)plaintext,
                                    plaintext_sz,
                                    ciphertext_mac,
                                    ciphertext_mac + plaintext_sz,
                                    BLECON_CHACHA20_POLY1305_TAG_SZ);
                blecon_assert(ret_val == NRF_SUCCESS);
            }

bool blecon_nrf5_crypto_aead_cipher_dec_auth(struct blecon_crypto_aead_cipher_t* cipher, 
            const uint8_t* nonce, size_t nonce_sz, 
            const uint8_t* ciphertext_mac, size_t ciphertext_mac_sz,
            const uint8_t* additional_data, size_t additional_data_sz,
            uint8_t* plaintext) {
                struct blecon_nrf5_aead_cipher_t* nrf5_cipher = (struct blecon_nrf5_aead_cipher_t*) cipher;

                if(ciphertext_mac_sz < BLECON_CHACHA20_POLY1305_TAG_SZ) {
                    return false;
                }

                ret_code_t ret_val = nrf_crypto_aead_crypt(&nrf5_cipher->ctx,
                                    NRF_CRYPTO_DECRYPT,
                                    (uint8_t*)nonce,
                                    nonce_sz,
                                    (uint8_t*)additional_data,
                                    additional_data_sz,
                                    (uint8_t*)ciphertext_mac,
                                    ciphertext_mac_sz - BLECON_CHACHA20_POLY1305_TAG_SZ,
                                    plaintext,
                                    (uint8_t*)(ciphertext_mac + ciphertext_mac_sz - BLECON_CHACHA20_POLY1305_TAG_SZ),
                                    BLECON_CHACHA20_POLY1305_TAG_SZ);
                return ret_val == NRF_SUCCESS;
            }

void blecon_nrf5_crypto_aead_cipher_free(struct blecon_crypto_aead_cipher_t* cipher) {
    struct blecon_nrf5_aead_cipher_t* nrf5_cipher = (struct blecon_nrf5_aead_cipher_t*) cipher;

    nrf_crypto_aead_uninit(&nrf5_cipher->ctx);

    BLECON_FREE(nrf5_cipher);
}

void blecon_nrf5_crypto_sha256_compute(struct blecon_crypto_t* crypto, const uint8_t* in, size_t in_sz, uint8_t* hash)
{
    nrf_crypto_hash_context_t ctx;
    size_t hash_sz = BLECON_SHA256_HASH_SZ;
    ret_code_t ret_val = nrf_crypto_hash_calculate(&ctx, &g_nrf_crypto_hash_sha256_info, in, in_sz, hash, &hash_sz);
    blecon_assert(ret_val == NRF_SUCCESS);
    blecon_assert(hash_sz == BLECON_SHA256_HASH_SZ);
}

bool blecon_nrf5_crypto_sha256_verify(struct blecon_crypto_t* crypto, const uint8_t* in, size_t in_sz, const uint8_t* hash) {
    uint8_t computed_hash[BLECON_SHA256_HASH_SZ];
    blecon_nrf5_crypto_sha256_compute(crypto, in, in_sz, computed_hash);
    return memcmp(hash, computed_hash, BLECON_SHA256_HASH_SZ) == 0;
}
