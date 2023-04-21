/*
 * Copyright (c) 2015 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_KRYPTON_KRYPTON_H_
#define CS_KRYPTON_KRYPTON_H_

#ifdef KR_LOCALS
#include <kr_locals.h>
#endif

typedef struct x509_store_ctx_st X509_STORE_CTX;
typedef struct ssl_st SSL;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_method_st SSL_METHOD;

int SSL_library_init_0(void);
SSL *SSL_new_0(SSL_CTX *ctx);
int SSL_set_fd_0(SSL *ssl, int fd);
int SSL_set_cipher_list_0(SSL *ssl, const char *str);
int SSL_get_fd_0(SSL *ssl);
int SSL_accept_0(SSL *ssl);
int SSL_connect_0(SSL *ssl);
int SSL_read_0(SSL *ssl, void *buf, int num);
int SSL_write_0(SSL *ssl, const void *buf, int num);
int SSL_shutdown_0(SSL *ssl);
void SSL_free_0(SSL *ssl);

#define SSL_ERROR_NONE 0
#define SSL_ERROR_SSL 1
#define SSL_ERROR_WANT_READ 2
#define SSL_ERROR_WANT_WRITE 3
#define SSL_ERROR_SYSCALL 5
#define SSL_ERROR_ZERO_RETURN 6
#define SSL_ERROR_WANT_CONNECT 7
#define SSL_ERROR_WANT_ACCEPT 8
int SSL_get_error_0(const SSL *ssl, int ret);

const SSL_METHOD *TLSv1_2_method_0(void);
const SSL_METHOD *TLSv1_2_server_method_0(void);
const SSL_METHOD *TLSv1_2_client_method_0(void);
const SSL_METHOD *SSLv23_method_0(void);
const SSL_METHOD *SSLv23_server_method_0(void);
const SSL_METHOD *SSLv23_client_method_0(void);

SSL_CTX *SSL_CTX_new_0(const SSL_METHOD *meth);

#define SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER 0x00000002L
#define SSL_CTX_set_mode(ctx, op) SSL_CTX_ctrl_0((ctx), 33, (op), NULL)
long SSL_CTX_ctrl_0(SSL_CTX *, int, long, void *);

int SSL_CTX_set_cipher_list_0(SSL_CTX *ctx, const char *str);

/* for the client */
#define SSL_VERIFY_NONE 0x00
#define SSL_VERIFY_PEER 0x01
#define SSL_VERIFY_FAIL_IF_NO_PEER_CERT 0x02
#define SSL_VERIFY_CLIENT_ONCE 0x04
void SSL_CTX_set_verify_0(SSL_CTX *ctx, int mode,
                        int (*verify_callback)(int, X509_STORE_CTX *));
int SSL_CTX_load_verify_locations_0(SSL_CTX *ctx, const char *CAfile,
                                  const char *CAPath);

/* Krypton-specific. */
int SSL_CTX_kr_set_verify_name(SSL_CTX *ctx, const char *name);

/* for the server */
#define SSL_FILETYPE_PEM 1
int SSL_CTX_use_certificate_chain_file_0(SSL_CTX *ctx, const char *file);
int SSL_CTX_use_certificate_file_0(SSL_CTX *ctx, const char *file, int type);
int SSL_CTX_use_PrivateKey_file_0(SSL_CTX *ctx, const char *file, int type);

void SSL_CTX_free_0(SSL_CTX *);

typedef struct {
  unsigned char block_len;
  unsigned char key_len;
  unsigned char iv_len;
  void *(*new_ctx)();
  void (*setup_enc)(void *ctx, const unsigned char *key);
  void (*setup_dec)(void *ctx, const unsigned char *key);
  void (*encrypt)(void *ctx, const unsigned char *msg, int len, unsigned char *out);
  void (*decrypt)(void *ctx, const unsigned char *msg, int len, unsigned char *out);
  void (*free_ctx)(void *ctx);
} kr_cipher_info;

#endif /* CS_KRYPTON_KRYPTON_H_ */