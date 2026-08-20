#define _GNU_SOURCE

#include "erl_nif.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <openssl/evp.h>
#include <stdatomic.h>
#include <stdint.h>
#include <unistd.h>

#ifndef LLI_MAX_BINARY_COPY_BYTES
#define LLI_MAX_BINARY_COPY_BYTES (64U * 1024U * 1024U)
#endif

/*
 * This mirrors the prefix of ERTS' internal Binary structure in
 * erts/emulator/beam/erl_binary.h. It is deliberately local to this debugging
 * NIF: Binary is not part of the public NIF ABI.
 */
typedef struct lli_binary_header
{
  uintptr_t flags;
  uintptr_t apparent_size;
  uintptr_t refc;
  intptr_t orig_size;
} LLI_BINARY_HEADER;

_Static_assert(sizeof(LLI_BINARY_HEADER) == 4 * sizeof(uintptr_t),
               "unexpected Binary header layout");

enum lli_binary_flags
{
  LLI_BIN_FLAG_MAGIC = 1U << 0,
  LLI_BIN_FLAG_DRV = 1U << 1,
  LLI_BIN_FLAG_WRITABLE = 1U << 2,
  LLI_BIN_FLAG_ACTIVE_WRITER = 1U << 3,
  LLI_BIN_FLAG_MASK = (1U << 4) - 1
};

typedef struct my_evp_mac_st
{
  void *prov;
  int name_id;
  char *type_name;
  const char *description;

  int refcnt;
  void *lock;

  void *newctx;
  void *dupctx;
  void *freectx;
  void *init;
  void *update;
  void *final;
  void *gettable_params;
  void *gettable_ctx_params;
  void *settable_ctx_params;
  void *get_params;
  void *get_ctx_params;
  void *set_ctx_params;
} MY_EVP_MAC;

ERL_NIF_TERM ATOM_OK;
ERL_NIF_TERM ATOM_ERROR;
ERL_NIF_TERM ATOM_SHOW;
ERL_NIF_TERM ATOM_PUT;
ERL_NIF_TERM ATOM_GET;
ERL_NIF_TERM ATOM_CRASHME_PUT;
ERL_NIF_TERM ATOM_CRASHME_GET;
ERL_NIF_TERM ATOM_BAD_ADDRESS;
ERL_NIF_TERM ATOM_BAD_HEADER;
ERL_NIF_TERM ATOM_CHANGED_DURING_READ;
ERL_NIF_TERM ATOM_NOT_SUPPORTED;
ERL_NIF_TERM ATOM_SIZE_MISMATCH;
ERL_NIF_TERM ATOM_TOO_LARGE;
ERL_NIF_TERM ATOM_UNSUPPORTED_BINARY;

#define INIT_ATOMS                                                            \
  ATOM(ATOM_OK, ok)                                                           \
  ATOM(ATOM_ERROR, error)                                                     \
  ATOM(ATOM_SHOW, show)                                                       \
  ATOM(ATOM_PUT, put)                                                         \
  ATOM(ATOM_GET, get)                                                         \
  ATOM(ATOM_CRASHME_GET, crashme_get)                                         \
  ATOM(ATOM_CRASHME_PUT, crashme_put)                                         \
  ATOM(ATOM_BAD_ADDRESS, bad_address)                                         \
  ATOM(ATOM_BAD_HEADER, bad_header)                                           \
  ATOM(ATOM_CHANGED_DURING_READ, changed_during_read)                         \
  ATOM(ATOM_NOT_SUPPORTED, not_supported)                                     \
  ATOM(ATOM_SIZE_MISMATCH, size_mismatch)                                     \
  ATOM(ATOM_TOO_LARGE, too_large)                                             \
  ATOM(ATOM_UNSUPPORTED_BINARY, unsupported_binary)

