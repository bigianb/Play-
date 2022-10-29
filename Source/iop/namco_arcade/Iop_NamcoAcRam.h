#pragma once

#include "../Iop_Module.h"

namespace Iop
{
	namespace Namco
	{
		class CAcRam : public CModule
		{
		public:
			CAcRam(uint8*);
			virtual ~CAcRam() = default;

			std::string GetId() const override;
			std::string GetFunctionName(unsigned int) const override;
			void Invoke(CMIPS&, unsigned int) override;

			void Read(uint32, uint8*, uint32);
			void Write(uint32, const uint8*, uint32);

		private:
			static const uint32_t g_extRamSize = 0x4000000;
			uint8 m_extRam[g_extRamSize];
			uint8* m_iopRam = nullptr;
		};
	}
}