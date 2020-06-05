#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <dirent.h>
#include <wait.h>
#include <sys/stat.h>

#define BMP_SOURCE_DIRECTORY "./BMPSource"
#define BMP_DESTINATION_DIRECTORY "./BMPDestination"

#define PIPE_READ_INDEX 0
#define PIPE_WRITE_INDEX 1

#define RED 0
#define GREEN 1
#define BLUE 2

int getBMPDataOffset(char *imageData)
{
    return *((int *)&imageData[0x0A]);
}

int getBMPWidth(char *imageData)
{
    return *((int *)&imageData[0x12]);
}

int getBMPHeight(char *imageData)
{
    return *((int *)&imageData[0x16]);
}

void leaveBMP24Color(char *imageData, int colorToLeave, char replacement)
{
    int dataOffset = getBMPDataOffset(imageData);
    printf("Смещение: %d\n", dataOffset);
    int pictureWidth = getBMPWidth(imageData);
    printf("Ширина: %d\n", pictureWidth);
    int pictureHeight = getBMPHeight(imageData);
    printf("Длина: %d\n", pictureHeight);
    while (pictureWidth % 4)
    {
        pictureWidth++;
    }
    long bytesToReplace = pictureHeight * pictureWidth;
    for (long i = dataOffset; i < bytesToReplace * 3 + dataOffset; i += 3)
    {
        switch (colorToLeave)
        {
        case RED:
            imageData[i] = replacement;
            imageData[i + 1] = replacement;
            break;
        case BLUE:
            imageData[i + 1] = replacement;
            imageData[i + 2] = replacement;
            break;
        case GREEN:
            imageData[i] = replacement;
            imageData[i + 2] = replacement;
            break;
        }
    }
}

int getFileSizeByName(char *fileName)
{
    FILE *fileHandle = fopen(fileName, "r");
    if (fileHandle)
    {
        fseek(fileHandle, 0L, SEEK_END);
        int fileSize = ftell(fileHandle);
        fclose(fileHandle);
        return fileSize;
    }
    else
    {
        printf("Размер не может быть определён.\n");
        return -1;
    }
}

int getFilesNumber(char *dirPath)
{
    int dirFiles = 0;
    DIR *directory = opendir(dirPath);
    if (directory)
    {
        while (readdir(directory))
        {
            dirFiles++;
        }
        closedir(directory);
        return dirFiles;
    }
    else
    {
        printf("Не удалось открыть каталог.");
    }
    return 0;
}

char **newFileNamesArray(int filesInDirectory)
{
    char **filenamesArray;
    filenamesArray = (char **)calloc(sizeof(char *), filesInDirectory);
    for (int i = 0; i < filesInDirectory; i++)
    {
        filenamesArray[i] = (char *)calloc(sizeof(char), 256);
    }
    return filenamesArray;
}

char **getBMPPathsByDirectory(char *directoryPath, int *filesNumberPtr)
{
    struct dirent *sourceDirStruct;
    DIR *sourceDir = opendir(directoryPath);
    if (sourceDir)
    {
        char **sourceDirFiles = newFileNamesArray(getFilesNumber(directoryPath) - 2);
        *filesNumberPtr = 0;
        while ((sourceDirStruct = readdir(sourceDir)) != NULL)
        {
            if (strstr(sourceDirStruct->d_name, "bmp"))
            {
                strcpy(sourceDirFiles[*filesNumberPtr], directoryPath);
                strcat(sourceDirFiles[*filesNumberPtr], "/");
                strcat(sourceDirFiles[*filesNumberPtr], sourceDirStruct->d_name);
                (*filesNumberPtr)++;
            }
        }
        closedir(sourceDir);
        return sourceDirFiles;
    }
    else
    {
        printf("Неверный путь.\n");
    }
    (*filesNumberPtr) = -1;
    return NULL;
}

int main(int argc, char *argv[])
{
    int filesToEdit;
    char **bmpPaths = getBMPPathsByDirectory(BMP_SOURCE_DIRECTORY, &filesToEdit);

    if (bmpPaths != NULL)
    {
        for (int i = 0; i < filesToEdit; i++)
        {
            printf("Текущий файл: %s\n", bmpPaths[i]);

            int pipeHandles[2];
            if (pipe(pipeHandles) == -1)
            {
                printf("Ошибка создания pipe.\n");
                exit(EXIT_FAILURE);
            }

            pid_t colorLeavingProcess = fork();
            if (colorLeavingProcess == -1)
            {
                printf("Ошибка создания процесса.\n");
                exit(EXIT_FAILURE);
            }

            if (colorLeavingProcess == 0)
            {
                close(pipeHandles[PIPE_WRITE_INDEX]);
                int firstIteration = 1;
                int bytesToRead = sizeof(int);
                char *fileContent;
                int fileSize;
                char *placeToPastePointer = (char *)&fileSize;
                while (read(pipeHandles[PIPE_READ_INDEX], placeToPastePointer, bytesToRead))
                {
                    if (firstIteration)
                    {
                        firstIteration = 0;
                        printf("Размер файла: %d\n", fileSize);
                        fileContent = (char *)malloc(fileSize);
                        placeToPastePointer = fileContent;
                        bytesToRead = 1;
                    }
                    else
                    {
                        placeToPastePointer++;
                    }
                }
                leaveBMP24Color(fileContent, BLUE, 0x00);

                char savePath[256] = {0};
                strcpy(savePath, BMP_DESTINATION_DIRECTORY);
                strcat(savePath, "/");
                sprintf(&savePath[strlen(savePath)], "%d", i);
                strcat(savePath, ".bmp");

                printf("Сохранённый файл: %s\n", savePath);

                FILE *updatedFile = fopen(savePath, "w");
                fwrite(fileContent, fileSize, 1, updatedFile);
                fclose(updatedFile);

                free(fileContent);
                close(pipeHandles[PIPE_READ_INDEX]);
                _exit(EXIT_SUCCESS);
            }
            else
            {
                close(pipeHandles[PIPE_READ_INDEX]);
                int fileSize = getFileSizeByName(bmpPaths[i]);
                FILE *fileHandle = fopen(bmpPaths[i], "r");
                if (fileHandle)
                {
                    write(pipeHandles[PIPE_WRITE_INDEX], &fileSize, sizeof(int));
                    char *fileContent = malloc(fileSize);
                    fread(fileContent, fileSize, 1, fileHandle);
                    write(pipeHandles[PIPE_WRITE_INDEX], fileContent, fileSize);
                    close(pipeHandles[PIPE_WRITE_INDEX]);
                    fclose(fileHandle);
                    free(fileContent);
                }
                else
                {
                    printf("Ошибка открытия файла.");
                }
                wait(NULL);
            }
        }
    }
}