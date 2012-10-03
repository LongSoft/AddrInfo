#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bios.h"

#define ERR_OK                      0
#define ERR_ARGS                    1
#define ERR_INPUT_FILE              2
#define ERR_OUTPUT_FILE             3
#define ERR_MEMORY                  4
#define ERR_NO_ASUSBKP              5

/* memmem - implementation of GNU memmem function using Boyer-Moore-Horspool algorithm */
unsigned char* memmem(unsigned char* string, size_t slen, const unsigned char* pattern, size_t plen)
{
    size_t scan = 0;
    size_t bad_char_skip[256];
    size_t last;

    if (plen == 0 || !string || !pattern)
        return NULL;

    for (scan = 0; scan <= 255; scan++)
        bad_char_skip[scan] = plen;

    last = plen - 1;

    for (scan = 0; scan < last; scan++)
        bad_char_skip[pattern[scan]] = last - scan;

    while (slen >= plen)
    {
        for (scan = last; string[scan] == pattern[scan]; scan--)
            if (scan == 0)
                return string;

        slen     -= bad_char_skip[string[last]];
        string   += bad_char_skip[string[last]];
    }

    return NULL;
}

/* find_free_space - finds free space between begin and end to insert new module. Returns alligned pointer to empty space or NULL if it can't be found. */
unsigned char* find_free_space(unsigned char* begin, unsigned char* end, size_t space_length, size_t allign)
{
    size_t pos;
    size_t free_bytes;

    if (space_length == 0 || !begin || !end)
        return NULL;

    free_bytes = 0;
    for(pos = 0; pos < (size_t)(end - begin); pos++)
    {
        if(*(begin+pos) == (unsigned char)'\xFF')
            free_bytes++;
        else
            free_bytes = 0;
        if(free_bytes == space_length)
        {
            pos -= free_bytes; /* back at the beginning of free space */
            if(allign > 1)
                pos += allign - pos%allign; /* alligning */
            return begin + pos;
        }
    }
    return NULL;
}

