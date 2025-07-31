#include <switch.h>
#define FREEZE_DIC_LENGTH 255

typedef struct {
	char state;
	u64 address;
	u8* vData;
	u64 size;
	u64 titleId;
} FreezeBlock;

extern FreezeBlock* freezes;

void initFreezes(void);
void freeFreezes(void);
int findAddrSlot(u64 addr);
int findNextEmptySlot();
int addToFreezeMap(u64 addr, u8* v_data, u64 v_size, u64 tid);
int removeFromFreezeMap(u64 addr);
int getFreezeCount();
u8 clearFreezes(void);

void freezeShort(u64 addr, s16 val);
void freezeInt(u64 addr, s32 val);
void freezeLongLong(u64 addr, s64 val);
void freezeFloat(u64 addr, float val);
void freezeDouble(u64 addr, double val);
void freezeLongDouble(u64 addr, long double val);