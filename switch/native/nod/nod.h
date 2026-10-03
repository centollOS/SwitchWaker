/* The part of nod's C API (https://github.com/encounter/nod, v2) that Aurora's DVD library uses,
 * for plain GameCube disc images only. nod itself is Rust, and Rust has no target for libnx, so
 * the native port's Switch build reads the disc with nod_gcn.cpp instead (switch/native/nod).
 * Declarations follow nod 2.0.0-alpha.12's nod.h, the version Aurora 3227d76 pins; only the
 * functions and types Aurora 3227d76 calls are here. */
#ifndef NOD_H
#define NOD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NOD_PARTITION_KIND_DATA 0
#define NOD_PARTITION_KIND_UPDATE 1
#define NOD_PARTITION_KIND_CHANNEL 2
#define NOD_FST_STOP UINT32_MAX

typedef enum NodResult {
  NOD_RESULT_OK,
  NOD_RESULT_ERR_IO,
  NOD_RESULT_ERR_FORMAT,
  NOD_RESULT_ERR_NOT_FOUND,
  NOD_RESULT_ERR_INVALID_HANDLE,
  NOD_RESULT_ERR_OTHER,
} NodResult;

typedef enum NodPartitionEncryption {
  NOD_PARTITION_ENCRYPTION_ORIGINAL,
  NOD_PARTITION_ENCRYPTION_FORCE_ENCRYPTED,
  NOD_PARTITION_ENCRYPTION_FORCE_DECRYPTED,
  NOD_PARTITION_ENCRYPTION_FORCE_DECRYPTED_NO_HASHES,
} NodPartitionEncryption;

typedef enum NodNodeKind {
  NOD_NODE_KIND_FILE,
  NOD_NODE_KIND_DIRECTORY,
} NodNodeKind;

typedef struct NodHandle NodHandle;

typedef struct NodDiscOptions {
  enum NodPartitionEncryption partition_encryption;
  uint32_t preloader_threads;
} NodDiscOptions;

typedef int64_t (*NodDiscStreamReadAtCallback)(void* user_data, uint64_t offset, void* out, size_t len);
typedef int64_t (*NodDiscStreamLenCallback)(void* user_data);
typedef void (*NodDiscStreamCloseCallback)(void* user_data);

typedef struct NodDiscStream {
  void* user_data;
  NodDiscStreamReadAtCallback read_at;
  NodDiscStreamLenCallback stream_len;
  NodDiscStreamCloseCallback close;
} NodDiscStream;

typedef struct NodPartitionOptions {
  bool validate_hashes;
} NodPartitionOptions;

typedef struct NodDiscHeader {
  char game_id[6];
  uint8_t disc_num;
  uint8_t disc_version;
  uint8_t audio_streaming;
  uint8_t audio_stream_buf_size;
  uint8_t _pad1[14];
  uint8_t wii_magic[4];
  uint8_t gcn_magic[4];
  char game_title[64];
  uint8_t no_partition_hashes;
  uint8_t no_partition_encryption;
  uint8_t _pad2[926];
} NodDiscHeader;

typedef struct NodBlob {
  const uint8_t* data;
  size_t size;
} NodBlob;

typedef struct NodPartitionMeta {
  struct NodBlob raw_boot;
  struct NodBlob raw_bi2;
  struct NodBlob raw_apploader;
  struct NodBlob raw_dol;
  struct NodBlob raw_fst;
  struct NodBlob raw_ticket;
  struct NodBlob raw_tmd;
  struct NodBlob raw_cert_chain;
  struct NodBlob raw_h3_table;
} NodPartitionMeta;

typedef uint32_t (*NodFstCallback)(uint32_t, enum NodNodeKind, const char*, uint32_t, void*);

#ifdef __cplusplus
extern "C" {
#endif

const char* nod_error_message(void);

/* Opens a disc from a caller-provided stream (only plain GameCube .iso images here). On success
 * the handle owns the stream and calls its close callback when freed. */
enum NodResult nod_disc_open_stream(const struct NodDiscStream* stream, const struct NodDiscOptions* options,
                                    struct NodHandle** out);

/* The GameCube disc has one partition, the whole disc: kind NOD_PARTITION_KIND_DATA. */
enum NodResult nod_disc_open_partition_kind(struct NodHandle* disc, uint32_t kind,
                                            const struct NodPartitionOptions* options, struct NodHandle** out);

/* Opens the file at FST index `index` of a partition for reading. */
enum NodResult nod_partition_open_file(struct NodHandle* partition, uint32_t index, struct NodHandle** out);

void nod_free(struct NodHandle* handle);

/* Reads from the current position of a disc or file handle; returns the bytes read, -1 on error. */
int64_t nod_read(struct NodHandle* handle, uint8_t* buf, size_t len);

/* Moves the position of a disc or file handle (whence: SEEK_SET, SEEK_CUR, SEEK_END); returns the
 * new position, -1 on error. */
int64_t nod_seek(struct NodHandle* handle, int64_t offset, int32_t whence);

enum NodResult nod_disc_header(const struct NodHandle* disc, struct NodDiscHeader* out);

/* Pointers into the partition's copies of boot.bin, bi2.bin, the apploader, main.dol and fst.bin;
 * valid while the partition handle is. */
enum NodResult nod_partition_meta(const struct NodHandle* partition, struct NodPartitionMeta* out);

/* Calls `callback` for every FST entry from index 1 (the root is not reported): index, kind, name,
 * and the size (files) or the index past the last child (directories). It returns the next index
 * to visit, or NOD_FST_STOP. */
void nod_partition_iterate_fst(const struct NodHandle* partition, NodFstCallback callback, void* user_data);

#ifdef __cplusplus
}
#endif

#endif /* NOD_H */
