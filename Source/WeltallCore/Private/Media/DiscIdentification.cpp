#include "Media/DiscIdentification.h"

#include "Foundation/Sha1.h"
#include "Media/PsxExecutable.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

namespace Weltall
{
	namespace
	{
		struct FKnownDisc
		{
			FDiscIdentity Identity;
			std::string_view BootExecutable;
			std::string_view ExecutableSha1;
		};

		constexpr std::array KnownDiscs = {
			FKnownDisc{{EDiscRelease::UsaDisc1, 1, "Xenogears (USA) Disc 1"},
				"SLUS_006.64", "560bbdbeb9264c935294ecad5a3d4ab230a006a9"},
			FKnownDisc{{EDiscRelease::UsaDisc2, 2, "Xenogears (USA) Disc 2"},
				"SLUS_006.69", "a378608389f04456925c6afaccb644ab90eaff30"},
			FKnownDisc{{EDiscRelease::UndubDisc1, 1, "Xenogears (USA undub) Disc 1"},
				"SLUS_006.64", "1e21636bc067bd3873b78be3d72978c8038daca0"},
			FKnownDisc{{EDiscRelease::UndubDisc2, 2, "Xenogears (USA undub) Disc 2"},
				"SLUS_006.69", "2d9a51f9d6fc6855118fc217002edaef30dadec6"},
			FKnownDisc{{EDiscRelease::JapanDisc1, 1, "Xenogears (Japan) Disc 1"},
				"SLPS_011.60", "3f9633f80213427f55894cda9fc8c3346ecf31b4"},
			FKnownDisc{{EDiscRelease::JapanDisc2, 2, "Xenogears (Japan) Disc 2"},
				"SLPS_011.61", "15dae4421e0db5d864b5cada59dfcc786f323c9b"},
			// The French translation, built on the undub's patch: with the English voices, and with the Japanese.
			FKnownDisc{{EDiscRelease::UsaDisc1, 1, "Xenogears (French translation) Disc 1", true},
				"SLUS_006.64", "3fd5d978b4ee6a3452827f5beaad5424c6925bfa"},
			FKnownDisc{{EDiscRelease::UsaDisc2, 2, "Xenogears (French translation) Disc 2", true},
				"SLUS_006.69", "36329b2840293a96b544416d8bd05a45bc7868b8"},
			FKnownDisc{{EDiscRelease::UndubDisc1, 1, "Xenogears (French translation, Japanese voices) Disc 1", true},
				"SLUS_006.64", "308c08f84d1d61c5c1de527b14e8932d795fca9d"},
			FKnownDisc{{EDiscRelease::UndubDisc2, 2, "Xenogears (French translation, Japanese voices) Disc 2", true},
				"SLUS_006.69", "f887847e031eaeb3a83f81c478b8b18bed56ffbe"},
		};

		/**
		 * The releases a patch of the game's data may rewrite: the SHA-1 of the executable with the parts such a patch
		 * changes zeroed (CodeSha1), the rest being its code.
		 */
		constexpr std::array PatchableDiscs = {
			FKnownDisc{{EDiscRelease::UsaDisc1, 1, "Xenogears (USA, patched) Disc 1", true},
				"SLUS_006.64", "433428236a2ff61fe404ffd93cc04e275b0df673"},
			FKnownDisc{{EDiscRelease::UsaDisc2, 2, "Xenogears (USA, patched) Disc 2", true},
				"SLUS_006.69", "8abdb9f6c3f4ff167cfd82957dd27350cdbd13fd"},
		};

		/**
		 * What a patch of the game's data rewrites in the executable: the file index, for the files it moves or
		 * resizes, and the boot notice's packed picture, which runs to the end of the body.
		 */
		constexpr uint32 FileIndexAddress = 0x80010004;
		constexpr uint32 FileIndexEnd = 0x80018004;
		constexpr uint32 BootPictureAddress = 0x8004EABC;

