// pdb_lookup.cpp : Defines the entry point for the console application.
//
#include <Windows.h>
#include <tchar.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <stdint.h>
#include <boost\format.hpp>

#define CV_SIGNATURE_RSDS   'SDSR'
typedef struct CV_INFO_PDB70 
{
	DWORD      CvSignature; 
	GUID       Signature;       // unique identifier 
	DWORD      Age;             // an always-incrementing value 
	BYTE       PdbFileName[1];  // zero terminated string with the name of the PDB file 
} * PCV_INFO_PDB70;

int _tmain(int argc, _TCHAR* argv[])
{
	if (argc != 2)
	{
		std::cerr << "Using pdb_lookup.exe <file>" << std::endl;
		return -1;
	}

	std::ifstream f (argv[1], std::ios::binary);
	f.seekg(0, std::ios::end);
	size_t file_size = static_cast<size_t>(f.tellg());
	f.seekg(0, std::ios::beg);

	std::vector<uint8_t> buff(file_size);
	f.read(reinterpret_cast<char*>(&buff[0]), file_size);

	std::string pdb_guid;
	std::string pdb_filename;
	std::string time_stamp;
	std::string img_size;

	PIMAGE_DOS_HEADER pDosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(&buff[0]);
	if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE)
	{
		std::cerr << "Error: Image hasn't DOS signature" << std::endl;
		return -1;
	}

	PIMAGE_NT_HEADERS32  pNtHeader32 = reinterpret_cast<PIMAGE_NT_HEADERS32>(&buff[0] + pDosHeader->e_lfanew);
	PIMAGE_NT_HEADERS64  pNtHeader64 = reinterpret_cast<PIMAGE_NT_HEADERS64>(&buff[0] + pDosHeader->e_lfanew);

	if( pNtHeader32->Signature != IMAGE_NT_SIGNATURE )
	{
		std::cerr << "Error: Image hasn't NT signature" << std::endl;
		return -1;
	}
	bool is_x64 = pNtHeader32->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC;

	time_stamp = (boost::format("%X") % pNtHeader32->FileHeader.TimeDateStamp).str();
	img_size = (boost::format("%X") % (is_x64?pNtHeader64->OptionalHeader.SizeOfImage : pNtHeader32->OptionalHeader.SizeOfImage ) ).str();
	

	PIMAGE_DATA_DIRECTORY pDataDic = is_x64 ? &pNtHeader64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG]
			                                : &pNtHeader32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];

	if( pDataDic && pDataDic->Size > 0 )
	{
		PIMAGE_SECTION_HEADER pSection = reinterpret_cast<PIMAGE_SECTION_HEADER>(reinterpret_cast<uint8_t*>(pNtHeader32) + (is_x64 ? sizeof(IMAGE_NT_HEADERS64) : sizeof(IMAGE_NT_HEADERS32)));

		PIMAGE_DEBUG_DIRECTORY pDebugDic = nullptr;
		for( int i = 0; i < pNtHeader32->FileHeader.NumberOfSections; i++, pSection++)
		{
			if( pSection->VirtualAddress <= pDataDic->VirtualAddress && pDataDic->VirtualAddress < pSection->VirtualAddress + pSection->SizeOfRawData)
			{
				pDebugDic = reinterpret_cast<PIMAGE_DEBUG_DIRECTORY>(&buff[0] + pSection->PointerToRawData + (pDataDic->VirtualAddress - pSection->VirtualAddress));
				break;
			}
		}
				
		if (pDebugDic != nullptr )
		{

			int nNumberOfEntries = pDataDic->Size / sizeof(IMAGE_DEBUG_DIRECTORY);
			for( int i = 0; i < nNumberOfEntries; i++, pDebugDic++)
			{
				if( pDebugDic->Type == IMAGE_DEBUG_TYPE_CODEVIEW )
				{
					PCV_INFO_PDB70 pCvInfo = reinterpret_cast<PCV_INFO_PDB70>(&buff[0] + pDebugDic->PointerToRawData); 
					if( pCvInfo->CvSignature == CV_SIGNATURE_RSDS  )
					{
						pdb_filename = reinterpret_cast<LPCSTR>(pCvInfo->PdbFileName);

						pdb_guid = (boost::format("%X%X%X%X%X%X%X%X%X%X%X%d")
							% pCvInfo->Signature.Data1
							% pCvInfo->Signature.Data2
							% pCvInfo->Signature.Data3
							% static_cast<int16_t>(pCvInfo->Signature.Data4[0])
							% static_cast<int16_t>(pCvInfo->Signature.Data4[1])
							% static_cast<int16_t>(pCvInfo->Signature.Data4[2])
							% static_cast<int16_t>(pCvInfo->Signature.Data4[3])
							% static_cast<int16_t>(pCvInfo->Signature.Data4[4])
							% static_cast<int16_t>(pCvInfo->Signature.Data4[5])
							% static_cast<int16_t>(pCvInfo->Signature.Data4[6])
							% static_cast<int16_t>(pCvInfo->Signature.Data4[7])
							% pCvInfo->Age).str();
						break;
					}
				}
			}
		}
	}

	std::wcout << L"file name   : " << argv[1] << std::endl;
	std::cout << "pdb file    : " << pdb_filename << std::endl;
	std::cout << "pdb guid    : " << pdb_guid << std::endl;
	std::cout << "time stamp  : " << time_stamp << std::endl;
	std::cout << "size of img : " << img_size << std::endl;
	return 0;
}

