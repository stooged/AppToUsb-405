#include "ps4.h"
#include "pkg.h"
#include "patch.h"

#define INI_FILE "AppToUsb.ini"

int nthread_run;
char notify_buf[1024];
char usb_mount_path[256];
char ini_file_path[256];
int  xfer_pct;
long xfer_cnt;
char *cfile;
int tmpcnt;
int isxfer;


void systemMessage(char* msg) {
 	char buffer[512]; 
 	sprintf(buffer, "%s\n\n\n\n\n\n\n", msg);
 	sceSysUtilSendSystemNotificationWithText(0x81, buffer);
}


int file_compare(char *fname1, char *fname2)
{
    long size1, size2;
    int  bytesRead1 = 0, bytesRead2 = 0, lastBytes = 100, res = 0, i;
    FILE *file1 = fopen(fname1, "rb"), *file2 = fopen(fname2, "rb");
    char *buffer1 = malloc(65536), *buffer2 = malloc(65536);
    if (!file1 || !file2) {
        return res;
    }
    fseek (file1, 0, SEEK_END);
    fseek (file2, 0, SEEK_END);
    size1 = ftell (file1);
    size2 = ftell (file2);
    fseek(file1, 0L, SEEK_SET);
    fseek(file2, 0L, SEEK_SET);
    if (size1 != size2) {
        res = 0;
        goto exit;
    }
    if (size1 < lastBytes) lastBytes = size1;
    fseek(file1, -lastBytes, SEEK_END);
    fseek(file2, -lastBytes, SEEK_END);
    bytesRead1 = fread(buffer1, sizeof(char), lastBytes, file1);
    bytesRead2 = fread(buffer2, sizeof(char), lastBytes, file2);
    if (bytesRead1 > 0 && bytesRead1 == bytesRead2) {
        for ( i = 0; i < bytesRead1; i++) {
            if (buffer1[i] != buffer2[i]) {
                res = 0;
                goto exit;
            }
        }
        res = 1;
    }
    free(buffer1);
    free(buffer2);
    exit:
    fclose(file1);
    fclose(file2);
    return res;
}


char *replace_str( char *str,  char *orig,  char *rep)
{
    char *ret;
    int i, count = 0;
    size_t newlen = strlen(rep);
    size_t oldlen = strlen(orig);
    for (i = 0; str[i] != '\0'; i++) {
       if (strstr(&str[i], orig) == &str[i]) {
          count++;
          i += oldlen - 1;
       }
    }
    ret = malloc(i + count * (newlen - oldlen));
    if (ret == NULL)
    return str;
    i = 0;
    while (*str) 
	{
       if (strstr(str, orig) == str) {
       strcpy(&ret[i], rep);
       i += newlen;
       str += oldlen;
       } 
	   else
       ret[i++] = *str++;
     }
     ret[i] = '\0';
     return ret;
}


int split_string(char *str, char c, char ***arr)
{
    int count = 1;
    int token_len = 1;
    int i = 0;
    char *p;
    char *t;
    p = str;
    while (*p != '\0')
    {
        if (*p == c)
            count++;
        p++;
    }
    *arr = (char**) malloc(sizeof(char*) * count);
    if (*arr == NULL)
        return 0;
    p = str;
    while (*p != '\0')
    {
        if (*p == c)
        {
            (*arr)[i] = (char*) malloc( sizeof(char) * token_len );
            if ((*arr)[i] == NULL)
                return 0;
            token_len = 0;
            i++;
        }
        p++;
        token_len++;
    }
    (*arr)[i] = (char*) malloc( sizeof(char) * token_len );
    if ((*arr)[i] == NULL)
        return 0;
    i = 0;
    p = str;
    t = ((*arr)[i]);
    while (*p != '\0')
    {
        if (*p != c && *p != '\0')
        {
            *t = *p;
            t++;
        }
        else
        {
            *t = '\0';
            i++;
            t = ((*arr)[i]);
        }
        p++;
    }
    return count;
}


int fgetc(FILE *fp)
{
  char c;

  if (fread(&c, 1, 1, fp) == 0)
    return (-1);
  return (c);
}


