#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "flash.h"
// 필요한 경우 헤더파일을 추가한다

int dd_read(int ppn, char *pagebuf);
int dd_write(int ppn, char *pagebuf);
int dd_erase(int pbn);

FILE *flashfp;	// fdevicedriver.c에서 사용

//
// 이 함수는 FTL의 역할 중 일부분을 수행하는데 물리적인 저장장치 flash memory에 Flash device driver를 이용하여 데이터를
// 읽고 쓰거나 블록을 소거하는 일을 한다 (동영상 강의를 참조).
// flash memory에 데이터를 읽고 쓰거나 소거하기 위해서 fdevicedriver.c에서 제공하는 인터페이스를
// 호출하면 된다. 이때 해당되는 인터페이스를 호출할 때 연산의 단위를 정확히 사용해야 한다.
// 읽기와 쓰기는 페이지 단위이며 소거는 블록 단위이다.
// 
int main(int argc, char *argv[])
{	

	char sectorbuf[SECTOR_SIZE];
	char sparebuf[SPARE_SIZE];
	char tmp_sectorbuf[SECTOR_SIZE];
	char tmp_sparebuf[SPARE_SIZE];
	char pagebuf[PAGE_SIZE];
	char free_block[BLOCK_SIZE];
	char *blockbuf = free_block;
	
	// flash memory 파일 생성: 위에서 선언한 flashfp를 사용하여 flash 파일을 생성한다. 그 이유는 fdevicedriver.c에서 
	//                 flashfp 파일포인터를 extern으로 선언하여 사용하기 때문이다.
	// 페이지 쓰기: pagebuf의 섹터와 스페어에 각각 입력된 데이터를 정확히 저장하고 난 후 해당 인터페이스를 호출한다
	// 페이지 읽기: pagebuf를 인자로 사용하여 해당 인터페이스를 호출하여 페이지를 읽어 온 후 여기서 섹터 데이터와
	//                  스페어 데이터를 분리해 낸다
	// memset(), memcpy() 등의 함수를 이용하면 편리하다. 물론, 다른 방법으로 해결해도 무방하다.


	char option = argv[1][0];
	char *fname = argv[2];
	int BLOCK_NUM;
	int ppn;
	char* sector_data;
	char* spare_data;
	int pbn;
	int fsize;
	int free_block_pbn = 0;


	if(option == 'c') {
		if ((flashfp = fopen(fname, "w+")) == NULL) {
			fprintf(stderr, "fopen error for %s\n", fname);
			exit(1);
		}

		BLOCK_NUM = atoi(argv[3]) * BLOCK_SIZE;

		for(int i=0; i<BLOCK_NUM; i++) {
			fputc((char)0xFF, flashfp);
		}

		fseek(flashfp, 0, SEEK_END);
		fclose(flashfp);
	}


	if(option == 'w') {
		if ((flashfp = fopen(fname, "r+")) == NULL) {
			fprintf(stderr, "fopen error for %s\n", fname);
			exit(1);
		}

		ppn = atoi(argv[3]);
		sector_data = argv[4];
		spare_data = argv[5];

		memset(pagebuf, (char)0xFF, PAGE_SIZE);
		dd_read(ppn, pagebuf);

		/* write 할 page가 비어있는 경우 */
		if(pagebuf[0]==(char)0xFF && pagebuf[SECTOR_SIZE]==(char)0xFF) {
			memcpy(pagebuf, sector_data, strlen(sector_data));
			memcpy(pagebuf+SECTOR_SIZE, spare_data, strlen(spare_data));

			dd_write(ppn, pagebuf);
		}

		/* In-place update 해야하는 경우 */
		else {
			pbn = ppn/4;

			/* free block 찾기 */
			fseek(flashfp, 0, SEEK_END);
			fsize = ftell(flashfp);
			BLOCK_NUM = fsize/BLOCK_SIZE;
			
			for(int i=0; i<BLOCK_NUM; i++) {
				int cnt=0;

				memset(blockbuf, (char)0xFF, BLOCK_SIZE);

				fseek(flashfp, BLOCK_SIZE*i, SEEK_SET);
				fread((void *)blockbuf, BLOCK_SIZE, 1, flashfp);

				for(int i=0; i<BLOCK_SIZE; i++) {
					if(blockbuf[i] != (char)0xFF) {
						cnt++;
						break;
					}
				}

				if(cnt==0) {
					free_block_pbn = i;
					break;
				}
			}


			/* free block에 유효한 data copy */
			for(int i=0; i<PAGE_NUM; i++) {
				if(4*pbn + i == ppn) {
					continue;
				}

				dd_read(4*pbn + i, pagebuf);
				dd_write(4*free_block_pbn + i, pagebuf);
			}

			dd_erase(pbn);


			memset(pagebuf, (char)0xFF, PAGE_SIZE);

			memcpy(pagebuf, sector_data, strlen(sector_data));
			memcpy(pagebuf+SECTOR_SIZE, spare_data, strlen(spare_data));

			dd_write(ppn, pagebuf);

			/* 다시 원래 자리로 block copy */
			for(int i=0; i<PAGE_NUM; i++) {
				if(4*pbn + i == ppn) {
					continue;
				}
				dd_read(4*free_block_pbn + i, pagebuf);
				dd_write(4*pbn + i, pagebuf);
			}

			dd_erase(free_block_pbn);
		}

		fclose(flashfp);
	}


	if(option == 'r') {
		if ((flashfp = fopen(fname, "r")) == NULL) {
			fprintf(stderr, "fopen error for %s\n", fname);
			exit(1);
		}

		ppn = atoi(argv[3]);

		dd_read(ppn, pagebuf);

		memset(tmp_sectorbuf, '\0', SECTOR_SIZE);
		memset(tmp_sparebuf, '\0', SPARE_SIZE);

		memset(sectorbuf, '\0', SECTOR_SIZE);
		memset(sparebuf, '\0', SPARE_SIZE);

		memcpy(tmp_sectorbuf, pagebuf, SECTOR_SIZE);
		memcpy(tmp_sparebuf, pagebuf+SECTOR_SIZE, SPARE_SIZE);

		for(int i=0; i<SECTOR_SIZE; i++) {
			if(tmp_sectorbuf[i] == (char)0xFF)
				break;

			sectorbuf[i] = tmp_sectorbuf[i];
		}

		for(int i=0; i<SPARE_SIZE; i++) {
			if(tmp_sparebuf[i] == (char)0xFF)
				break;

			sparebuf[i] = tmp_sparebuf[i];
		}

		if(tmp_sectorbuf[0] != (char)0xff && tmp_sparebuf[0] != (char)0xff)  {
			printf("%s %s\n", sectorbuf, sparebuf);
		}

		fclose(flashfp);
	}


	if(option == 'e') {
		if ((flashfp = fopen(fname, "r+")) == NULL) {
			fprintf(stderr, "fopen error for %s\n", fname);
			exit(1);
		}

		pbn = atoi(argv[3]);

		dd_erase(pbn);

		fclose(flashfp);
	}

	return 0;
}
