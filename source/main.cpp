#include <gccore.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <ogc/machine/processor.h>
#include <wiiuse/wpad.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "devicemounter.h"

#define EXECUTE_ADDR    ((u8 *)0x92000000)
#define BOOTER_ADDR             ((u8 *)0x93000000)
#define ARGS_ADDR               ((u8 *)0x93200000)

#define Priiloader_CFG1 ((vu32*)0x8132FFFB)
#define Priiloader_CFG2 ((vu32*)0x817FEFF0)

#define MAX_CMDLINE 4096
#define MAX_ARGV    1000
struct __argv args;
char *a_argv[MAX_ARGV];
char *meta_buf = NULL;

extern "C" {
extern void __exception_setreload(int t);
extern void __exception_closeall();
};

typedef void (*entrypoint) (void);
extern const u8 app_booter_bin[];
extern const u32 app_booter_bin_size;

//static const char *DEFAULT_TARGET_PATH = "sd:/apps/projplus/usb.dol"; //default fallback

void SystemMenu()
{
        /* If priiloader is installed say return to system menu */
        *Priiloader_CFG1 = 0x50756E65;
        DCFlushRange((void*)Priiloader_CFG1, 4);
        *Priiloader_CFG2 = 0x50756E65;
        DCFlushRange((void*)Priiloader_CFG2, 4);
        /* Use Return to Menu to exit */
        SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
}

static bool IsDollZ(u8 *buf)
{
        u8 cmp1[] = {0x3C};
        return memcmp(&buf[0x100], cmp1, sizeof(cmp1)) == 0;
}

static bool IsSpecialELF(u8 *buf)
{
        u32 cmp1[] = {0x7F454C46};
        u8 cmp2[] = {0x00};
        return memcmp(buf, cmp1, sizeof(cmp1)) == 0 && memcmp(&buf[0x24], cmp2, sizeof(cmp2)) == 0;
}

void arg_init()
{
        memset(&args, 0, sizeof(args));
        memset(a_argv, 0, sizeof(a_argv));
        args.argvMagic = ARGV_MAGIC;
        args.length = 1; // double \0\0
        args.argc = 0;
        //! Put the argument into mem2 too, to avoid overwriting it
        args.commandLine = (char *) ARGS_ADDR + sizeof(args);
        args.argv = a_argv;
        args.endARGV = a_argv;
}

char* strcopy(char *dest, const char *src, int size)
{
        strncpy(dest,src,size);
        dest[size-1] = 0;
        return dest;
}

int arg_addl(char *arg, int len)
{
        if (args.argc >= MAX_ARGV)
                return -1;
        if (args.length + len + 1 > MAX_CMDLINE)
                return -1;
        strcopy(args.commandLine + args.length - 1, arg, len+1);
        args.length += len + 1; // 0 term.
        args.commandLine[args.length - 1] = 0; // double \0\0
        args.argc++;
        args.endARGV = args.argv + args.argc;
        return 0;
}

int arg_add(char *arg)
{
        return arg_addl(arg, strlen(arg));
}

void load_meta(const char *exe_path)
{
        char meta_path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
        memset(meta_path, 0, ISFS_MAXPATH);
        const char *p;

        p = strrchr(exe_path, '/');
        snprintf(meta_path, sizeof(meta_path), "%.*smeta.xml", p ? p-exe_path+1 : 0, exe_path);

        s32 fd = ISFS_Open(meta_path, ISFS_OPEN_READ);
        if(fd >= 0)
        {
                s32 filesize = ISFS_Seek(fd, 0, SEEK_END);
                ISFS_Seek(fd, 0, SEEK_SET);
                if(filesize)
                {
                        meta_buf = (char*)memalign(32, filesize + 1);
                        memset(meta_buf, 0, filesize + 1);
                        ISFS_Read(fd, meta_buf, filesize);
                }
                ISFS_Close(fd);
        }
        else
        {
                FILE *exeFile = fopen(meta_path ,"rb");
                if(exeFile)
                {
                        fseek(exeFile, 0, SEEK_END);
                        u32 exeSize = ftell(exeFile);
                        rewind(exeFile);
                        if(exeSize)
                        {
                                meta_buf = (char*)memalign(32, exeSize + 1);
                                memset(meta_buf, 0, exeSize + 1);
                                fread(meta_buf, 1, exeSize, exeFile);
                        }
                        fclose(exeFile);
                }
        }
}

void strip_comments(char *buf)
{
        char *p = buf; // start of comment
        char *e; // end of comment
        int len;
        while (p && *p)
        {
                p = strstr(p, "<!--");
                if (!p)
                        break;
                e = strstr(p, "-->");
                if (!e)
                {
                        *p = 0; // terminate
                        break;
                }
                e += 3;
                len = strlen(e);
                memmove(p, e, len + 1); // +1 for 0 termination
        }
}

