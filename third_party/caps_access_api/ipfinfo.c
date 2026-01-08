#include <stdio.h>
#include <caps/capsimage.h>

#ifdef __AMIGA__
#include <inline/capsimage.h>
#include <proto/exec.h>

struct MsgPort *msgport;
struct IORequest *ioreq;
struct Library *CapsImageBase;
#endif /* __AMIGA__ */

void print_info(char *name)
{
	int i, id = CAPSAddImage();

	if (CAPSLockImage(id, name) == imgeOk)
	{
		struct CapsImageInfo cii;

		if (CAPSGetImageInfo(&cii, id) == imgeOk)
		{
			printf("Type: %d\n", (int)cii.type);
			printf("Release: %d\n", (int)cii.release);
			printf("Revision: %d\n", (int)cii.revision);
			printf("Min Cylinder: %d\n", (int)cii.mincylinder);
			printf("Max Cylinder: %d\n", (int)cii.maxcylinder);
			printf("Min Head: %d\n", (int)cii.minhead);
			printf("Max Head: %d\n", (int)cii.maxhead);
			printf("Creation Date: %04d/%02d/%02d %02d:%02d:%02d.%03d\n", (int)cii.crdt.year, (int)cii.crdt.month, (int)cii.crdt.day, (int)cii.crdt.hour, (int)cii.crdt.min, (int)cii.crdt.sec, (int)cii.crdt.tick);
			printf("Platforms:");
			for (i = 0; i < CAPS_MAXPLATFORM; i++)
				if (cii.platform[i] != ciipNA)
					printf(" %s", CAPSGetPlatformName(cii.platform[i]));
			printf("\n");
		}
		CAPSUnlockImage(id);
	}
	CAPSRemImage(id);
}

int main(int argc, char **argv)
{
	if (argc == 2)
	{
#ifdef __AMIGA__
		if ((msgport = CreateMsgPort()))
		{
			if ((ioreq = CreateIORequest(msgport, sizeof(struct IORequest))))
			{
				if (!OpenDevice(CAPS_NAME, 0, ioreq, 0))
				{
					CapsImageBase = (struct Library *)ioreq->io_Device;
#endif /* __AMIGA__ */
					if (CAPSInit() == imgeOk)
					{
						print_info(argv[1]);
						CAPSExit();
					}
#ifdef __AMIGA__
					CloseDevice(ioreq);
				}
				DeleteIORequest(ioreq);
			}
			DeleteMsgPort(msgport);
		}
#endif /* __AMIGA__ */
	}
	else
		printf("Usage: %s FILE\n", argv[0]);

	return 0;
}