char *read_string(FILE* f)
{
    char *string = malloc(sizeof(char) * 65536);
    int c;
    int length = 0;
    if (!string) return string;
    while((c = fgetc(f)) != -1)
    {
        string[length++] = c;
    }
    string[length++] = '\0';

    return realloc(string, sizeof(char) * length);
}


int file_exists(char *fname)
{
    FILE *file = fopen(fname, "rb");
    if (file)
    {
        fclose(file);
        return 1;
    }
    return 0;
}

int dir_exists(char *dname)
{
    DIR *dir = opendir(dname);

    if (dir)
    {
        closedir(dir);
        return 1;
    }
    return 0;
}


int symlink_exists(const char* fname)
{
    struct stat statbuf;
    if (lstat(fname, &statbuf) < 0) {
        return -1;
    }

    if (S_ISLNK(statbuf.st_mode) == 1) {
        return 1;
    } else {
        return 0;
    }
}



void makeini()
{
    if (!file_exists(ini_file_path)) 
    {
    FILE *ini = fopen(ini_file_path,"wb");
    char *buffer;
    buffer ="To check the usb root for the pkg file to save time copying from the internal ps4 drive then uncomment the line below.\r\nbut remember this will move the pkg from the root directory to the PS4 folder.\r\n//CHECK_USB\r\n\r\nTo rename previously linked pkg files to the new format uncomment the line below.\r\n//RENAME_APP\r\n\r\nTo disable the processing of icons/art and sound uncomment the line below\r\n//DISABLE_META\r\n\r\nTo leave game updates on the internal drive uncomment the line below.\r\n//IGNORE_UPDATES\r\n\r\nTo use this list as a list of games you want to move not ignore then uncomment the line below.\r\n//MODE_MOVE\r\n\r\nExample ignore or move usage.\r\n\r\nCUSAXXXX1\r\nCUSAXXXX2\r\nCUSAXXXX3";
    fwrite(buffer, 1, strlen(buffer), ini);
    fclose(ini);
    }
}


char *getContentID(char* pkgFile)
{
        char buffer[37];
        char *retval = malloc(sizeof(char)*37);
        FILE *pfile = fopen(pkgFile, "rb");
        fseek (pfile, 64, SEEK_SET);
        fread(buffer, 1, sizeof(buffer), pfile);
        fclose(pfile);
        strcpy(retval, buffer);
        return retval;
}


char *getPkgName(char* sourcefile)
{
   char *retval = malloc(sizeof(char)*256);
   char *jfile = replace_str(sourcefile, ".pkg", ".json");
   if (file_exists(jfile)) 
   {
      FILE *cfile = fopen(jfile, "rb");
      char *idata = read_string(cfile);
      fclose(cfile);
      char *ret;
      ret = strstr(idata, "\"url\":\"");
      if (ret != NULL)
      {
         int bcnt = 0;
         char **buf = NULL;
         bcnt = split_string(ret, '/', &buf);
         split_string(buf[bcnt - 1], '"', &buf);
         if (strlen(buf[0]) > 0)
         {
            buf[0] = replace_str(buf[0], ".pkg", "");
            strcpy(retval, buf[0]);
            return retval;
         }
      }   
   }
        char *cid = getContentID(sourcefile);
        strcpy(retval, cid);
        free(cid);
        return retval;       
}


int isinlist(char *sourcefile)
{
        if (file_exists(ini_file_path)) 
        {
            FILE *cfile = fopen(ini_file_path, "rb");
            char *idata = read_string(cfile);
            fclose(cfile);
            if (strlen(idata) != 0)
            {
             char *tmpstr;
                if (strstr(sourcefile, "/user/app/") != NULL)
                {
                tmpstr = replace_str(sourcefile, "/user/app/", "");
                tmpstr = replace_str(tmpstr, "/app.pkg", "");
                }
                else
                {
                tmpstr = replace_str(sourcefile, "/user/patch/", "");
                tmpstr = replace_str(tmpstr, "/patch.pkg", "");
                }
				if (strstr(sourcefile, "/user/addcont/") != NULL)
                {
				tmpstr = replace_str(sourcefile, "/user/addcont/", "");
                tmpstr = replace_str(tmpstr, "/ac.pkg", "");
                }
                if(strstr(idata, tmpstr) != NULL) 
                {
                   return 1;
                }
             return 0;
             }
        return 0;
        }
        else
        {
             return 0;
        }
}



