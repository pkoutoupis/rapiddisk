/** @file
 * @brief Utility functions declarations
 */
//
// Created by Matteo Tenca on 03/10/2022.
//

#ifndef UTILS_H
#define UTILS_H
#include "common.h"

int preg_replace(const char *re, char *replacement, char *subject, char *result, size_t pcre2_result_len);
int split(char* input_string, char** output_arr, char* delim);
void free_linked_lists(RC_PROFILE *rc_head, RD_PROFILE *rd_head, VOLUME_PROFILE *vp_head);
void free_nvmet_linked_lists(struct NVMET_PORTS *ports_head, struct NVMET_PROFILE *nvmet_head);
struct dirent **clean_scandir(struct dirent **scanlist, int num);
int check_loaded_modules(void);
void print_message(int ret_value, char *message, bool json_flag);

#endif //UTILS_H
