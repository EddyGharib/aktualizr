#include "libaktualizr/utilities/aktualizr_version.h"

#ifndef AKTUALIZR_VERSION
#include "src/libaktualizr/utilities/autogen_version.hpp"
#endif

const char *aktualizr_version() { return AKTUALIZR_VERSION; }
