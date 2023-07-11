/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */
#include "stdlib.h"
#include "string.h"
#include "assert.h"

#include "blecon_zephyr_crypto.h"
#include "blecon/blecon_defs.h"
#include "blecon/blecon_error.h"
#include "blecon_zephyr_aead_cipher.h"

#include "zephyr/random/rand32.h"

#include "psa/crypto.h"

static void blecon_zephyr_crypto_setup(struct blecon_crypto_t* crypto);
static void blecon_zephyr_crypto_get_random(struct blecon_crypto_t* crypto, uint8_t* random, size_t sz);
static uint32_t blecon_zephyr_crypto_get_random_integer(struct blecon_crypto_t* crypto, uint32_t max);
static bool blecon_zephyr_crypto_generate_x25519_keypair(struct blecon_crypto_t* crypto, uint8_t* private_key, uint8_t* public_key);
static bool blecon_zephyr_crypto_x25519_dh(struct blecon_crypto_t* crypto, const uint8_t* private_key, const uint8_t* peer_public_key, uint8_t* shared_secret);
static bool blecon_zephyr_crypto_hkdf_sha256(struct blecon_crypto_t* crypto, const uint8_t* secret, size_t secret_sz, const uint8_t* salt, size_t salt_sz, uint8_t* output, size_t output_sz);
static struct blecon_crypto_aead_cipher_t* blecon_zephyr_crypto_aead_cipher_new(struct blecon_crypto_t* crypto, const uint8_t* key, bool encrypt_ndecrypt);
static bool blecon_zephyr_crypto_aead_cipher_enc_auth(struct blecon_crypto_aead_cipher_t* cipher, 
            const uint8_t* nonce, size_t nonce_sz, 
            const uint8_t* plaintext, size_t plaintext_sz, 
            const uint8_t* additional_data, size_t additional_data_sz,
            uint8_t* ciphertext_mac);
static bool blecon_zephyr_crypto_aead_cipher_dec_auth(struct blecon_crypto_aead_cipher_t* cipher, 
            const uint8_t* nonce, size_t nonce_sz, 
            const uint8_t* ciphertext_mac, size_t ciphertext_mac_sz,
            const uint8_t* additional_data, size_t additional_data_sz,
            uint8_t* plaintext);
static void blecon_zephyr_crypto_aead_cipher_free(struct blecon_crypto_aead_cipher_t* cipher);
static void blecon_zephyr_crypto_sha256_compute(struct blecon_crypto_t* crypto, const uint8_t* in, size_t in_sz, uint8_t* hash);
static bool blecon_zephyr_crypto_sha256_verify(struct blecon_crypto_t* crypto, const uint8_t* in, size_t in_sz, const uint8_t* hash);

struct blecon_crypto_t* blecon_zephyr_crypto_init(void) {
    static const struct blecon_crypto_fn_t crypto_fn = {
        .setup = blecon_zephyr_crypto_setup,
        .get_random = blecon_zephyr_crypto_get_random,
        .get_random_integer = blecon_zephyr_crypto_get_random_integer,
        .generate_x25519_keypair = blecon_zephyr_crypto_generate_x25519_keypair,
        .x25519_dh = blecon_zephyr_crypto_x25519_dh,
        .hkdf_sha256 = blecon_zephyr_crypto_hkdf_sha256,
        .aead_cipher_new = blecon_zephyr_crypto_aead_cipher_new,
        .aead_cipher_enc_auth = blecon_zephyr_crypto_aead_cipher_enc_auth,
        .aead_cipher_dec_auth = blecon_zephyr_crypto_aead_cipher_dec_auth,
        .aead_cipher_free = blecon_zephyr_crypto_aead_cipher_free,
        .sha256_compute = blecon_zephyr_crypto_sha256_compute,
        .sha256_verify = blecon_zephyr_crypto_sha256_verify
    };

    struct blecon_crypto_t* crypto = BLECON_ALLOC(sizeof(struct blecon_crypto_t));
    if(crypto == NULL) {
        blecon_fatal_error();
    }

    blecon_crypto_init(crypto, &crypto_fn);

    return crypto;
}