int ismovemode()
{
        if (file_exists(ini_file_path)) 
        {
            FILE *cfile = fopen(ini_file_path, "rb");
            char *idata = read_string(cfile);
            fclose(cfile);
            if (strlen(idata) != 0)
            {
                if(strstr(idata, "//MODE_MOVE") != NULL) 
                {
                   return 0;
                }
                else if(strstr(idata, "MODE_MOVE") != NULL) 
                {
                   return 1;
                }
             return 0;
             }
        return 0;
        }
        else
        {
             return 0;
        }
}



int isusbcheck()
{
        if (file_exists(ini_file_path)) 
        {
            FILE *cfile = fopen(ini_file_path, "rb");
            char *idata = read_string(cfile);
            fclose(cfile);
            if (strlen(idata) != 0)
            {
                if(strstr(idata, "//CHECK_USB") != NULL) 
                {
                   return 0;
                }
                else if(strstr(idata, "CHECK_USB") != NULL) 
                {
                   return 1;
                }
             return 0;
             }
        return 0;
        }
        else
        {
             return 0;
        }
}


int isignupdates()
{
        if (file_exists(ini_file_path)) 
        {
            FILE *cfile = fopen(ini_file_path, "rb");
            char *idata = read_string(cfile);
            fclose(cfile);
            if (strlen(idata) != 0)
            {
                if(strstr(idata, "//IGNORE_UPDATES") != NULL) 
                {
                   return 0;
                }
                else if(strstr(idata, "IGNORE_UPDATES") != NULL) 
                {
                   return 1;
                }
             return 0;
             }
        return 0;
        }
        else
        {
             return 0;
        }
}


int isigndlc()
{
        if (file_exists(ini_file_path)) 
        {
            FILE *cfile = fopen(ini_file_path, "rb");
            char *idata = read_string(cfile);
            fclose(cfile);
            if (strlen(idata) != 0)
            {
                if(strstr(idata, "//IGNORE_DLC") != NULL) 
                {
                   return 0;
                }
                else if(strstr(idata, "IGNORE_DLC") != NULL) 
                {
                   return 1;
                }
             return 0;
             }
        return 0;
        }
        else
        {
             return 0;
        }
}
int isrelink()
{
        if (file_exists(ini_file_path)) 
        {
            FILE *cfile = fopen(ini_file_path, "rb");
            char *idata = read_string(cfile);
            fclose(cfile);
            if (strlen(idata) != 0)
            {
                if(strstr(idata, "//RENAME_APP") != NULL) 
                {
                   return 0;
                }
                else if(strstr(idata, "RENAME_APP") != NULL) 
                {
                   return 1;
                }
             return 0;
             }
        return 0;
        }
        else
        {
             return 0;
        }
}


int isnometa()
{
        if (file_exists(ini_file_path)) 
        {
            FILE *cfile = fopen(ini_file_path, "rb");
            char *idata = read_string(cfile);
            fclose(cfile);
            if (strlen(idata) != 0)
            {
                if(strstr(idata, "//DISABLE_META") != NULL) 
                {
                   return 0;
                }
                else if(strstr(idata, "DISABLE_META") != NULL) 
                {
                   return 1;
                }
             return 0;
             }
        return 0;
        }
        else
        {
             return 0;
        }
}


void resetflags()
{
    if (file_exists(ini_file_path)) 
    {
       FILE *cfile = fopen(ini_file_path, "rb");
       char *idata = read_string(cfile);
       fclose(cfile);
       if (strlen(idata) != 0)
       {
          if(strstr(idata, "//RENAME_APP") == NULL) 
          {
             idata = replace_str(idata, "RENAME_APP", "//RENAME_APP");
          }
          FILE *dfile = fopen(ini_file_path,"wb");
          fwrite(idata, 1, strlen(idata), dfile);
          fclose(dfile);
       }
    }
}


