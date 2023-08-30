/* this is based in part on main68.c from KISS-BIOS 
 * Portions Copyright (C) 2011-2016 John R. Coffman.
 * Portions Copyright (C) 2015-2023 William R. Sowerbutts
 */

#include <types.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fatfs/ff.h>
#include <tinyalloc.h>
#include <cli.h>

void do_cd(char *argv[], int argc)
{
    FRESULT r;

    r = f_chdir(argv[0]);
    if(r != FR_OK)
        f_perror(r);
}

void do_mkdir(char *argv[], int argc)
{
    FRESULT fr = f_mkdir(argv[0]);
    if(fr != FR_OK){
        printf("f_mkdir(\"%s\"): ", argv[0]);
        f_perror(fr);
    }
}

#define COPY_BUFFER_SIZE 65536
void do_cp(char *argv[], int argc)
{
    FRESULT fr;
    FIL src, dst;
    char *buffer;
    bool done = false;
    UINT bytes_read, bytes_written;

    fr = f_open(&src, argv[0], FA_READ);
    if(fr != FR_OK){
        printf("f_open(\"%s\"): ", argv[0]);
        f_perror(fr);
        return;
    }

    fr = f_open(&dst, argv[1], FA_WRITE | FA_CREATE_ALWAYS);
    if(fr != FR_OK){
        printf("f_open(\"%s\"): ", argv[1]);
        f_perror(fr);
        f_close(&src);
        return;
    }

    buffer = malloc(COPY_BUFFER_SIZE);
    if(!buffer){
        printf("Out of memory\n");
    }else{
        while(!done){
            fr = f_read(&src, buffer, COPY_BUFFER_SIZE, &bytes_read);
            if(fr != FR_OK){
                printf("f_read(\"%s\"): ", argv[0]);
                f_perror(fr);
                done = true;
            }else{
                if(bytes_read > 0){
                    fr = f_write(&dst, buffer, bytes_read, &bytes_written);
                    if(fr != FR_OK || bytes_read != bytes_written){
                        printf("f_write(\"%s\"): ", argv[1]);
                        f_perror(fr);
                        done = true;
                    }
                }
                if(bytes_read < COPY_BUFFER_SIZE)
                    done = true;
            }
        }
        free(buffer);
    }

    fr = f_close(&src);
    if(fr != FR_OK) f_perror(fr);
    fr = f_close(&dst);
    if(fr != FR_OK) f_perror(fr);
}

void do_mv(char *argv[], int argc)
{
    FRESULT fr = f_rename(argv[0], argv[1]);
    if(fr != FR_OK){
        printf("f_rename(\"%s\", \"%s\"): ", argv[0], argv[1]);
        f_perror(fr);
    }
}

void do_rm(char *argv[], int argc)
{
    FRESULT fr;
    DIR di;
    FILINFO fi;

    for(int i=0; i<argc; i++){
        while(true){
            fr = f_findfirst(&di, &fi, "", argv[i]);
            f_closedir(&di); // in any event we MUST close the dir before calling f_unlink
            if(fr == FR_OK && fi.fname[0]){
                printf("Deleting \"%s\"\n", fi.fname);
                f_unlink(fi.fname);
            }else
                break;
        }
    }
}

static int ls_sort_name(const void *a, const void *b)
{
    return strcasecmp((*(FILINFO**)a)->fname, (*(FILINFO**)b)->fname);
}

void do_ls(char *argv[], int argc)
{
    FRESULT fr;
    const char *path;
    DIR fat_dir;
    FILINFO *fat_file;
    FILINFO **fat_file_ptr;
    DWORD free_clusters, csize, free_sectors;
    FATFS *fatfs;
    int fat_file_used = 0;
    int fat_file_length = 2;

    if(argc == 0)
        path = "";
    else
        path = argv[0];

    fr = f_opendir(&fat_dir, path);
    if(fr != FR_OK){
        printf("f_opendir(\"%s\"): ", path);
        f_perror(fr);
        return;
    }

    fat_file = malloc(sizeof(FILINFO) * fat_file_length);

    while(true){
        if(fat_file_used == fat_file_length){
            fat_file_length *= 2;
            fat_file = realloc(fat_file, sizeof(FILINFO) * fat_file_length);
        }
        fr = f_readdir(&fat_dir, &fat_file[fat_file_used]);
        if(fr != FR_OK){
            printf("f_readdir(): ");
            f_perror(fr);
            break;
        }
        if(fat_file[fat_file_used].fname[0] == 0) /* end of directory? */
            break;
        fat_file_used++;
    }
    fr = f_closedir(&fat_dir);

    if(fr != FR_OK){
        printf("f_closedir(): ");
        f_perror(fr);
        // report but keep going
    }

    // sort into name order
    // it is faster to sort pointers, since it avoids copying around 
    // the entries themselves, which are quite large with LFN enabled.
    fat_file_ptr = (FILINFO**)malloc(sizeof(FILINFO*)*fat_file_used);
    for(int i=0; i<fat_file_used; i++)
        fat_file_ptr[i] = &fat_file[i];

    qsort(fat_file_ptr, fat_file_used, sizeof(FILINFO*), ls_sort_name);
    
    for(int i=0; i<fat_file_used; i++){
        if(fat_file_ptr[i]->fattrib & AM_DIR){
            /* directory */
            printf("           %04d-%02d-%02d %02d:%02d %s/", 
                    1980 + ((fat_file_ptr[i]->fdate >> 9) & 0x7F),
                    (fat_file_ptr[i]->fdate >> 5) & 0xF,
                    fat_file_ptr[i]->fdate & 0x1F,
                    fat_file_ptr[i]->ftime >> 11,
                    (fat_file_ptr[i]->ftime >> 5) & 0x3F,
                    fat_file_ptr[i]->fname);
        }else{
            /* regular file */
            printf("%10lu %04d-%02d-%02d %02d:%02d %s", 
                    fat_file_ptr[i]->fsize, 
                    1980 + ((fat_file_ptr[i]->fdate >> 9) & 0x7F),
                    (fat_file_ptr[i]->fdate >> 5) & 0xF,
                    fat_file_ptr[i]->fdate & 0x1F,
                    fat_file_ptr[i]->ftime >> 11,
                    (fat_file_ptr[i]->ftime >> 5) & 0x3F,
                    fat_file_ptr[i]->fname);
        }

        printf("\n");
    }

    free(fat_file);
    free(fat_file_ptr);

    fr = f_getfree(path, &free_clusters, &fatfs);
    if(fr != FR_OK){
        printf("f_getfree(\"%s\"): ", path);
        f_perror(fr);
        return;
    }

    // calculate cluster size in bytes
    csize = 
        #if FF_MAX_SS != FF_MIN_SS
        fatfs->ssize
        #else
        FF_MAX_SS
        #endif
        * fatfs->csize;

    // free space measured in units of 512 bytes (max 2TB in 32 bits)
    free_sectors = (csize >> 9) * free_clusters;
        
    printf("%ld MB free (%ld clusters of %ld bytes)\n", free_sectors >> 11, free_clusters, csize);
}