int main(int argc, char* argv[])
{
    FILE* file;                                                                 /* file pointer to work with input and output file */
    char* inputfile;                                                            /* path to input file*/
    char* outputfile;                                                           /* path to output file */
    unsigned char* buffer;                                                      /* buffer to read input file into */
    size_t filesize;                                                            /* size of opened file */
    size_t read;                                                                /* read bytes counter */
    size_t rest;                                                                /* rest of filesize to search in */
    unsigned char* asusbkp;                                                     /* ASUSBKP header */
    unsigned char* module;                                                      /* FD44 module header */
    unsigned char* me;                                                          /* ME firmware header */
    unsigned char* gbe;                                                         /* GbE firmware header */
    unsigned char* s2lp;                                                        /* */
    unsigned char* keys;                                                        /* */
    unsigned char* freespace;                                                   /* */
    unsigned char* msoa;                                                        /* */
    ptrdiff_t asusbkp_start_address = -1;                                       /* */
    ptrdiff_t asusbkp_s2lp_address = -1;                                        /* */
    ptrdiff_t asusbkp_keys_address = -1;                                        /* */
    ptrdiff_t asusbkp_freespace_address = -1;                                   /* ASUSBKP free space address*/
    ptrdiff_t slic_s2lp_address = -1;                                           /* */
    ptrdiff_t slic_keys_address = -1;                                           /* */
    ptrdiff_t slic_freespace_address = -1;                                      /* */
    ptrdiff_t me_start_address = -1;                                            /* ME start address */
    ptrdiff_t gbe_start_address = -1;                                           /* GbE start address */
    ptrdiff_t bsa_adresses[MAX_FD44_MODULES];                                   /* BSA modules addresses*/
    size_t bsa_count = 0;                                                       /* BSA modules count*/

    if(argc < 3)
    {
        printf("AddrInfo v0.1\nThis program finds addresses of different BIOS structures of ASUS BIOS files and stores them to INI-formated file\n\n"
            "Usage: AddrInfo BIOSFILE INIFILE\n\n");
        return ERR_ARGS;
    }

    inputfile = argv[1];
    outputfile = argv[2];

     /* Opening input file */
    file = fopen(inputfile, "rb");
    if (!file)
    {
        fprintf(stderr, "Can't open input file\n");
        return ERR_INPUT_FILE;
    }

    /* Determining file size */
    fseek(file, 0, SEEK_END);
    filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    /* Allocating memory for buffer */
    buffer = (unsigned char*)malloc(filesize);
    if (!buffer)
    {
        fprintf(stderr, "Can't allocate memory for input buffer\n");
        return ERR_MEMORY;
    }

    /* Reading whole file to buffer */
    read = fread((void*)buffer, sizeof(char), filesize, file);
    if (read != filesize)
    {
        fprintf(stderr, "Can't read input file\n");
        return ERR_INPUT_FILE;
    }

    /* Searching for ASUSBKP */
    asusbkp = memmem(buffer, filesize, ASUSBKP_HEADER, sizeof(ASUSBKP_HEADER));
    if(!asusbkp)
    {
        fprintf(stderr, "ASUSBKP signature not found in BIOS file. Nothing to do\n");
        return ERR_NO_ASUSBKP;
    }

    /* Storing ASUSBKP address */
    asusbkp_start_address = asusbkp - buffer;

    /* Finding free space in ASUSBKP, alligned to 4 */
    freespace = find_free_space(asusbkp, buffer + filesize - 1, ASUSBKP_FREE_SPACE_LENGTH, 4);
    if(!freespace)
    {
        fprintf(stderr, "No space left in ASUSBKP to insert data\n");
        return ERR_NO_ASUSBKP;
    }

    /* Storing free space address */
    asusbkp_freespace_address = freespace - buffer;
    
    /* Searching for S2LP in ASUSBKP */
    rest = freespace - asusbkp;
    s2lp = memmem(asusbkp, rest, ASUSBKP_S2LP_HEADER, sizeof(ASUSBKP_S2LP_HEADER));
    if(s2lp)
        asusbkp_s2lp_address = buffer - s2lp;

    /* Searching for KEYS in ASUSBKP */
    keys = memmem(asusbkp, rest, ASUSBKP_KEYS_HEADER, sizeof(ASUSBKP_KEYS_HEADER));
    if(keys)
        asusbkp_keys_address = buffer - keys;

    /* Searching for ME firmware address*/
    me = memmem(buffer, filesize, ME_HEADER, sizeof(ME_HEADER));
    if(me)
        me_start_address = me - buffer;

    /* Searching for GbE firmware address*/
    gbe = memmem(buffer, filesize, GBE_HEADER, sizeof(GBE_HEADER));
    if(gbe)
        gbe_start_address = gbe + GBE_MAC_OFFSET - buffer;
    
    /* Searching for MSOA module*/
    msoa = memmem(buffer, filesize, MSOA_MODULE_HEADER, sizeof(MSOA_MODULE_HEADER));
    if(msoa)
    {
        rest = filesize - (msoa - buffer);
        
        /* Searching for S2LP module*/
        s2lp = memmem(msoa, rest, SLIC_S2LP_HEADER, sizeof(SLIC_S2LP_HEADER));
        if(s2lp)
            slic_s2lp_address = s2lp - buffer;

        /* Searching for KEYS module*/
        keys = memmem(msoa, rest, SLIC_KEYS_HEADER, sizeof(SLIC_KEYS_HEADER));
        if(keys)
            slic_keys_address = keys - buffer;
        
        /* Finding free space and alligning it to 8*/
        freespace = find_free_space(msoa, buffer + filesize - 1, SLIC_FREE_SPACE_LENGTH, 8);
        if (freespace)
            slic_freespace_address = freespace - buffer;
    }

    /* Searching for module header */
    module = memmem(buffer, filesize, FD44_MODULE_HEADER, sizeof(FD44_MODULE_HEADER));
    if(module)
    {
        /* Looking for BSA_ module */
        rest = filesize - (module - buffer);
        while(module)
        {
            /* If one found, storing the address of it */
            if (!memcmp(module + FD44_MODULE_HEADER_BSA_OFFSET, FD44_MODULE_HEADER_BSA, sizeof(FD44_MODULE_HEADER_BSA)))
            {
                module = module + FD44_MODULE_HEADER_LENGTH;
                bsa_adresses[bsa_count++] = module - buffer;
            }

            module = memmem(module + FD44_MODULE_HEADER_LENGTH, rest, FD44_MODULE_HEADER, sizeof(FD44_MODULE_HEADER));
            rest = filesize - (module - buffer);
        }
    }

    /* Closing input file */
    fclose(file);

    /* Creating output file*/
    file = fopen(outputfile, "w");

    /* Writing data to output file */
    fprintf(file, "[ASUSBKP]\n");
    asusbkp_start_address > 0 ? fprintf(file,"START=0x%08X\n", asusbkp_start_address) : fprintf(file,"START=NOT_FOUND\n");
    asusbkp_keys_address > 0 ? fprintf(file,"KEYS=0x%08X\n", asusbkp_keys_address) : fprintf(file,"KEYS=NOT_FOUND\n");
    asusbkp_s2lp_address > 0 ? fprintf(file,"S2LP=0x%08X\n", asusbkp_s2lp_address) : fprintf(file,"S2LP=NOT_FOUND\n");
    asusbkp_freespace_address > 0 ? fprintf(file,"FREESPACE=0x%08X\n", asusbkp_freespace_address) : fprintf(file,"FREESPACE=NOT_FOUND\n");
    
    fprintf(file, "\n[SLIC]\n");
    slic_keys_address > 0 ? fprintf(file,"KEYS=0x%08X\n", slic_keys_address) : fprintf(file,"KEYS=NOT_FOUND\n");
    slic_s2lp_address > 0 ? fprintf(file,"S2LP=0x%08X\n", slic_s2lp_address) : fprintf(file,"S2LP=NOT_FOUND\n");
    slic_freespace_address > 0 ? fprintf(file,"FREESPACE=0x%08X\n", slic_freespace_address) : fprintf(file,"FREESPACE=NOT_FOUND\n");

    fprintf(file, "\n[ME]\n");
    me_start_address > 0 ? fprintf(file, "START=0x%08X\n", me_start_address) : fprintf(file,"START=NOT_FOUND\n");

    fprintf(file, "\n[GBE]\n");
    gbe_start_address > 0 ? fprintf(file, "START=0x%08X\n", gbe_start_address) : fprintf(file,"START=NOT_FOUND\n");

    if(bsa_count)
    {
        int i;
        fprintf(file, "\n[BSA]\n");
        for(i = 0; i < bsa_count; i++)
        {
            fprintf(file, "START%i=0x%08X\n", i, bsa_adresses[i] - FD44_MODULE_HEADER_LENGTH);
            fprintf(file, "DATA%i=0x%08X\n", i, bsa_adresses[i]);
        }
    }

    /* Closing output file */
    fclose(file);

    /* Freeing buffer */
    free(buffer);
    
    return ERR_OK;
}