void blecon_zephyr_crypto_setup(struct blecon_crypto_t* crypto) {
    psa_status_t status = psa_crypto_init();
    blecon_assert( status == PSA_SUCCESS );
}

void blecon_zephyr_crypto_get_random(struct blecon_crypto_t* crypto, uint8_t* random, size_t sz) {
   if(sys_csrand_get(random, sz) != 0) {
        blecon_fatal_error();
    }
}

uint32_t blecon_zephyr_crypto_get_random_integer(struct blecon_crypto_t* crypto, uint32_t max) {
    uint32_t result = 0;
    blecon_zephyr_crypto_get_random(crypto, (uint8_t*)&result, sizeof(uint32_t));
    result %= max;
    return result;
}

bool blecon_zephyr_crypto_generate_x25519_keypair(struct blecon_crypto_t* crypto, uint8_t* private_key, uint8_t* public_key) {
    bool success = false;

    // Set key parameters (X25519)
    psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_DERIVE | PSA_KEY_USAGE_EXPORT);
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_VOLATILE);
	psa_set_key_algorithm(&key_attributes, PSA_ALG_ECDH);
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_MONTGOMERY));
	psa_set_key_bits(&key_attributes, 255);

    psa_key_handle_t key_handle = PSA_KEY_HANDLE_INIT;
    psa_status_t status = psa_generate_key(&key_attributes, &key_handle);
    if( status != PSA_SUCCESS ) { goto fail; }

    size_t private_key_sz = 0;
    status = psa_export_key(key_handle, private_key, BLECON_X25519_PRIVATE_KEY_SZ, &private_key_sz);
    if( status != PSA_SUCCESS ) { goto fail; }

    blecon_assert(private_key_sz == BLECON_X25519_PRIVATE_KEY_SZ);

    size_t public_key_sz = 0;
    status = psa_export_public_key(key_handle, public_key, BLECON_X25519_PUBLIC_KEY_SZ, &public_key_sz);
    if( status != PSA_SUCCESS ) { goto fail; }

    blecon_assert(public_key_sz == BLECON_X25519_PUBLIC_KEY_SZ);

    success = true;

fail:
    status = psa_destroy_key(key_handle); // Ok even if key_handle is set to 0
    blecon_assert( status == PSA_SUCCESS );

    return success;
}

bool blecon_zephyr_crypto_x25519_dh(struct blecon_crypto_t* crypto, const uint8_t* private_key, const uint8_t* peer_public_key, uint8_t* shared_secret) {
    bool success = false;
    psa_key_handle_t key_handle = PSA_KEY_HANDLE_INIT;
    
    // Set key parameters for private key (X25519)
    psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_DERIVE);
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_VOLATILE);
	psa_set_key_algorithm(&key_attributes, PSA_ALG_ECDH);
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_MONTGOMERY));
	psa_set_key_bits(&key_attributes, 255);

    // Import private key
    psa_status_t status = psa_import_key(&key_attributes, private_key, BLECON_X25519_PRIVATE_KEY_SZ, &key_handle);
    if( status != PSA_SUCCESS ) { goto fail; }
    
    size_t output_sz = 0;
    status = psa_raw_key_agreement(PSA_ALG_ECDH, key_handle, peer_public_key, BLECON_X25519_PUBLIC_KEY_SZ, shared_secret, BLECON_X25519_OUT_SECRET_SZ, &output_sz);
    if( status != PSA_SUCCESS ) { goto fail; }

    blecon_assert(output_sz == BLECON_X25519_OUT_SECRET_SZ);

    success = true;

fail:
    // Destroy private key
    status = psa_destroy_key(key_handle); // Ok even if key_handle is set to 0
    blecon_assert( status == PSA_SUCCESS );

    return success;
}