static void
init_atoms(ErlNifEnv *env)
{
  // init atoms in use.
#define ATOM(name, val)                                                       \
  {                                                                           \
    (name) = enif_make_atom(env, #val);                                       \
  }
  INIT_ATOMS
#undef ATOM
}

static ERL_NIF_TERM
mk_atom(ErlNifEnv *env, const char *atom)
{
  ERL_NIF_TERM ret;

  if (!enif_make_existing_atom(env, atom, &ret, ERL_NIF_LATIN1))
    {
      return enif_make_atom(env, atom);
    }

  return ret;
}

static ERL_NIF_TERM
make_error(ErlNifEnv *env, ERL_NIF_TERM reason)
{
  return enif_make_tuple2(env, ATOM_ERROR, reason);
}

#if defined(__linux__)
/*
 * Reading through /proc/self/mem makes an invalid/unmapped pointer return an
 * error instead of delivering SIGSEGV to the VM. It does not solve address
 * reuse or lifetime races; the Erlang wrapper suspends and rechecks the owner
 * to reduce those races.
 */
static int
safe_read_self(void *destination, uintptr_t source, size_t size)
{
  int fd;
  unsigned char *dst = destination;

  if ((off_t)source < 0)
    {
      return 0;
    }

  fd = open("/proc/self/mem", O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    {
      return 0;
    }

  while (size != 0)
    {
      ssize_t copied = pread(fd, dst, size, (off_t)source);

      if (copied < 0 && errno == EINTR)
        {
          continue;
        }
      if (copied <= 0)
        {
          close(fd);
          return 0;
        }

      dst += (size_t)copied;
      source += (uintptr_t)copied;
      size -= (size_t)copied;
    }

  close(fd);
  return 1;
}
#endif

static int
on_load(ErlNifEnv *env, void **priv_data, ERL_NIF_TERM loadinfo)
{
  init_atoms(env);
  return 0;
}

ERL_NIF_TERM
mac_refcnt(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
  ERL_NIF_TERM ret = ATOM_ERROR;
  // refcnt +1
  EVP_MAC *mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
  ERL_NIF_TERM cmd = argv[0];
  if (enif_is_identical(ATOM_SHOW, cmd))
    {
      ret = enif_make_int(env, ((MY_EVP_MAC *)mac)->refcnt);
    }
  else if (enif_is_identical(ATOM_GET, cmd))
    {
      // refcnt +1
      mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
      ret = enif_make_int(env, ((MY_EVP_MAC *)mac)->refcnt);
    }
  else if (enif_is_identical(ATOM_PUT, cmd))
    {
      // refcnt -1
      EVP_MAC_free(mac);
      ret = ATOM_OK;
    }
  else if (enif_is_identical(ATOM_CRASHME_PUT, cmd))
    {
      unsigned int cnt = 0;
      do
        {
          EVP_MAC_free(mac);
        }
      while (1);
    }
  else if (enif_is_identical(ATOM_CRASHME_GET, cmd))
    {
      unsigned int cnt = 0;
      do
        {
          mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
          cnt++;
          if (0 == (cnt % 100000000))
            {
              printf("mac refcnt: %d\n", ((MY_EVP_MAC *)mac)->refcnt);
            }
        }
      while (1);
    }
  // refcnt -1
  EVP_MAC_free(mac);
  return ret;
}

/*
 * Copy the orig_bytes allocation from an ERTS Binary* returned as BinaryId by
 * erlang:process_info(Pid, binary).
 *
 * This is intentionally unsafe and version-specific. The integer address does
 * not retain the Binary. Callers should use lli:process_binary/2, which
 * synchronously suspends the owner and verifies {BinaryId, BinarySize, _}
 * immediately before invoking this function.
 */
static ERL_NIF_TERM
unsafe_copy_binary(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
#if defined(__linux__)
  ErlNifUInt64 address_arg;
  ErlNifUInt64 size_arg;
  uintptr_t address;
  size_t expected_size;
  LLI_BINARY_HEADER before;
  LLI_BINARY_HEADER after;
  ERL_NIF_TERM result;
  unsigned char *destination;

  if (argc != 2 || !enif_get_uint64(env, argv[0], &address_arg)
      || !enif_get_uint64(env, argv[1], &size_arg) || address_arg == 0
      || size_arg > SIZE_MAX)
    {
      return enif_make_badarg(env);
    }

  address = (uintptr_t)address_arg;
  expected_size = (size_t)size_arg;

  if (address > UINTPTR_MAX - sizeof(LLI_BINARY_HEADER))
    {
      return make_error(env, ATOM_BAD_ADDRESS);
    }
  if (expected_size > LLI_MAX_BINARY_COPY_BYTES)
    {
      return make_error(env, ATOM_TOO_LARGE);
    }
  if (!safe_read_self(&before, address, sizeof(before)))
    {
      return make_error(env, ATOM_BAD_ADDRESS);
    }
  if ((before.flags & ~(uintptr_t)LLI_BIN_FLAG_MASK) != 0
      || before.orig_size < 0)
    {
      return make_error(env, ATOM_BAD_HEADER);
    }
  if ((before.flags & LLI_BIN_FLAG_MAGIC) != 0)
    {
      /* Magic/resource binaries can keep their payload somewhere else. */
      return make_error(env, ATOM_UNSUPPORTED_BINARY);
    }
  if ((uintptr_t)before.orig_size != (uintptr_t)expected_size)
    {
      return make_error(env, ATOM_SIZE_MISMATCH);
    }
  if ((before.flags & LLI_BIN_FLAG_WRITABLE) != 0
      && before.apparent_size > (uintptr_t)before.orig_size)
    {
      return make_error(env, ATOM_BAD_HEADER);
    }

  destination = enif_make_new_binary(env, expected_size, &result);
  if (expected_size != 0
      && !safe_read_self(
          destination, address + sizeof(LLI_BINARY_HEADER), expected_size))
    {
      return make_error(env, ATOM_BAD_ADDRESS);
    }

  /* Detect common free/reuse/change cases. The refcount may legitimately
   * change, so it is intentionally excluded from this comparison. */
  if (!safe_read_self(&after, address, sizeof(after)))
    {
      return make_error(env, ATOM_CHANGED_DURING_READ);
    }
  if (before.flags != after.flags
      || before.apparent_size != after.apparent_size
      || before.orig_size != after.orig_size)
    {
      return make_error(env, ATOM_CHANGED_DURING_READ);
    }

  return enif_make_tuple2(env, ATOM_OK, result);
#else
  (void)argc;
  (void)argv;
  return make_error(env, ATOM_NOT_SUPPORTED);
#endif
}

static ErlNifFunc nif_funcs[] = {
  { "mac_refcnt", 1, mac_refcnt, 0 },
  { "unsafe_copy_binary", 2, unsafe_copy_binary, 0 },
};

ERL_NIF_INIT(lli_nif, nif_funcs, &on_load, NULL, NULL, NULL);