void copySmFile(char *sourcefile, char* destfile)
{
     FILE *src = fopen(sourcefile, "rb");
     if (src)
     {
         FILE *out = fopen(destfile,"wb");
         if (out)
         {
             size_t bytes;
             char *buffer = malloc(65536);
             if (buffer != NULL)
             {
                 while (0 < (bytes = fread(buffer, 1, 65536, src)))
                     fwrite(buffer, 1, bytes, out);
                     free(buffer);
             }
             fclose(out);
         }
         else {
         }
         fclose(src);
     }
     else {
     }
}


void copyFile(char *sourcefile, char* destfile)
{
    FILE *src = fopen(sourcefile, "rb");
    if (src)
    {
        FILE *out = fopen(destfile,"wb");
        if (out)
        {
            cfile = sourcefile;
            isxfer = 1;
            size_t bytes, bytes_size, bytes_copied = 0;
            char *buffer = malloc(65536);
            if (buffer != NULL)
            {
                fseek(src, 0L, SEEK_END);
                bytes_size = ftell(src);
                fseek(src, 0L, SEEK_SET);
                while (0 < (bytes = fread(buffer, 1, 65536, src))) {
                    fwrite(buffer, 1, bytes, out);
                    bytes_copied += bytes;
                    if (bytes_copied > bytes_size) bytes_copied = bytes_size;
                   xfer_pct = bytes_copied * 100 / bytes_size;
                   xfer_cnt += bytes;
                }
                free(buffer);
            }
            fclose(out);
            isxfer = 0;
            xfer_pct = 0;
            xfer_cnt = 0;
            unlink(sourcefile);
            symlink(destfile, sourcefile);
        }
        else {
        }
        fclose(src);
    }
    else {
    }
}


void makePkgInfo(char *pkgFile, char *destpath)
{
    if(strstr(pkgFile, "app.pkg") != NULL && !isnometa())
    {
        char *titleid;
        char srcfile[256];
        char dstfile[256];
        destpath = replace_str(destpath, "/app.pkg", "");
        titleid = replace_str(pkgFile, "/user/app/", "");
        titleid = replace_str(titleid, "/app.pkg", "");
        sprintf(srcfile, "/user/appmeta/%s/pronunciation.xml", titleid);
        if (file_exists(srcfile))
        {
           char *cid = getContentID(pkgFile);
           sprintf(dstfile, "%s/%s.txt", destpath , cid);
           free(cid);
           if (!file_exists(dstfile))
           {
              copySmFile(srcfile, dstfile);
           }
        }
        sprintf(srcfile, "/user/appmeta/%s/icon0.png", titleid);
        sprintf(dstfile, "%s/icon0.png", destpath);
        if (file_exists(srcfile) && !file_exists(dstfile))
        {
           copySmFile(srcfile, dstfile);
        }
        sprintf(srcfile, "/user/appmeta/%s/pic0.png", titleid);
        sprintf(dstfile, "%s/pic0.png", destpath);
        if (file_exists(srcfile) && !file_exists(dstfile))
        {
           copySmFile(srcfile, dstfile);
        }
        sprintf(srcfile, "/user/appmeta/%s/pic1.png", titleid);
        sprintf(dstfile, "%s/pic1.png", destpath);
        if (file_exists(srcfile) && !file_exists(dstfile))
        {
           copySmFile(srcfile, dstfile);
        }
        sprintf(srcfile, "/user/appmeta/%s/snd0.at9", titleid);
        sprintf(dstfile, "%s/snd0.at9", destpath);
        if (file_exists(srcfile) && !file_exists(dstfile))
        {
           copySmFile(srcfile, dstfile);
        }
    }
}