bool blecon_zephyr_crypto_hkdf_sha256(struct blecon_crypto_t* crypto, const uint8_t* secret, size_t secret_sz, const uint8_t* salt, size_t salt_sz, uint8_t* output, size_t output_sz) {
    bool success = false;
    psa_key_handle_t input_key_handle = PSA_KEY_HANDLE_INIT;
    psa_key_handle_t output_key_handle = PSA_KEY_HANDLE_INIT;
    psa_key_derivation_operation_t operation = PSA_KEY_DERIVATION_OPERATION_INIT;
    
    // Set key parameters for input secret
    psa_key_attributes_t input_key_attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&input_key_attributes, PSA_KEY_USAGE_DERIVE);
	psa_set_key_lifetime(&input_key_attributes, PSA_KEY_LIFETIME_VOLATILE);
	psa_set_key_algorithm(&input_key_attributes, PSA_ALG_HKDF(PSA_ALG_SHA_256));
	psa_set_key_type(&input_key_attributes, PSA_KEY_TYPE_DERIVE);
	psa_set_key_bits(&input_key_attributes, secret_sz * 8);

    // Import secret
    psa_status_t status = psa_import_key(&input_key_attributes, secret, secret_sz, &input_key_handle);
    if( status != PSA_SUCCESS ) { goto fail; }
    
    // Set-up key derivation
    status = psa_key_derivation_setup(&operation, PSA_ALG_HKDF(PSA_ALG_SHA_256));
    if( status != PSA_SUCCESS ) { goto fail; }

    // Input salt
    status = psa_key_derivation_input_bytes(&operation, PSA_KEY_DERIVATION_INPUT_SALT, salt, salt_sz);
    if( status != PSA_SUCCESS ) { goto fail; }

    // Input key
    status = psa_key_derivation_input_key(&operation, PSA_KEY_DERIVATION_INPUT_SECRET, input_key_handle);
    if( status != PSA_SUCCESS ) { goto fail; }

    // Input additional info (empty)
    status = psa_key_derivation_input_bytes(&operation, PSA_KEY_DERIVATION_INPUT_INFO, NULL, 0);
    if( status != PSA_SUCCESS ) { goto fail; }

    // Set key parameters for output secret
    psa_key_attributes_t output_key_attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&output_key_attributes, PSA_KEY_USAGE_EXPORT);
	psa_set_key_lifetime(&output_key_attributes, PSA_KEY_LIFETIME_VOLATILE);
	psa_set_key_type(&output_key_attributes, PSA_KEY_TYPE_RAW_DATA);
	psa_set_key_bits(&output_key_attributes, output_sz * 8);

    // Store output key
    status = psa_key_derivation_output_key(&output_key_attributes, &operation, &output_key_handle);
    if( status != PSA_SUCCESS ) { goto fail; }

    // Export key
    size_t output_key_sz = 0;
    status = psa_export_key(output_key_handle, output, output_sz, &output_key_sz);
    if( status != PSA_SUCCESS ) { goto fail; }

    blecon_assert(output_key_sz == output_sz);

    success = true;

fail:
    // Abort operation
    psa_key_derivation_abort(&operation);

    // Destroy secrets
    status = psa_destroy_key(input_key_handle); // Ok even if key_handle is set to 0
    blecon_assert( status == PSA_SUCCESS );
    status = psa_destroy_key(output_key_handle); // Ok even if key_handle is set to 0
    blecon_assert( status == PSA_SUCCESS );

    return success;
}

struct blecon_crypto_aead_cipher_t* blecon_zephyr_crypto_aead_cipher_new(struct blecon_crypto_t* crypto, const uint8_t* key, bool encrypt_ndecrypt) {
    struct blecon_zephyr_aead_cipher_t* zephyr_cipher = malloc(sizeof(struct blecon_zephyr_aead_cipher_t));
    assert(zephyr_cipher != NULL);
    blecon_crypto_aead_cipher_init(&zephyr_cipher->cipher, crypto);

    // Import key
    zephyr_cipher->key_handle = PSA_KEY_HANDLE_INIT;
    
    // Set key parameters for private key (ChaChaPoly)
    psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&key_attributes, encrypt_ndecrypt ? PSA_KEY_USAGE_ENCRYPT : PSA_KEY_USAGE_DECRYPT);
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_VOLATILE);
	psa_set_key_algorithm(&key_attributes, PSA_ALG_CHACHA20_POLY1305);
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_CHACHA20);
	psa_set_key_bits(&key_attributes, 256);

    // Import key
    psa_status_t status = psa_import_key(&key_attributes, key, BLECON_CHACHA20_POLY1305_SECRET_SZ, &zephyr_cipher->key_handle);
    blecon_assert( status == PSA_SUCCESS );

    return &zephyr_cipher->cipher;
}

