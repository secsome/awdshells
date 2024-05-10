#pragma once

#define __str(x) #x
#define __str__(x) __str(x)

#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION_PATCH 1

#define VERSION_STR __str__(VERSION_MAJOR) "." __str__(VERSION_MINOR) "." __str__(VERSION_PATCH)
#define PROJECT_NAME "awdshells"
#define PROJECT_FULL_NAME PROJECT_NAME " " VERSION_STR