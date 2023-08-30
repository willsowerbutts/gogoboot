#ifndef __GOGOBOOT_LOADER_DOT_H__
#define __GOGOBOOT_LOADER_DOT_H__

bool load_m68k_executable(char *argv[], int argc, FIL *fd);
bool load_elf_executable(char *arg[], int numarg, FIL *fd);

#endif
