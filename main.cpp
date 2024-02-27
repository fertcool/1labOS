#include <iostream>
#include <vector>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
using namespace std;

//ф-я поиска названия последней поддиректории в пути
char* getlastfolder(char* dir //путь состоящий только из директорий
                   )
{
    char* cursymbol = dir;
    char* curslash = dir;
    //проходим по всем символам пути сохраняя позицию последнего слеша
    while(*cursymbol != '\0')
    {
        if(*cursymbol == '/')
        {
            curslash = cursymbol;
        }
        cursymbol++;
    }
    // если dir состоит из 1 папки - ее и возвращаем
    if (curslash == dir)
        return dir;

    //возвращаем указатель на 1 символ имени папки    
    return ++curslash;
}

// ф-я создания отсутствующих директорий 
void makedirs(char* file // полный путь к файлу
             )
{
    char* cursymbol = file; // итератор по символам пути
    
    // CХЕМА ПРОХОДА
    //   FOLDER1     /   FOLDER2   /.../   FOLDERN   /FILE
    //              /\            /\                /\
    //              ||            ||                ||
    //         1 ПРОВЕРКА         ||                ||
    //                            ||                ||
    //                        2 ПРОВЕРКА            ||
    //                                       ...    || 
    //                                              ||  
    //                                          N ПРОВЕРКА
    //

    // цикл посимвольного прохода по пути
    while(*cursymbol != '\0')
    {
        // если встречаем слеш - то проверяем есть ли такая директори
        if (*cursymbol == '/')
        {
            *cursymbol = '\0'; //добавляем чтобы продолжение пути не было видно

            //если ошибка открытия директории - то создаем ее со всеми правами доступа
            if(opendir(file) == NULL)
            {
                mkdir(file, S_IRWXU | S_IRWXG | S_IRWXO);
            }
            *cursymbol = '/';
        }
        // выходим если встречаем точку расширения файла (не директорию)
        if(*cursymbol == '\.' && *(cursymbol+sizeof(char)) != '/' && *(cursymbol+sizeof(char)) != '.')
        {
            return;
        }
        ++cursymbol;
    }
}

//ф-я поиска всех файлов в заданной директории
bool GetFiles(char* dir, //текущая директория поиска
              vector<char*>& files, // заполняемый вектор путей к файлам
              bool firstin = true // флаг первого входа в ф-ю
              )
{

    //            СТРУКТУРА ПУТЕЙ
    // ARCHIVE_DIR/SUB_DIR_1/.../SUB_DIR_N/FILE1
    // ...
    // ARCHIVE_DIR/SUB_DIR_1/FILE1
    // ...
    // ARCHIVE_DIR/FILE1
    // ...
    // ARCHIVE_DIR/FILEN
    // 

    DIR* dp; //указатель на поток директории
    struct dirent* entry; // указатель на файл директории
    struct stat statbuf; //буфер для просмотра метаинформации о файлах
    
    //проверка
    if ((dp = opendir(dir)) == NULL)
    {
        cout<<"Cannot open directory: "<< dir<<endl;
        return false;
    }

    // первый вход 
    // нужен для настройи рабочей директории - т.к. из нее мы будем работать с файлами со структурой ВЫШЕ)
    if (firstin)
    {
        chdir(dir);

        dir = getlastfolder(dir); //обрезаем путь до последней папки (т.е. архивируемой папки)
        firstin = false;
        
        chdir(".."); // теперь мы в родительской директории архивируемой папки 
        //и сможем правильно работать с ф-ми lstat 
    }
    // цикл по всем файлами текущей директории 
    while((entry = readdir(dp)) != NULL)
    {
        //создание полного пути файла со структурой ВЫШЕ
        char* fullentry = new char[strlen(dir)+strlen(entry->d_name)+2];
        strcpy(fullentry, dir);
        strcat(fullentry, "/");
        strcat(fullentry, entry->d_name);


        lstat(fullentry, &statbuf); //сбор метаданных

        // если файл - директория, то ищем файлы и в ней
        if(S_ISDIR(statbuf.st_mode))
        {
            // не учитываем ссылки ".", "..""
            if (strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0)
            {
                delete[] fullentry;
                continue;
            }

            GetFiles(fullentry, files, firstin);
            delete[] fullentry;
        }
        else// добавляем файл в вектор
        {
            files.push_back(fullentry);
        }
        
    }
    closedir(dp);

    return true;
}