void copypkg(char *sourcepath, char* destpath)
{       
    if (!symlink_exists(sourcepath))
    {
        if (isfpkg(sourcepath) == 0) 
        {
            char cmsg[1024];
            char dstfile[256];
            char *ndestpath;
            char *pknm = getPkgName(sourcepath);
            sprintf(dstfile, "%s.pkg", pknm);
            free(pknm);
            if(strstr(sourcepath, "app.pkg") != NULL)
            {
                ndestpath = replace_str(destpath, "app.pkg", dstfile);
            }
            else
            {
                ndestpath = replace_str(destpath, "patch.pkg", dstfile);
            }
            
			if(strstr(sourcepath, "ac.pkg") != NULL)
            {                
				ndestpath = replace_str(destpath, "ac.pkg", dstfile);
            }
            if (file_exists(destpath)) 
            {
               rename(destpath, ndestpath);
            }
            if (!file_exists(ndestpath)) 
            {
                sprintf(cmsg, "%s\n%s", "Processing:" , sourcepath);
                systemMessage(cmsg);
                copyFile(sourcepath, ndestpath);
            }
            else
            {
                if (!file_compare(sourcepath, ndestpath))
                {
                    sprintf(cmsg, "%s\n%s\nOverwriting as pkg files are mismatched", "Found pkg at " , ndestpath);
                    systemMessage(cmsg);
                    copyFile(sourcepath, ndestpath);
                } 
                else
                {  
                    sprintf(cmsg, "%s\n%s\nSkipping copy and linking existing pkg", "Found pkg at " , ndestpath);
                    systemMessage(cmsg);
                    sceKernelSleep(5);
                    unlink(sourcepath);
                    symlink(ndestpath, sourcepath);
                }
            }
        }
    }
}


void checkusbpkg(char *sourcedir, char* destdir) {
   if (isusbcheck())
   {
     if (!symlink_exists(sourcedir))
     {
        if (isfpkg(sourcedir) == 0) 
        {
            char dstfile[256];
            char *pknm = getPkgName(sourcedir);
            sprintf(dstfile, "%s.pkg", pknm);
            free(pknm);
            if(strstr(sourcedir, "app.pkg") != NULL)
            {
                destdir = replace_str(destdir, "app.pkg", dstfile);
            }
            else
            {
                destdir = replace_str(destdir, "patch.pkg", dstfile);
            }
            if(strstr(sourcedir, "ac.pkg") != NULL)
            {                
				destdir = replace_str(destdir, "ac.pkg", dstfile);
            }
            if (!file_exists(destdir)) 
            {
            DIR *dir;
            struct dirent *dp;
            struct stat info;
            char upkg_path[1024];
            dir = opendir(usb_mount_path);
            if (dir) {
            while ((dp = readdir(dir)) != NULL)
            {
            if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..") || !strcmp(dp->d_name, "$RECYCLE.BIN") || !strcmp(dp->d_name, "System Volume Information"))
            {}
            else
            {
            sprintf(upkg_path, "%s/%s", usb_mount_path, dp->d_name);
            if (!stat(upkg_path, &info))
            {
                if (S_ISREG(info.st_mode))
                {
		if (file_compare(sourcedir, upkg_path))
		{
		    rename(upkg_path, destdir);
		    break;
	        }
             }
          }
        }
      }
      closedir(dir);
      }
     }
   }
 }
}
}


void relink(char *sourcepath, char* destpath)
{
   if (isrelink())
   {
      if (symlink_exists(sourcepath))
      {
         char dstfile[256];
         char cidfile[50];
         char *ndestpath;
         if (file_exists(destpath)) 
         {
            char *pknm = getPkgName(sourcepath);
            sprintf(dstfile, "%s.pkg", pknm);
            free(pknm);
            if(strstr(destpath, "app.pkg") != NULL)
            {
                ndestpath = replace_str(destpath, "app.pkg", dstfile);
            }
            else
            {
                ndestpath = replace_str(destpath, "patch.pkg", dstfile);
            }
            if(strstr(destpath, "ac.pkg") != NULL)
            {                
				ndestpath = replace_str(destpath, "ac.pkg", dstfile);
            }
            rename(destpath, ndestpath);
            unlink(sourcepath);
            symlink(ndestpath, sourcepath);
         }
         else
         {
            char *cdestpath;
            char *cid = getContentID(sourcepath);
            sprintf(cidfile, "%s.pkg", cid);
            free(cid);
            if(strstr(sourcepath, "app.pkg") != NULL)
            {
                cdestpath = replace_str(destpath, "app.pkg", cidfile);
            }
            else
            {
                cdestpath = replace_str(destpath, "patch.pkg", cidfile);
            }
            if(strstr(sourcepath, "ac.pkg") != NULL)
            {                
				cdestpath = replace_str(destpath, "ac.pkg", cidfile);
            }
            if (file_exists(cdestpath)) 
            {
                char *pknm = getPkgName(sourcepath);
                sprintf(dstfile, "%s.pkg", pknm);
                free(pknm);
                ndestpath = replace_str(cdestpath, cidfile, dstfile);
                rename(cdestpath, ndestpath);
                unlink(sourcepath);
                symlink(ndestpath, sourcepath);
            }
         }
      }
   }
}


