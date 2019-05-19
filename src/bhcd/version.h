#ifndef	VERSION_H
#define	VERSION_H

#include "git_version.h"

#define BHCD_VERSION VERSION GIT_VERSION	
#define STR_VALUE_INNER(arg) #arg
#define STR_VALUE(arg) STR_VALUE_INNER(arg)
#define BHCD_NAME_VERSION   STR_VALUE(PACKAGE BHCD_VERSION)

#endif /*VERSION_H*/
