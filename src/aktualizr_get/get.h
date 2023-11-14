#ifndef AKTUALIZR_GET_HELPERS
#define AKTUALIZR_GET_HELPERS

#include "libaktualizr/config.h"
#include "libaktualizr/storage/invstorage.h"

std::string aktualizrGet(Config &config, const std::string &url, const std::vector<std::string> &headers);

#endif  // AKTUALIZR_GET_HELPERS
