#include "erl_nif.h"
#include <assert.h>
#include <openssl/evp.h>
#include <stdatomic.h>
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

#define INIT_ATOMS                                                            \
  ATOM(ATOM_OK, ok)                                                           \
  ATOM(ATOM_ERROR, error)                                                     \
  ATOM(ATOM_SHOW, show)                                                       \
  ATOM(ATOM_PUT, put)                                                         \
  ATOM(ATOM_GET, get)                                                         \
  ATOM(ATOM_CRASHME_GET, crashme_get)                                         \
  ATOM(ATOM_CRASHME_PUT, crashme_put)

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
          cnt++;
          if (0 == cnt % 100000)
            {
              printf("mac refcnt: %u\n", ((MY_EVP_MAC *)mac)->refcnt);
            }
        }
      while (1);
    }
  else if (enif_is_identical(ATOM_CRASHME_GET, cmd))
    {
      do
        {
          mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
        }
      while (1);
    }
  // refcnt -1
  EVP_MAC_free(mac);
  return ret;
}

static ErlNifFunc nif_funcs[] = {
  { "mac_refcnt", 1, mac_refcnt, 0 },
};

ERL_NIF_INIT(lli_nif, nif_funcs, &on_load, NULL, NULL, NULL);