void parse_meta()
{
        char *p;
        char *e, *end;
        if (meta_buf == NULL)
                return;
        strip_comments(meta_buf);
        if (!strstr(meta_buf, "<app") || !strstr(meta_buf, "</app>"))
                return;

        p = strstr(meta_buf, "<arguments>");
        if (!p)
                return;

        end = strstr(meta_buf, "</arguments>");
        if (!end)
                return;

        do
        {
                p = strstr(p, "<arg>");
                if (!p)
                        return;
                p += 5; //strlen("<arg>");
                e = strstr(p, "</arg>");
                if (!e)
                        return;
                arg_addl(p, e-p);
                p = e + 6;
        }
        while (p < end);

        if (meta_buf)
        {
                free(meta_buf);
                meta_buf = NULL;
        }
}

static bool ends_with_txt_ci(const char *name)
{
        size_t len = strlen(name);
        if (len < 4) return false;
        char b0 = name[len-4], b1 = name[len-3], b2 = name[len-2], b3 = name[len-1];
        return (b0 == '.') &&
               (tolower(b1) == 't') &&
               (tolower(b2) == 'x') &&
               (tolower(b3) == 't');
}

static bool has_alpha(const char *s, size_t n)
{
        for (size_t i = 0; i < n; ++i) {
                if (isalpha((int)(unsigned char)s[i])) return true;
        }
        return false;
}

static bool getModName(char *out, size_t outsz)
{
        const char *dirpath = "sd:/private/wii/app/rsbe/st";
        DIR *d = opendir(dirpath);
        if (!d) return false;

        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
                const char *n = e->d_name;
                if (!ends_with_txt_ci(n)) continue;

                size_t len = strlen(n);
                size_t core_len = len - 4;
                if (core_len == 0) continue;

                if (!has_alpha(n, core_len)) continue;

                size_t copy_len = (core_len < outsz - 1) ? core_len : (outsz - 1);
                memcpy(out, n, copy_len);
                out[copy_len] = '\0';
                closedir(d);
                return true;
        }

        closedir(d);
        return false;
}

static void DeInitDevices()
{
        SDCard_deInit();
        USBDevice_deInit();
        ISFS_Deinitialize();
}

int main(int argc, char *argv[])
{
        u32 cookie;
        entrypoint exeEntryPoint;
        __exception_setreload(0);

        // initial video
        void *xfb = NULL;
        GXRModeObj *rmode = NULL;

        VIDEO_Init();
        rmode = VIDEO_GetPreferredMode(NULL);
        xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
        VIDEO_Configure(rmode);
        VIDEO_SetNextFramebuffer(xfb);
        VIDEO_SetBlack(TRUE);
        VIDEO_Flush();
        VIDEO_WaitVSync();

        char full_path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
        memset(full_path, 0, ISFS_MAXPATH);
        bool FileFound = false;

        WPAD_Init();

        /* Original try both SD and USB *
        if(!FileFound)
        {
                for(u8 dev = SD; dev < MAXDEVICES; ++dev)
                {
                        if(FileFound)
                                break;
                        else if(dev == SD)
                                SDCard_Init();
                        else if(dev == USB1)
                                USBDevice_Init();
                        for(u8 i = 0; i < PathNum; i++)
                        {
                                sprintf(full_path, "%s:%s", DeviceName[dev], Paths[i]);
                                FILE *exeFile = fopen(full_path ,"rb");
                                if(exeFile)
                                {
                                        fseek(exeFile, 0, SEEK_END);
                                        u32 exeSize = ftell(exeFile);
                                        rewind(exeFile);
                                        if(exeSize)
                                        {
                                                fread(EXECUTE_ADDR, 1, exeSize, exeFile);
                                                FileFound = true;
                                        }
                                        fclose(exeFile);
                                        break;
                                }
                        }
                }
        }*/

        SDCard_Init();
        char appName[256] = {0};
        if (getModName(appName, sizeof(appName))) {
                snprintf(full_path, ISFS_MAXPATH, "sd:/apps/%.40s/boot.dol", appName);
        }

        FILE *exeFile = fopen(full_path, "rb");
        if(exeFile)
        {
                fseek(exeFile, 0, SEEK_END);
                u32 exeSize = ftell(exeFile);
                rewind(exeFile);
                if(exeSize)
                {
                        fread(EXECUTE_ADDR, 1, exeSize, exeFile);
                        FileFound = true;
                }
                fclose(exeFile);
        }

        if(!FileFound)
        {
                DeInitDevices();
                SystemMenu();
                return 0;
        }
        else
        {
                if(!IsDollZ(EXECUTE_ADDR) && !IsSpecialELF(EXECUTE_ADDR))
                {
                        arg_init();
                        arg_add(full_path); // argv[0] = full_path
                        // load meta.xml
                        load_meta(full_path);
                        // parse <arguments> in meta.xml
                        parse_meta();
                }
        }

        DeInitDevices();

        memcpy(BOOTER_ADDR, app_booter_bin, app_booter_bin_size);
        DCFlushRange(BOOTER_ADDR, app_booter_bin_size);

        memcpy(ARGS_ADDR, &args, sizeof(args));
        DCFlushRange(ARGS_ADDR, sizeof(args) + args.length);

        exeEntryPoint = (entrypoint)BOOTER_ADDR;
        /* cleaning up and load dol */
        SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
        _CPU_ISR_Disable(cookie);
        __exception_closeall();
        exeEntryPoint();
        _CPU_ISR_Restore(cookie);

        SystemMenu();
        return 0;
}
