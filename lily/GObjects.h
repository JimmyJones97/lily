#pragma once
#include <Windows.h>

#include "common/ida_defs.h"
#include "pubg_process.h"
#include "pubg_struct.h"

class TNameEntryArray;

class UObject
{
private:
	uint8_t pad[0x40];
public:
	//DWORD64 GetClass() {
	//	__int64 a1 = (__int64)this;
	//	__int64 v2; // rax

	//	v2 = __ROL8__(*(_QWORD*)(a1 + 0x18) ^ 0x7753CBD5413FEAD7i64, 0x17);
	//	return v2 ^ (v2 << 0x20) ^ 0xA7950E40FAB9478i64;
	//}

	FName GetFName() const {
		__int64 a1 = (__int64)this;
		int v1;
		int v2;
		int v3;
		__int64 v5;
		__int64 v75;

		v1 = *(_DWORD*)(a1 + 0x34) ^ 0x1A31B5BC;
		LODWORD(v5) = __ROL4__(*(_DWORD*)(a1 + 0x30) ^ 0x894853B, 2);
		v2 = int(v5 ^ ((_DWORD)v5 << 0x10) ^ 0xAF8DB5BC);
		LODWORD(v75) = v2;
		v3 = __ROR4__(v1, 6);
		HIDWORD(v75) = v3 ^ (v3 << 0x10) ^ 0x8DAF853B;

		FName name;
		name.ComparisonIndex = LODWORD(v75);
		name.Number = HIDWORD(v75);

		return name;
	}

	//DWORD GetIndex() {
	//	__int64 a2 = (__int64)this;
	//	int v2;

	//	v2 = __ROL4__(*(_DWORD*)(a2 + 8) ^ 0x45D2C49B, 9);
	//	return v2 ^ (v2 << 0x10) ^ 0x5C80CBD3;
	//}

	//DWORD GetObjectFlags() {
	//	unsigned __int64 v2 = (unsigned __int64)this;
	//	int v3;

	//	v3 = __ROR4__(*(_DWORD*)(v2 + 0x30) ^ 0xD1911304, 8);
	//	return (v3 ^ (v3 << 0x10) ^ 0x354055CB);
	//}

	//DWORD64 GetOuter() {
	//	unsigned __int64 v2 = (unsigned __int64)this;
	//	__int64 v4;

	//	v4 = __ROR8__(*(_QWORD*)(v2 + 0x28) ^ 0x1555DE0E8CFD7EE3i64, 3);
	//	v2 = v4 ^ (v4 << 0x20) ^ 0x3811FD43E3F5A7C4i64;
	//	return v2;
	//}

	unsigned GetNameHash() const { return g_Pubg->NameArr.GetNameHashByID(GetFName()); }
	bool GetName(char* szBuf, size_t SizeMax) const { return g_Pubg->NameArr.GetNameByID(GetFName(), szBuf, SizeMax); }
	std::string GetName() const {
		char szBuf[0x100];
		if (!GetName(szBuf, sizeof(szBuf)))
			return {};
		return szBuf;
	}
};

class FUObjectItem
{
private:	uint8_t Pad1[0x8];
public:		uintptr_t ObjectPtr;
private:	uint8_t Pad2[0x20];
};

class FUObjectArray
{
private:
	static constexpr uintptr_t ADDRESS_GOBJECTS = 0x8F70468;

	uintptr_t BaseAddress;
	uint32_t NumElements;

public:
	FUObjectArray() {
		EncryptedPtr<uintptr_t> P;
		g_Pubg->ReadBase(ADDRESS_GOBJECTS + 0x18, &P);
		BaseAddress = P;
		g_Pubg->ReadBase(ADDRESS_GOBJECTS + 0x28, &NumElements);
	}

	NativePtr<UObject> GetNativePtrById(size_t Index) const {
		if (Index >= NumElements)
			return 0;

		FUObjectItem ObjectItem;
		if (g_Pubg->Read(BaseAddress + Index * sizeof(ObjectItem), &ObjectItem) == 0)
			return 0;

		return ObjectItem.ObjectPtr;
	}

	size_t GetNumElements() const { return NumElements; }
	void DumpObject(const TNameEntryArray& NameArr) const;
};