bool blecon_zephyr_crypto_aead_cipher_enc_auth(struct blecon_crypto_aead_cipher_t* cipher, 
            const uint8_t* nonce, size_t nonce_sz, 
            const uint8_t* plaintext, size_t plaintext_sz, 
            const uint8_t* additional_data, size_t additional_data_sz,
            uint8_t* ciphertext_mac) {
    struct blecon_zephyr_aead_cipher_t* zephyr_cipher = (struct blecon_zephyr_aead_cipher_t*)cipher;
    
    bool success = false;
    
    // Perform AEAD Encryption
    size_t ciphertext_mac_sz = 0;
    psa_status_t status = psa_aead_encrypt(zephyr_cipher->key_handle, PSA_ALG_CHACHA20_POLY1305, 
        nonce, nonce_sz, additional_data, additional_data_sz, plaintext, plaintext_sz, 
        ciphertext_mac, plaintext_sz + BLECON_CHACHA20_POLY1305_TAG_SZ, &ciphertext_mac_sz);
    if( status != PSA_SUCCESS ) { goto fail; }
    blecon_assert( ciphertext_mac_sz == plaintext_sz + BLECON_CHACHA20_POLY1305_TAG_SZ );

    success = true;

fail:
    return success;
}

bool blecon_zephyr_crypto_aead_cipher_dec_auth(struct blecon_crypto_aead_cipher_t* cipher, 
            const uint8_t* nonce, size_t nonce_sz, 
            const uint8_t* ciphertext_mac, size_t ciphertext_mac_sz,
            const uint8_t* additional_data, size_t additional_data_sz,
            uint8_t* plaintext) {
    struct blecon_zephyr_aead_cipher_t* zephyr_cipher = (struct blecon_zephyr_aead_cipher_t*)cipher;
    
    bool success = false;

    if(ciphertext_mac_sz < BLECON_CHACHA20_POLY1305_TAG_SZ) {
        return false;
    }
    
    // Perform AEAD Encryption
    size_t plaintext_sz = 0;
    psa_status_t status = psa_aead_decrypt(zephyr_cipher->key_handle, PSA_ALG_CHACHA20_POLY1305, 
        nonce, nonce_sz, additional_data, additional_data_sz, ciphertext_mac, ciphertext_mac_sz, 
        plaintext, ciphertext_mac_sz - BLECON_CHACHA20_POLY1305_TAG_SZ, &plaintext_sz);
    if( status != PSA_SUCCESS ) { goto fail; }
    blecon_assert( plaintext_sz == ciphertext_mac_sz - BLECON_CHACHA20_POLY1305_TAG_SZ );

    success = true;

fail:
    return success;
}

void blecon_zephyr_crypto_aead_cipher_free(struct blecon_crypto_aead_cipher_t* cipher) {
    struct blecon_zephyr_aead_cipher_t* zephyr_cipher = (struct blecon_zephyr_aead_cipher_t*)cipher;
    
    // Destroy secret
    psa_status_t status = psa_destroy_key(zephyr_cipher->key_handle); // Ok even if key_handle is set to 0
    blecon_assert( status == PSA_SUCCESS );

    free(zephyr_cipher);
}

void blecon_zephyr_crypto_sha256_compute(struct blecon_crypto_t* crypto, const uint8_t* in, size_t in_sz, uint8_t* hash) {
    size_t out_sz = 0;
    psa_status_t status = psa_hash_compute(PSA_ALG_SHA_256, in, in_sz, hash, BLECON_SHA256_HASH_SZ, &out_sz);
    blecon_assert( status == PSA_SUCCESS );
    blecon_assert( out_sz == BLECON_SHA256_HASH_SZ );
}

bool blecon_zephyr_crypto_sha256_verify(struct blecon_crypto_t* crypto, const uint8_t* in, size_t in_sz, const uint8_t* hash) {
    psa_status_t status = psa_hash_compare(PSA_ALG_SHA_256, in, in_sz, hash, BLECON_SHA256_HASH_SZ);
    return status == PSA_SUCCESS;
}