void copyDir(char *sourcedir, char* destdir)
{
    DIR *dir;
    struct dirent *dp;
    struct stat info;
    char src_path[1024], dst_path[1024];
    dir = opendir(sourcedir);
    if (!dir)
        return;
        mkdir(destdir, 0777);
    while ((dp = readdir(dir)) != NULL)
    {
        if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
        {}
        else
        {
            sprintf(src_path, "%s/%s", sourcedir, dp->d_name);
            sprintf(dst_path, "%s/%s", destdir  , dp->d_name);

            if (!stat(src_path, &info))
            {
                if (S_ISDIR(info.st_mode))
                {
                  copyDir(src_path, dst_path);
                }
                else
                if (S_ISREG(info.st_mode))
                {
                  if(strstr(src_path, "app.pkg") != NULL || strstr(src_path, "patch.pkg") != NULL || strstr(src_path, "ac.pkg") != NULL) 
                  {
                   if (ismovemode() )
                   {
                   if (isinlist(src_path) )
                   {
                     relink(src_path, dst_path);
                     makePkgInfo(src_path, dst_path);
                     checkusbpkg(src_path, dst_path);
                     copypkg(src_path, dst_path);
                   }
                 }
                 else
                 {
                   if (!isinlist(src_path) )
                   {
                     relink(src_path, dst_path);
                     makePkgInfo(src_path, dst_path);
                     checkusbpkg(src_path, dst_path);
                     copypkg(src_path, dst_path);
                   }
                 }
               }
             }
          }
       }
    }
    closedir(dir);
}


void copyMeta(char *sourcedir, char* destdir)
{
    DIR *dir;
    struct dirent *dp;
    struct stat info;
    char src_path[1024], dst_path[1024];
    dir = opendir(sourcedir);
    if (!dir)
        return;
        mkdir(destdir, 0777);
    while ((dp = readdir(dir)) != NULL)
    {
        if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..") || !strcmp(dp->d_name, "$RECYCLE.BIN") || !strcmp(dp->d_name, "System Volume Information"))
        {}
        else
        {
            sprintf(src_path, "%s/%s", sourcedir, dp->d_name);
            sprintf(dst_path, "%s/%s", destdir  , dp->d_name);

            if (!stat(src_path, &info))
            {
                if (S_ISDIR(info.st_mode))
                {
                  copyMeta(src_path, dst_path);
                }
                else
                if (S_ISREG(info.st_mode))
                {
                   if(strstr(src_path, "icon0.png") != NULL || strstr(src_path, "pic0.png") != NULL || strstr(src_path, "pic1.png") != NULL || strstr(src_path, "snd0.at9") != NULL)
                   { 
                      if (file_exists(dst_path)) 
                      {
                         if (!file_compare(src_path, dst_path))
                         {
                            copySmFile(src_path, dst_path);
                         }
                      }
                   }
                }
            }
        }
    }
    closedir(dir);
}


void *nthread_func(void *arg)
{
        time_t t1, t2;
        t1 = 0;
	while (nthread_run)
	{
		if (isxfer)
		{
			t2 = time(NULL);
			if ((t2 - t1) >= 20)
			{
				t1 = t2;
				if (tmpcnt >= 1048576)
				{
				   sprintf(notify_buf, "Copying: %s\n\n%u%% completed\nSpeed: %u MB/s", cfile , xfer_pct, tmpcnt / 1048576);
				}
				else if (tmpcnt >= 1024)
				{
				   sprintf(notify_buf, "Copying: %s\n\n%u%% completed\nSpeed: %u KB/s", cfile , xfer_pct, tmpcnt / 1024);
				}
				else
				{
				   sprintf(notify_buf, "Copying: %s\n\n%u%% completed\nSpeed: %u B/s", cfile , xfer_pct, tmpcnt);
				}
				
				systemMessage(notify_buf);
			}
		}
		else t1 = 0;
		sceKernelSleep(1);
	}
	return NULL;
}