//ф-я архивации выбранной папки
void archivate(char* dir, //путь к архивируемой папке
               char* archivename //имя архива (или путь)
              )
{
    vector<char*> files; //вектор с путями к файлам
    vector<int> filesizes; //вектор с размерами этих файлов

    //находим все файлы в заданной папке (включая поддиректории)
    if(!GetFiles(dir, files))
        return;


    //            СТРУКТУРА АРХИВА 
    // | КОЛИЧЕСТВО_ФАЙЛОВ(N)                 |\
    // | ПУТЬ_К_1_ФАЙЛУ     РАЗМЕР(В БАЙТАХ)  | | ЗАГОЛОВОК АРХИВА
    // |                ...                   | |
    // | ПУТЬ_К_N_ФАЙЛУ     РАЗМЕР(В БАЙТАХ)  |/
    // | ДАННЫЕ_1_ФАЙЛА ... ДАННЫЕ_N_ФАЙЛА EOF|
    //

    FILE* out = fopen(archivename, "wb"); //создаем файл архива
    fprintf(out, "%d\n", files.size()); //записываем в него количество файлов

    //цикл записи данных о файлах в заголовк архива
    for (auto file: files)
    {
        FILE* in = fopen(file, "rb");//открытие файла
        int filesize = 0; //размер файла

        //цикл вычисления размера файла
        while(fgetc(in) != EOF)
            ++filesize;

        filesizes.push_back(filesize);// добавляем размер текущего файла в вектор 

        fprintf(out, "%s %d\n", file, filesize); // записваем строку с данными файла в заголовок
        
        fclose(in);
    }

    // цикл записи данных файлов в архив
    for (int i = 0; i < files.size(); ++i)
    {
        FILE* in = fopen(files[i], "rb"); //открытие файла
        
        // цикл копирования каждого байта данных из текущего файла в архив
        for (int j = 0; j < filesizes[i]; ++j)
            fputc(fgetc(in), out);
        
        fclose(in);
    }
    
    fclose(out);

    //чистим строки с путями к файлам (т.к. больше их не используем)
    for (auto file : files)
    {
        delete[] file;
    }
}

//ф-я распаковки
void dearchivate(char* archdir, //путь к архиву
                 char* dearchdir // путь распаковки
                )
{
    int countfiles; //количество файлов
    vector<pair<char*, int>> files; //вектор файлов (имя и размер (в байтах))

    FILE* in = fopen(archdir, "rb"); //открытие файла архива
    fscanf(in, "%d\n", &countfiles); //считывание количества файлов

    files.resize(countfiles);//выделение памяти для вектора

    //цикл заполнения вектора файлов данными
    for(int i = 0; i < countfiles; ++i)
    {
        files[i].first = new char[128]; //128 байт - столько мы выделяем памяти под строку с путем к файлу 
        
        //формируем путь - dearchpath + '/' + filepath_in_archive
        // добавляем dearchpath
        strcpy(files[i].first, dearchdir);
        strcat(files[i].first, "/");
        char* filepath = files[i].first + strlen(dearchdir)+1;//указатель для записи filepath_in_archive
        fscanf(in, "%s%d\n", filepath, &files[i].second); //запись filepath_in_archive
    }

    //цикл создания файлов из архива 
    for(auto file: files)
    {
        FILE* out;
        //если не смогли создать файл (т.е. нет нужных дерикторий и поддерикторий) 
        // - то создаем необходимые директории 
        if((out = fopen(file.first, "wb")) == NULL)
        {
            makedirs(file.first);
            out = fopen(file.first, "wb");
        }
        char* buf = new char[file.second]; //буфер для копирования данных
        fread(buf, file.second, 1, in); //считыванеи
        fwrite(buf, file.second, 1, out); //записываем

        fclose(out);

        //чистим буфер
        delete[] buf;
    }

    //чистим строки с названиями файлов (т.к. больше они не используются)
    for (auto file : files)
    {
        delete[] file.first;
    }
    
}
int main(int argc, char* argv[])
{                            //            |||           Архивация             |||                 Распаковка
    char* archdir = argv[1]; // 1 аргумент ||| путь к папке архивации          ||| путь к архиву
    char* command = argv[2]; // 2 аргумент ||| комманда: "-a" (архивирование)  ||| комманда: "-da" (распаковка)
    char* archname = argv[3];// 3 аргумент ||| имя (путь) создаваемого архива  ||| путь где будет храниться распаковываемая папка

    //запуск комманды
    if (strcmp(command, "-a") == 0)
        archivate(archdir, archname);
    else if (strcmp(command, "-da") == 0)
        dearchivate(archdir, archname);
    else
    {
        cout<< "Wrong command!";
    }
    return 0;
}