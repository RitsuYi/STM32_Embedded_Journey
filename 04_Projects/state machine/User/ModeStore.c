#include "stm32f10x.h"
#include "stm32f10x_flash.h"
#include "ModeStore.h"

#define MODE_STORE_PAGE_ADDRESS		((uint32_t)0x0800FC00)
#define MODE_STORE_PAGE_END_ADDRESS	((uint32_t)0x08010000)
#define MODE_STORE_EMPTY_RECORD		((uint16_t)0xFFFF)
#define MODE_STORE_PAGE_HEADER		((uint16_t)0x5AA5)
#define MODE_STORE_MAGIC			((uint16_t)0xA500)

static uint8_t ModeStore_LastMode;

static uint8_t ModeStore_IsValidMode(uint8_t Mode)
{
	return (Mode >= MODE_STORE_MIN_MODE && Mode <= MODE_STORE_MAX_MODE) ? 1 : 0;
}

static uint8_t ModeStore_IsPageReady(void)
{
	return (*(__IO uint16_t *)MODE_STORE_PAGE_ADDRESS == MODE_STORE_PAGE_HEADER) ? 1 : 0;
}

static uint16_t ModeStore_Encode(uint8_t Mode)
{
	return MODE_STORE_MAGIC | ((((uint16_t)(~Mode)) & 0x0F) << 4) | (Mode & 0x0F);
}

static uint8_t ModeStore_Decode(uint16_t Record, uint8_t *Mode)
{
	uint8_t Value;
	uint8_t Check;
	
	if ((Record & 0xFF00) != MODE_STORE_MAGIC)
	{
		return 0;
	}
	
	Value = Record & 0x0F;
	Check = (Record >> 4) & 0x0F;
	if ((Value ^ Check) != 0x0F)
	{
		return 0;
	}
	if (!ModeStore_IsValidMode(Value))
	{
		return 0;
	}
	
	*Mode = Value;
	return 1;
}

static uint32_t ModeStore_FindEmptyAddress(void)
{
	uint32_t Address;
	
	if (!ModeStore_IsPageReady())
	{
		return 0;
	}
	
	for (Address = MODE_STORE_PAGE_ADDRESS + 2; Address < MODE_STORE_PAGE_END_ADDRESS; Address += 2)
	{
		if (*(__IO uint16_t *)Address == MODE_STORE_EMPTY_RECORD)
		{
			return Address;
		}
	}
	
	return 0;
}

uint8_t ModeStore_Load(uint8_t DefaultMode)
{
	uint32_t Address;
	uint16_t Record;
	uint8_t Mode;
	
	if (!ModeStore_IsValidMode(DefaultMode))
	{
		DefaultMode = MODE_STORE_DEFAULT_MODE;
	}
	Mode = DefaultMode;
	
	if (!ModeStore_IsPageReady())
	{
		ModeStore_LastMode = Mode;
		return Mode;
	}
	
	for (Address = MODE_STORE_PAGE_ADDRESS + 2; Address < MODE_STORE_PAGE_END_ADDRESS; Address += 2)
	{
		Record = *(__IO uint16_t *)Address;
		if (Record == MODE_STORE_EMPTY_RECORD)
		{
			break;
		}
		ModeStore_Decode(Record, &Mode);
	}
	
	ModeStore_LastMode = Mode;
	return Mode;
}

uint8_t ModeStore_Save(uint8_t Mode)
{
	uint32_t Address;
	uint32_t Primask;
	uint16_t Record;
	uint8_t StoredMode;
	uint8_t PageReady;
	FLASH_Status Status;
	
	if (!ModeStore_IsValidMode(Mode))
	{
		return 0;
	}
	if (Mode == ModeStore_LastMode)
	{
		return 1;
	}
	
	Address = ModeStore_FindEmptyAddress();
	Record = ModeStore_Encode(Mode);
	PageReady = ModeStore_IsPageReady();
	Primask = __get_PRIMASK();
	
	__disable_irq();
	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
	
	if (!PageReady || Address == 0)
	{
		Status = FLASH_ErasePage(MODE_STORE_PAGE_ADDRESS);
		if (Status == FLASH_COMPLETE)
		{
			Status = FLASH_ProgramHalfWord(MODE_STORE_PAGE_ADDRESS, MODE_STORE_PAGE_HEADER);
			Address = MODE_STORE_PAGE_ADDRESS + 2;
		}
	}
	else
	{
		Status = FLASH_COMPLETE;
	}
	
	if (Status == FLASH_COMPLETE)
	{
		Status = FLASH_ProgramHalfWord(Address, Record);
	}
	
	FLASH_Lock();
	if (Primask == 0)
	{
		__enable_irq();
	}
	
	if (Status != FLASH_COMPLETE)
	{
		return 0;
	}
	if (!ModeStore_Decode(*(__IO uint16_t *)Address, &StoredMode))
	{
		return 0;
	}
	
	ModeStore_LastMode = StoredMode;
	return 1;
}
