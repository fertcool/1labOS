#include <iostream>
#include <vector>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
using namespace std;

//ф-я поиска всех файлов в заданной директории
void GetFiles(char* dir, vector<char*>& files)
{
    DIR* dp;
    struct dirent* entry;
    struct stat statbuf;
    if ((dp = opendir(dir)) == NULL)
    {
        cout<<"Cannot open directory: "<< dir<<endl;
        return;
    }
    chdir(dir);
    while((entry = readdir(dp)) != NULL)
    {
        //создание полного пути файла
        char* fullentry = new char[strlen(dir)+strlen(entry->d_name)+1];
        strcpy(fullentry, dir);
        strcat(fullentry, "/");
        strcat(fullentry, entry->d_name);

        lstat(entry->d_name, &statbuf);
        if(S_ISDIR(statbuf.st_mode))
        {
            if (strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0)
                continue;
            GetFiles(fullentry, files);
        }
        else
        {
            files.push_back(fullentry);
        }
    }
    chdir("..");
    closedir(dp);
}
void archivate(char* dir)
{
    vector<char*> files;
    vector<int> filesizes;
    GetFiles(dir, files);

    FILE* out = fopen("combined", "wb");
    fprintf(out, "%d\n", files.size());

    //цикл создания заголовка
    for (auto file: files)
    {
        int c;
        FILE* in = fopen(file, "rb");
        int filesize = 0;
        while((c = fgetc(in)) != EOF)
            ++filesize;

        filesizes.push_back(filesize);

        fprintf(out, "%s %d\n", file, filesize);
        
        fclose(in);
    }

    // цикл записи данных в файлах 
    for (int i = 0;i < files.size();++i)
    {
        int c;
        FILE* in = fopen(files[i], "rb");

        for (int j = 0; j < filesizes[i]; ++j)
            fputc(fgetc(in), out);
        
        fclose(in);
    }
   
    fclose(out);
}
int main(int argc, char* argv[])
{
    char* archdir = argv[1];

    archivate(archdir);
    return 0;

}