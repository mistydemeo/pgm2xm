/*

    pgm2xm
        Converts PGM module files (from Z80 ram dumps) to .XM

    By Ian Karlsson / ctr 2015-11-17 - 2015-11-18


    Usage:

    Create a Z80 RAM dump in MAME like this (do this for each song)
         focus soundcpu
         save z80prg.bin,0,10000
    Then copy the sample ROM from the romset (and the PGM BIOS samples) and run the program.

    Now you can open it and hopefully it will work. If you see any unknown commands please report.

    for non-Cave games just use the -b parameter, it will load the BIOS samples too in case some
    games use it.

*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

    uint8_t* source;        // Sound driver data
    uint8_t* samples;       // Sample ROM data

    uint8_t instable[256];  // Instrument table
    uint16_t inscount;     // Pointer to instrument count in xm file
    uint8_t* fptr;          // Current file position in xm file
    uint8_t* filedata;

    int samplebase;
    int usebios;
    int ftfix;

    uint8_t default_samplerom [] = "m04401b032.u17";
    uint8_t default_title [] =  "pgm2xm";
    //uint8_t default_filename [] = "out.xm";

uint8_t instrument(uint8_t id)
{
    id -= 1;

    uint16_t i;
    // search for matches
    for(i=0;i<inscount;i++)
    {
        if(instable[i] == id)
           return i+1;
    }

    // no matches were found - create new instrument
    instable[inscount] = id;

    inscount ++;
    return inscount;
}

void storeinstrument(int sampleid)
{
    uint16_t sptr = (*(uint16_t*)(source+0x60)) +(sampleid*22);

    uint8_t base = *(source+sptr);
    uint32_t start = (*(source+sptr+7)<<24) | (*(source+sptr+6)<<16) | (*(source+sptr+5)<<8);
    uint32_t end = (*(source+sptr+11)<<24) | (*(source+sptr+10)<<16) | (*(source+sptr+9)<<8);
    uint32_t lstart = (*(source+sptr+17)<<24) | (*(source+sptr+16)<<16) | (*(source+sptr+15)<<8);
    uint32_t lend = (*(source+sptr+21)<<24) | (*(source+sptr+20)<<16) | (*(source+sptr+19)<<8);

    uint8_t flags = *(source+sptr+1);
    //uint8_t unknown2 = *(source+sptr+2);
    //uint8_t unknown3 = *(source+sptr+3);
    uint8_t volume = *(source+sptr+12);

    //printf("base:%02x s:%08x e:%08x l:%08x le:%08x u:%02x%02x%02x v:%02x\n",base,start,end,lstart,lend,flags,unknown2,unknown3,volume);

    uint32_t startaddr = (((base<<20) & 0xffffff) | (start>>12)) - samplebase;
    uint32_t endaddr = (((base<<20) & 0xffffff) | (end>>12)) - samplebase;
    uint32_t lstartaddr = (((base<<20) & 0xffffff) | (lstart>>12)) - samplebase;
    uint32_t lendaddr = (((base<<20) & 0xffffff) | (lend>>12)) - samplebase;

    //printf("try: %08x, %08x, %08x, %08x",startaddr,endaddr, lstartaddr, lendaddr);

    uint32_t samplesize = endaddr-startaddr;
    uint32_t loopsize = lendaddr-lstartaddr;

    if((usebios==0 && startaddr <= 0x400000) || (startaddr > endaddr))
    {
        samplesize=0;
        loopsize=0;
        lstartaddr=0;
        startaddr=0;
    }

    int i;
    for(i=0;i<252;i++)
    {
        fptr[i]=0;
    }
    *(uint32_t*)(fptr+0) = 252;      // Instrument header size
    sprintf(fptr+4, "%02x at %04x", sampleid, sptr);
    *(uint16_t*)(fptr+27) = 1;       // Sample count
    *(uint32_t*)(fptr+29) = 40;      // Sample header size
    fptr += 252;
    *(uint32_t*)(fptr+0) = samplesize;  // Sample length
    *(uint32_t*)(fptr+4) = lstartaddr-startaddr;  // Loop start
    *(uint32_t*)(fptr+8) = loopsize;    // Loop length
    *(fptr+12) = volume;                // sample volume
    *(fptr+14) = flags&0x08 ? 1 : 0;    // loop enable
    *(fptr+15) = 128;                   // panning
    sprintf(fptr+18, "%02x at %06x", sampleid, startaddr);
    fptr += 40;

    uint8_t byte=0, temp=0, delta=0;
    for(i=0;i<samplesize;i++)
    {
        byte = samples[startaddr+i];
        temp=byte;
        byte -= delta;
        *fptr++ = byte;
        delta = temp;
    }
}

int readcommand(uint8_t** cmdptr, int chn, int pat)
{
    uint8_t cmd = **cmdptr;
    *cmdptr += 1;
    int effect = cmd&0x0f, param = 0, note = 0, ins = 0, flag=0x80, delay=0;

    if(cmd < 0x40)
    {
        // Delay
        delay += cmd;
    }
    else if(cmd < 0x50)
    {
        // Effect
        flag |= 0x18;
        param = *(*cmdptr);
        *cmdptr += 1;
    }
    else if(cmd >= 0x80 && cmd < 0xc0)
    {
        // Note
        flag |= 0x03;
        note = cmd&0x7f;
        ins = *(*cmdptr);
        *cmdptr += 1;
    }
    else if(cmd >= 0xc0 && cmd < 0xd0)
    {
        // Effect and note
        flag |= 0x1b;
        param = *(*cmdptr);
        note = *(*cmdptr+1);
        ins = *(*cmdptr+2);
        *cmdptr += 3;
    }
    else if(cmd >= 0xd0 && cmd < 0xe0)
    {
        // Effect and note, but not instrument (I guess, used by Espgaluda)
        flag |= 0x19;
        param = *(*cmdptr);
        note = *(*cmdptr+1);
        *cmdptr += 2;
    }
    else if(cmd >= 0xe0 && cmd < 0xf0)
    {
        // Effect and instrument, but not note (I guess, used by Espgaluda)
        flag |= 0x1a;
        param = *(*cmdptr);
        ins = *(*cmdptr+1);
        *cmdptr += 2;
    }
    else
    {
        printf("Unknown command %02x in pattern %d channel %d at %04x\n", cmd, pat, chn, *cmdptr-source);
    }

    *fptr++ = flag;
    if(flag & 0x01)
        *fptr++ = note+24;
    if(flag & 0x02)
        *fptr++ = instrument(ins);
    if(flag & 0x18)
    {
        // Fix fine tune
        if(effect == 0x0e && (param&0xf0) == 0x50 && ftfix)
        {
            param = (param&0xf0) | (((param&0x0f)-8)&0x0f);
        }

        *fptr++ = effect;
        *fptr++ = param;
    }

    return delay;
}

void song2xm(int songptr, uint8_t* title, uint8_t* filename)
{
    filedata = (uint8_t*) malloc (10000000);
    uint32_t filesize = 0;
    fptr = filedata;

    uint16_t* inscount2;

    uint8_t* channelptrs[16];
    uint8_t channeldelays[16];

    int i,j,r;
    for(i=0;i<256;i++)
        instable[i]=0;

    //uint8_t positions [256];
    uint16_t patterns[16][256];

    int num_positions = *(source+songptr);
    int num_channels = *(source+songptr+1);
    int num_patterns = *(source+songptr+2);

    uint8_t* d = source+songptr+4;

    printf("Reading file header\n");

    for(i=0;i<num_positions;i++)
    {
        //positions[i]=*d;
        filedata[80+i]=*d;
        d++;
    }

    if(num_positions&1)
        d++;

    for(j=0;j<num_channels;j++)
    {
        for(i=0;i<num_patterns;i++)
        {
            patterns[j][i] = (*(uint16_t*)d);
            d+=2;
        }

        if(i>0)
            patterns[j-1][num_patterns] = patterns[j][0];

        patterns[j][num_patterns] = patterns[j][num_patterns-1]+0x100;
    }

    printf("Writing XM header\n");

    // truncate strings longer than 20 chars
    if(strlen(title) > 20)
        title[20] = 0;

    sprintf(filedata, "Extended Module: %s", title);
    *(filedata+37)=0x1a;
    //                    01234567890123456789
    sprintf(filedata+38, "pgm2xm 151118 by ctr");
    *(uint16_t*)(filedata+58) = 0x104; // XM Version
    fptr += 60;
    *(uint32_t*)(fptr+0) = 276; // Header size
    *(uint16_t*)(fptr+4) = num_positions;
    *(uint16_t*)(fptr+6) = 0;
    *(uint16_t*)(fptr+8) = num_channels;
    *(uint16_t*)(fptr+10) = num_patterns;
    inscount2 = (uint16_t*)(fptr+12);
    *inscount2 = 0;
    *(uint16_t*)(fptr+14) = 0;       // flags
    *(uint16_t*)(fptr+16) = 6;       // default tempo
    *(uint16_t*)(fptr+18) = 125;     // default BPM
    fptr += 276;

    uint16_t* pattern_size;
    uint8_t* startoffset;

    //uint8_t command;

    // Parse patterns
    for(i=0; i<num_patterns; i++)
    {
        printf("Parsing pattern %d\n",i, fptr-filedata);

        *(uint32_t*)(fptr+0) = 9;
        *(fptr+4) = 0;
        *(uint16_t*)(fptr+5) = 64;
        pattern_size = (uint16_t*)(fptr+7);

        fptr+=9;
        startoffset = fptr;

        for(j=0;j<num_channels;j++)
        {
            channelptrs[j] = source + patterns[j][i];
            channeldelays[j] = 0;
        }

        for(r=0;r<64;r++)
        {
            for(j=0;j<num_channels;j++)
            {
                if(!channeldelays[j])
                    channeldelays[j] = readcommand(&channelptrs[j],j,i);
                else
                {
                    channeldelays[j]--;
                    *fptr++ = 0x80;
                }
            }
        }

        *pattern_size = fptr-startoffset;
        //printf("Pattern end @ %04x\n",fptr-filedata);
    }

    printf("%d instruments\n",inscount);
    //for(i=0;i<inscount;i++)
    //    printf("%02x ",instable[i]);
    printf("\n");

    for(i=0;i<inscount;i++)
        storeinstrument(instable[i]);

    *inscount2 = inscount;

    filesize = fptr-filedata;

    FILE *destfile;
    destfile = fopen(filename,"wb");
    if(!destfile)
    {
        printf("Could not open %s\n",filename);
        exit(EXIT_FAILURE);
    }
    for(i=0;i<filesize;i++)
    {
        putc(filedata[i],destfile);
    }
    fclose(destfile);
    printf("wrote %d bytes to %s\n",i,filename);

    return;

}

void loadsample(uint8_t* srom, char* filename)
{
    FILE* samplefile;
    samplefile = fopen(filename,"rb");

    if(!samplefile)
    {
        printf("Could not open Sample ROM (%s)\n",filename);
        perror("Error");
        exit(EXIT_FAILURE);
    }
    fseek(samplefile,0,SEEK_END);
    long samplefile_size = ftell(samplefile);
    rewind(samplefile);
//    samples = (uint8_t*)malloc(samplefile_size);
    //uint8_t* sampledata = (uint8_t*)malloc(samplefile_size);
    int res = fread(srom,1,samplefile_size,samplefile);
    if(res != samplefile_size)
    {
        printf("Reading error\n");
        exit(EXIT_FAILURE);
    }
    fclose(samplefile);

    printf("%s loaded at %06x\n", filename, srom-samples );

}

int main(int argc, char* argv [])
{
    int songid = 0;

    char trunc_name[FILENAME_MAX], default_filename[FILENAME_MAX], default_title[FILENAME_MAX];
    char *filename = default_filename, *samplerom = default_samplerom, *title = default_title, *temp;

    ftfix = 1;
    usebios=0;
    samplebase = 0x400000;

    int a, startarg = 1;
    for(a=1;a<argc;a++)
    {
        if((!strcmp(argv[a],"-b") || !strcmp(argv[a],"--enable-bios")))
        {
            printf("BIOS enabled\n\n");
            samplebase=0;
            usebios=1;

            startarg++;
        }
        if((!strcmp(argv[a],"-f") || !strcmp(argv[a],"--no-finetunefix")))
        {
            printf("Finetune fix disabled\n\n");
            ftfix=0;
            startarg++;
        }
        /* global options have to be before the module options,
           so if we see something that isn't a global option,
           don't even bother checking. */
        else
            break;
    }


    if(argc<=startarg)
    {
        printf("\npgm2xm ver 2015-11-18 by ctr\n\n"
               "Usage:\n"
               "\t%s [options] <z80rom.bin> <sample.bin> <songID> [output.xm] [title]\n\n"
               "Options:\n"
               "\t-b --no-baseoffset : Disables base offset of 0x400000. For games that\n"
               "\t                     use the BIOS samples.\n"
               "\t-f --no-finetunefix: Disables the fine tune fix. Use when converting to\n"
               "\t                     .MOD with OpenMPT.\n\n"
               , argv[0]);
        return 0;
    }
    if(argc>(startarg+1))  // sample rom
        samplerom=argv[startarg+1];
    if(argc>(startarg+2))  // song id
        songid=(int)strtoul(argv[startarg+2],NULL,0);
    if(argc>(startarg+3))  // filename
        filename=argv[startarg+3];
    if(argc>(startarg+4))  // title
        title=argv[startarg+4];

    strcpy(trunc_name,argv[startarg]);
    temp = strrchr(trunc_name,'.');
    if(temp)
        *temp = 0;  // truncate
    sprintf(default_filename, "%s.%02x.xm", trunc_name, songid);
    sprintf(default_title, "%s (%02x)", trunc_name, songid);

