#include <stdint.h>
#include <stdio.h>

#include "esp_err.h"
#include "esp_spiffs.h"
#include "esp_vfs_dev.h"

#include "uart.h"
#include "cli.h"
#include "dos.h"

#define MAX_PATH_LEN		(260)

#define FS_LABEL			"storage"

void _fs_format(uint8_t argc,char* argv[])
{
	printf("It takes a few seconds / minutes\n");
	esp_spiffs_format(FS_LABEL);
}

void _fs_info(uint8_t argc,char* argv[])
{
	size_t total = 0,used = 0;
	int ret = esp_spiffs_info(FS_LABEL,&total,&used);
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
	printf("This is 'copy con'\n");
	char szPath[MAX_PATH_LEN];
	sprintf(szPath,"/sf/%s",argv[1]);

	char nRcv;
	// Use POSIX and C standard library functions to work with files.
	// First create a file.
	FILE* f = fopen(szPath,"w");

	if(NULL != f)
	{
		while(1)
		{
			if(UART_RxD(&nRcv))
			{
				if(0x1A == nRcv) // Ctrl-Z
				{
					break;
				}
				printf("%c",nRcv);
				fputc(nRcv, f);
			}
		}

		fclose(f);
	}
	else
	{
		printf("Failed to open: %s\n", szPath);
	}
}

void _fs_list(uint8_t argc,char* argv[])
{
	DIR* pDir = opendir("/sf/");
	struct dirent* pEntry;
	struct stat st;
	if(pDir != NULL)
	{
		char szPath[MAX_PATH_LEN];
		while((pEntry = readdir(pDir)) != NULL)
		{
			snprintf(szPath,MAX_PATH_LEN,"/sf/%s",(char*)(pEntry->d_name));
			stat(szPath, &st);
			printf("%s (%ld bytes)\n",pEntry->d_name,st.st_size);
		}
		closedir(pDir);
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


#define SIZE_DUMP_BUF		(32)
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
		uint8_t aBuf[SIZE_DUMP_BUF];
		int nBytes;
		uint32_t nCnt = 0;

		while(1)
		{
			nBytes = fread(aBuf,1,SIZE_DUMP_BUF,pFile);
			if(nBytes <= 0)
			{
				break;
			}
			printf(" %4X : ",nCnt * SIZE_DUMP_BUF);
			for(uint8_t nIdx = 0; nIdx < nBytes; nIdx++)
			{
				printf("%02X ",aBuf[nIdx]);
			}
			nCnt++;
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
		.partition_label = FS_LABEL,
		.max_files = 5,
		.format_if_mount_failed = true
	};

	esp_err_t ret = esp_vfs_spiffs_register(&conf);
	if(ESP_OK == ret)
	{
		CLI_Register("fs_info",_fs_info);
		CLI_Register("fs_format",_fs_format);
		CLI_Register("fs_read",_fs_read);
		CLI_Register("fs_dump",_fs_dump);
		CLI_Register("fs_write",_fs_write);
		CLI_Register("fs_list",_fs_list);
		CLI_Register("fs_delete",_fs_delete);
	}
	return ret;
}
