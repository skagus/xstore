#include <stdint.h>
#include <stdio.h>

#include "esp_err.h"
#include "esp_spiffs.h"
#include "esp_vfs_dev.h"

#include "cli.h"
#include "dos.h"

#define MAX_PATH_LEN		(64)


void _fs_info(uint8_t argc,char* argv[])
{
	size_t total = 0,used = 0;
	int ret = esp_spiffs_info(NULL,&total,&used);
	if(ret == ESP_OK)
	{
		printf("Partition size: total: %d, used: %d",total,used);
	}
	else
	{
		printf("esp_spiffs_info error\n");
	}
}

void _fs_write(uint8_t argc,char* argv[])
{
	if(argc < 2)
	{
		printf("needs file name\n");
		return;
	}
	char szPath[MAX_PATH_LEN];
	sprintf(szPath,"/sf/%s",argv[1]);

	// Use POSIX and C standard library functions to work with files.
	// First create a file.
	FILE* f = fopen(szPath,"w");
	if(NULL != f)
	{
		fprintf(f,"Hello World!\n");
		fclose(f);
	}
	else
	{
		printf("Failed to open: %s\n", szPath);
	}
}

void _fs_list(uint8_t argc,char* argv[])
{
	DIR* dp;
	struct dirent* ep;
	dp = opendir("/sf/");
	if(dp != NULL)
	{
		while((ep = readdir(dp)) != NULL)
		{
			printf("%s %d\n",ep->d_name,ep->d_type);
		}
		closedir(dp);
	}
}

void _fs_delete(uint8_t argc,char* argv[])
{
	if(argc < 2)
	{
		printf("needs file name\n");
		return;
	}
	char szPath[MAX_PATH_LEN];
	sprintf(szPath,"/sf/%s",argv[1]);
	remove(szPath);
}

void _fs_read(uint8_t argc,char* argv[])
{
	if(argc < 2)
	{
		printf("needs file name\n");
		return;
	}
	char szPath[MAX_PATH_LEN];
	sprintf(szPath,"/sf/%s",argv[1]);
	printf("Open: %s\n",szPath);

	FILE* pFile = fopen(szPath,"r");

	if(NULL != pFile)
	{
		uint8_t aBuf[32];
		int nBytes;

		while(1)
		{
			nBytes = fread(aBuf,1,32,pFile);
			if(nBytes <= 0)
			{
				break;
			}
			for(uint8_t nIdx = 0; nIdx < nBytes; nIdx++)
			{
				printf("%c",aBuf[nIdx]);
			}
		}
		fclose(pFile);
	}
	else
	{
		printf("Error while open: %s\n", szPath);
	}
}


void _fs_dump(uint8_t argc,char* argv[])
{
	if(argc < 2)
	{
		printf("needs file name\n");
		return;
	}
	char szPath[MAX_PATH_LEN];
	sprintf(szPath, "/sf/%s", argv[1]);
	printf("Open: %s\n", szPath);

	FILE* pFile = fopen(szPath,"r");

	if(NULL != pFile)
	{
		uint8_t aBuf[32];
		int nBytes;

		while(1)
		{
			nBytes = fread(aBuf,1,32,pFile);
			if(nBytes <= 0)
			{
				break;
			}
			for(uint8_t nIdx = 0; nIdx < nBytes; nIdx++)
			{
				printf("%2X ",aBuf[nIdx]);
			}
			printf("\n");
		}
		fclose(pFile);
	}
	else
	{
		printf("Error while open: %s\n",szPath);
	}
}

esp_err_t DOS_Init()
{
	esp_vfs_spiffs_conf_t conf =
	{
		.base_path = "/sf",
		.partition_label = NULL,
		.max_files = 5,
		.format_if_mount_failed = true
	};

	esp_err_t ret = esp_vfs_spiffs_register(&conf);
	if(ESP_OK == ret)
	{
		CLI_Register("fs_info",_fs_info);
		CLI_Register("fs_read",_fs_read);
		CLI_Register("fs_dump",_fs_dump);
		CLI_Register("fs_write",_fs_write);
		CLI_Register("fs_list",_fs_list);
		CLI_Register("fs_delete",_fs_delete);
	}
	return ret;
}