//    printf("argc=%d\nSong id %02x\n",argc,songid);

/*************************************************/
    FILE* sourcefile;
    sourcefile = fopen(argv[startarg],"rb");

    if(!sourcefile)
    {
        printf("Could not open %s\n",argv[startarg]);
        perror("Error");
        exit(EXIT_FAILURE);
    }
    fseek(sourcefile,0,SEEK_END);
    long sourcefile_size = ftell(sourcefile);
    rewind(sourcefile);
    source = (uint8_t*)malloc(sourcefile_size);
    int res;
    res = fread(source,1,sourcefile_size,sourcefile);
    if(res != sourcefile_size)
    {
        printf("Reading error\n");
        exit(EXIT_FAILURE);
    }
    fclose(sourcefile);

    samples = (uint8_t*)malloc(16777216);


/*
    FILE* samplefile;
    samplefile = fopen(samplerom,"rb");

    if(!samplefile)
    {
        printf("Could not open Sample ROM (%s)\n",samplerom);
        perror("Error");
        exit(EXIT_FAILURE);
    }
    fseek(samplefile,0,SEEK_END);
    long samplefile_size = ftell(samplefile);
    rewind(samplefile);
    samples = (uint8_t*)malloc(samplefile_size);
    //uint8_t* sampledata = (uint8_t*)malloc(samplefile_size);
    res = fread(samples,1,samplefile_size,samplefile);
    if(res != samplefile_size)
    {
        printf("Reading error\n");
        exit(EXIT_FAILURE);
    }
    fclose(samplefile);
*/
    if(usebios)
    {
        loadsample(samples,"pgm_m01s.rom");
        loadsample(samples+0x400000,samplerom);
    }
    else
        loadsample(samples,samplerom);


    int maxsongs = *(source+0x52);
    printf("Song %02x / %02x\n",songid,maxsongs-1);

    if(songid > maxsongs)
    {
        printf("Song ID does not exist\n");
    }
    else
    {
        uint16_t songptr = (*(uint16_t*)(source+0x70+(songid<<1)));
        printf("Header at %04x\n", songptr);
        song2xm(songptr, title, filename);
    }

    free(source);

    return 0;
}