void *sthread_func(void *arg)
{
	while (nthread_run)
	{
           if (isxfer)
           {
              tmpcnt = xfer_cnt;
              xfer_cnt = 0;
           }
          sceKernelSleep(1);
	}
	return NULL;
}


char* getusbpath()
{
    FILE *usbdir;
    char tmppath[64];
    char ps4path[64];
    char tmpusb[64];
    tmpusb[0] = '\0';
    char *retval;
    for (int x = 0; x <= 7; x++)
    {
       sprintf(tmppath, "/mnt/usb%i/.dirtest", x);
       usbdir = fopen(tmppath, "wb");
                if (usbdir)
                {
                fclose(usbdir);
                unlink(tmppath);
                sprintf(tmpusb, "/mnt/usb%i", x);
                    sprintf(ps4path, "/mnt/usb%i/PS4/", x);
                    if (dir_exists(ps4path))
                    {
                    sprintf(ps4path, "/mnt/usb%i", x);
                    retval = malloc (sizeof (char) * 10);
                    strcpy(retval, ps4path);
                    return retval;
                    }
                    
             }
    }
    if (tmpusb[0] != '\0')
     {
       retval = malloc (sizeof (char) * 10);
       strcpy(retval, tmpusb);
       return retval;
     }
     return NULL;
}



int _main(struct thread *td) {
	initKernel();
	initLibc();
        initPthread();
        

   DIR *dir;
    dir = opendir("/user/app");
    if (!dir)
    {
       syscall(11,patcher,td);
    }
    else
    {
       closedir(dir);
    }
    initSysUtil();
        xfer_cnt = 0;
        isxfer = 0;
	nthread_run = 1;
	ScePthread nthread;
	scePthreadCreate(&nthread, NULL, nthread_func, NULL, "nthread");
	ScePthread sthread;
	scePthreadCreate(&sthread, NULL, sthread_func, NULL, "sthread");
        systemMessage("Warning this payload will modify the filesystem on your PS4\n\nUnplug your usb drive to cancel this");
        sceKernelSleep(10);
        systemMessage("Last warning\n\nUnplug your usb drive to cancel this");
        sceKernelSleep(10);
        char* usb_mnt_path = getusbpath();
    if (usb_mnt_path != NULL)
    {
            sprintf(usb_mount_path, "%s", usb_mnt_path);
            free(usb_mnt_path);
            char tmppath[256];
            sprintf(tmppath, "%s/PS4", usb_mount_path);
            if (!dir_exists(tmppath)) 
            {
            mkdir(tmppath, 0777);
            }
            sprintf(ini_file_path, "%s/%s", usb_mount_path, INI_FILE);
            if (!file_exists(ini_file_path))
            {
            sprintf(ini_file_path, "%s/%s", tmppath, INI_FILE);
            makeini();
            }
            systemMessage("Copying Apps to USB\n\nThis will take a while if you have lots of games installed");
            copyDir("/user/app",tmppath);
            if (!isignupdates())
            {
            char tmppathp[256];
            sprintf(tmppathp, "%s/PS4/updates", usb_mount_path);
            if (!dir_exists(tmppathp)) 
            {
               mkdir(tmppathp, 0777);
            }
               systemMessage("Copying updates to USB");
               copyDir("/user/patch",tmppathp);
            }
            if (!isigndlc())
            {
            char tmppathp[256];
            sprintf(tmppathp, "%s/PS4/addcont", usb_mount_path);
            if (!dir_exists(tmppathp)) 
            {
               mkdir(tmppathp, 0777);
            }
               systemMessage("Copie des DLC vers l'USB0");
               copyDir("/user/addcont",tmppathp);
            }		
            if (!isnometa())
            {
               systemMessage("Processing appmeta");
               copyMeta(tmppath,"/user/appmeta");
            }
            if (isrelink())
            {
               resetflags();
            }
            systemMessage("Complete.");
    }
    else
    {
            systemMessage("No usb mount found.\nYou must use a eXfat formatted usb hdd");
    }
    nthread_run = 0;
    return 0;
}

