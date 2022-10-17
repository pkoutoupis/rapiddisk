/** @file
 * @brief JSON function declarations
 */
//
// Created by Matteo Tenca on 26/09/2022.
//

#ifndef JSON_H
#define JSON_H

#include "common.h"

int json_device_list(struct RD_PROFILE *rd, struct RC_PROFILE *rc, char **list_result, bool wantresult);
int json_resources_list(struct MEM_PROFILE *mem, struct VOLUME_PROFILE *volume, char **list_result, bool wantresult);
int json_cache_statistics(struct RC_STATS *stats, char **stats_result, bool wantresult);
int json_cache_wb_statistics(struct WC_STATS *stats, char **stats_result, bool wantresult);
int json_status_return(int return_value, char *optional_message, char **json_result, bool wantresult);
int json_nvmet_view_exports(struct NVMET_PROFILE *nvmet, struct NVMET_PORTS *ports, char **json_result, bool wantresult);
int json_nvmet_view_ports(struct NVMET_PORTS *ports, char **json_result, bool wantresult);
#ifdef SERVER
int json_status_check(char **json_str);
#endif

#endif //JSON_H
