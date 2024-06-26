#include "Iop_NamcoNANDDevice.h"
#include <cassert>
#include <cstring>
#include "StringUtils.h"
#include "MemStream.h"

using namespace Iop;
using namespace Iop::Namco;

CNamcoNANDDevice::CNamcoNANDDevice(std::unique_ptr<Framework::CStream> nandDumpStream, uint32 rootSector)
    : m_nandDumpStream(std::move(nandDumpStream))
    , m_nandReader(*m_nandDumpStream.get(), rootSector)
{
}

Framework::CStream* CNamcoNANDDevice::GetFile(uint32 flags, const char* path)
{
	flags &= ~OPEN_FLAG_NOWAIT;
	assert(flags == OPEN_FLAG_RDONLY);

	if(path[0] == '/')
	{
		path++;
	}

	auto sections = StringUtils::Split(path, '/');

	::Namco::CSys147NANDReader::DIRENTRY currEntry = {};
	for(unsigned int i = 0; i < sections.size(); i++)
	{
		auto dir = m_nandReader.ReadDirectory(currEntry.sector);
		//Strangely, filesystem only saves 16 characters for entry name but some games
		//will request files with more characters (ex.: Umimonogatari)... Cut the excess.
		auto section = sections[i].substr(0, 16);

		bool found = false;
		for(unsigned int j = 0; j < dir.size(); j++)
		{
			const auto& dirEntry = dir[j];
			if(!strncmp(dirEntry.name, section.c_str(), 16))
			{
				uint32 baseSector = currEntry.sector;
				currEntry = dirEntry;
				currEntry.sector += baseSector;
				found = true;
				break;
			}
		}

		if(!found)
		{
			return nullptr;
		}
	}

	assert(currEntry.type == 'F');

	auto fileContents = m_nandReader.ReadFile(currEntry.sector, currEntry.size);
	assert(fileContents.size() == currEntry.size);
	auto stream = new Framework::CMemStream();
	stream->Write(fileContents.data(), fileContents.size());
	stream->Seek(0, Framework::STREAM_SEEK_SET);

	return stream;
}

Ioman::DirectoryIteratorPtr CNamcoNANDDevice::GetDirectory(const char*)
{
	return Ioman::DirectoryIteratorPtr();
}