		/** The executable with those parts zeroed, hashed: empty where it is not a program of the game's shape. */
		std::string CodeSha1(std::span<const uint8> Executable)
		{
			const TResult<FPsxExecutable> Parsed = FPsxExecutable::Parse(std::vector<uint8>(Executable.begin(), Executable.end()));
			if (!Parsed || !Parsed->Contains(FileIndexAddress, FileIndexEnd - FileIndexAddress) || !Parsed->Contains(BootPictureAddress, 4))
			{
				return {};
			}
			std::vector<uint8> Code(Executable.begin(), Executable.end());
			const uint32 Load = Parsed->LoadAddress();
			const auto Clear = [&Code, Load](uint32 From, uint32 To)
			{
				std::fill(Code.begin() + FPsxExecutable::HeaderSize + (From - Load), Code.begin() + FPsxExecutable::HeaderSize + (To - Load), uint8{0});
			};
			Clear(FileIndexAddress, FileIndexEnd);
			Clear(BootPictureAddress, Load + static_cast<uint32>(Parsed->Body().size()));
			return FSha1::ToHex(FSha1::Hash(Code));
		}
	}

	std::string_view DiscReleaseId(EDiscRelease Release)
	{
		switch (Release)
		{
		case EDiscRelease::UsaDisc1: return "usa.disc1";
		case EDiscRelease::UsaDisc2: return "usa.disc2";
		case EDiscRelease::UndubDisc1: return "usa.undub.disc1";
		case EDiscRelease::UndubDisc2: return "usa.undub.disc2";
		case EDiscRelease::JapanDisc1: return "jpn.disc1";
		case EDiscRelease::JapanDisc2: return "jpn.disc2";
		}
		return {};
	}

	uint8 DiscNumberOfRelease(EDiscRelease Release)
	{
		return Release == EDiscRelease::UsaDisc2 || Release == EDiscRelease::UndubDisc2 || Release == EDiscRelease::JapanDisc2 ? 2 : 1;
	}

	bool IsPlayableRelease(EDiscRelease Release)
	{
		return Release == EDiscRelease::UsaDisc1 || Release == EDiscRelease::UsaDisc2 ||
			Release == EDiscRelease::UndubDisc1 || Release == EDiscRelease::UndubDisc2;
	}

	EDiscRelease GameplayRelease(EDiscRelease Release)
	{
		return IsPlayableRelease(Release) ? (DiscNumberOfRelease(Release) == 2 ? EDiscRelease::UsaDisc2 : EDiscRelease::UsaDisc1) : Release;
	}

	TResult<FDiscIdentity> IdentifyDisc(const FIso9660& Disc)
	{
		auto BootPath = FindBootExecutable(Disc);
		if (!BootPath)
		{
			return BootPath.Error();
		}
		const bool bKnownName = std::any_of(KnownDiscs.begin(), KnownDiscs.end(), [&BootPath](const FKnownDisc& Known) { return Known.BootExecutable == *BootPath; });
		if (!bKnownName)
		{
			return MakeError("not a supported disc (boot executable " + *BootPath + ")");
		}
		auto Executable = Disc.ReadFile(*BootPath);
		if (!Executable)
		{
			return Executable.Error();
		}
		return IdentifyExecutable(*BootPath, *Executable);
	}

	TResult<FDiscIdentity> IdentifyExecutable(std::string_view BootPath, std::span<const uint8> Executable)
	{
		const std::string Hash = FSha1::ToHex(FSha1::Hash(Executable));
		bool bKnownName = false;
		for (const FKnownDisc& Known : KnownDiscs)
		{
			bKnownName = bKnownName || BootPath == Known.BootExecutable;
			if (BootPath == Known.BootExecutable && Hash == Known.ExecutableSha1)
			{
				return Known.Identity;
			}
		}
		if (!bKnownName)
		{
			return MakeError("not a supported disc (boot executable " + std::string(BootPath) + ")");
		}
		const std::string Code = CodeSha1(Executable);
		for (const FKnownDisc& Patchable : PatchableDiscs)
		{
			if (BootPath == Patchable.BootExecutable && Code == Patchable.ExecutableSha1)
			{
				return Patchable.Identity;
			}
		}
		return MakeError(std::string(BootPath) + " is a supported disc, but this release of it is not (executable " + Hash + ")");
	}
}
