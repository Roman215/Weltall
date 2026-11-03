#include "ShareableCode.h"

#include "EventAssemblyText.h"
#include "EventOperandText.h"
#include "Field/EventInstructionSet.h"

#include <map>
#include <span>
#include <vector>

namespace Weltall
{
	namespace
	{
		using namespace EventAssemblyText;
		using namespace EventScriptMatch;

		std::string OffsetText(std::size_t Offset)
		{
			std::string Text = "@0x";
			AppendHex(Text, static_cast<uint32>(Offset), 4);
			return Text;
		}

		std::span<const uint8> CodeOf(const FProgram& Program)
		{
			return {Program.Scripts->Code(), Program.Scripts->CodeSize()};
		}

		/** The writing of one patch's code: which edited steps it carries, the names it gives them, and its lines. */
		class FCodeWriter
		{
		public:
			FCodeWriter(const FProgram& InEdited, const FProgram& InDisc, const FCorrespondence& InMatch, const std::function<void(std::string&, int32)>& InMessage,
				const std::set<std::string>& InNames)
				: Edited(InEdited), Disc(InDisc), Match(InMatch), Message(InMessage), Names(InNames), Carried(InEdited.Steps.size(), false)
			{
			}

			TResult<std::string> Write()
			{
				if (std::optional<FError> Unread = PlaceEntries())
				{
					return *Unread;
				}
				for (std::size_t Site = 0; Site < Disc.Steps.size(); ++Site)
				{
					const int32 Step = Match.Detours[Site];
					if (Step >= 0 && Match.Kept[static_cast<std::size_t>(Step)] < 0)
					{
						Carry(Step);
					}
				}
				if (std::optional<FError> Broken = CarryWhatFollows())
				{
					return *Broken;
				}
				NameSteps();
				std::string Text = DetourLines();
				Text += EntryLines;
				Text += CodeLines();
				return Text;
			}

		private:
			/** Each entry point of the edited map: left as the disc's, the disc's other code, none, or carried code. */
			std::optional<FError> PlaceEntries()
			{
				const FEventScripts& Mine = *Edited.Scripts;
				const FEventScripts& Theirs = *Disc.Scripts;
				for (int32 Actor = 0; Actor < Mine.ActorCount(); ++Actor)
				{
					for (int32 Script = 0; Script < FEventScripts::ScriptsPerActor; ++Script)
					{
						const bool bMine = IsScriptUsed(Mine, Actor, Script);
						const bool bTheirs = IsScriptUsed(Theirs, Actor, Script);
						if (!bMine)
						{
							if (bTheirs)
							{
								AddEntryLine(Actor, Script, 0);
							}
							continue;
						}
						const int32 Step = Edited.StepAt(Mine.EntryPoint(Actor, Script));
						const int32 Site = bTheirs ? Disc.StepAt(Theirs.EntryPoint(Actor, Script)) : -1;
						// A script that begins where no instruction does, on the disc too, is left as the disc's.
						if (Step < 0 && bTheirs && Site < 0)
						{
							continue;
						}
						if (Step < 0)
						{
							return MakeError("actor " + std::to_string(Actor) + "'s script " + std::to_string(Script) + " begins where no instruction does");
						}
						if (Site >= 0 && (Kept(Step) == Site || Match.Detours[static_cast<std::size_t>(Site)] == Step))
						{
							continue;
						}
						if (Kept(Step) >= 0)
						{
							AddEntryLine(Actor, Script, Disc.Steps[static_cast<std::size_t>(Kept(Step))].Offset);
							continue;
						}
						ScriptsAt[Step].emplace_back(Actor, Script);
						Carry(Step);
					}
				}
				return std::nullopt;
			}

			void AddEntryLine(int32 Actor, int32 Script, std::size_t Offset)
			{
				EntryLines += "entry " + std::to_string(Actor) + " " + std::to_string(Script) + " " + OffsetText(Offset) + "\n";
			}

			int32 Kept(int32 Step) const { return Match.Kept[static_cast<std::size_t>(Step)]; }

			void Carry(int32 Step)
			{
				if (!Carried[static_cast<std::size_t>(Step)])
				{
					Carried[static_cast<std::size_t>(Step)] = true;
					Pending.push_back(Step);
				}
			}

			/** What the carried steps go on to that the disc's map does not play, a jump table's entries always: they follow it. */
			std::optional<FError> CarryWhatFollows()
			{
				while (!Pending.empty())
				{
					const int32 Step = Pending.back();
					Pending.pop_back();
					const FStep& At = Edited.Steps[static_cast<std::size_t>(Step)];
					for (std::size_t Each = 0; Each < At.Successors.size(); ++Each)
					{
						const int32 Next = Edited.StepAt(At.Successors[Each]);
						if (Next < 0)
						{
							std::string Where;
							AppendHex(Where, At.Offset, 4);
							return MakeError("its code at " + Where + " goes on into what is no instruction");
						}
						if (Kept(Next) < 0 || Each + At.Entries >= At.Successors.size())
						{
							Carry(Next);
						}
					}
				}
				return std::nullopt;
			}

			bool EndsThread(const FStep& At) const
			{
				return ReadEventInstruction(CodeOf(Edited), At.Offset)->Shape->bEndsThread;
			}

			/** Whether the step after Step in the edited code is carried and laid right after it. */
			bool FollowedInPlace(std::size_t Step) const
			{
				const FStep& At = Edited.Steps[Step];
				return Step + 1 < Edited.Steps.size() && Carried[Step + 1] && Edited.Steps[Step + 1].Offset == At.Offset + At.Length;
			}

			/** A name for each carried step a jump or a detour goes to, in their order, and one for each detour's way to a step the disc plays. */
			void NameSteps()
			{
				std::vector<bool> Named(Edited.Steps.size(), false);
				for (std::size_t Step = 0; Step < Edited.Steps.size(); ++Step)
				{
					const FStep& At = Edited.Steps[Step];
					// The step after is reached without a name where the code lays it next.
					const bool bFollowedInPlace = !EndsThread(At) && FollowedInPlace(Step);
					for (std::size_t Each = bFollowedInPlace ? 1 : 0; Carried[Step] && Each < At.Successors.size(); ++Each)
					{
						const int32 Next = Edited.StepAt(At.Successors[Each]);
						Named[static_cast<std::size_t>(Next)] = Named[static_cast<std::size_t>(Next)] || Kept(Next) < 0;
					}
				}
				for (const int32 Step : Match.Detours)
				{
					if (Step >= 0 && Kept(Step) < 0)
					{
						Named[static_cast<std::size_t>(Step)] = true;
					}
				}
				for (std::size_t Step = 0; Step < Edited.Steps.size(); ++Step)
				{
					if (Named[Step])
					{
						Labels[static_cast<int32>(Step)] = NextName();
					}
				}
				for (std::size_t Site = 0; Site < Disc.Steps.size(); ++Site)
				{
					if (Match.Detours[Site] >= 0 && Kept(Match.Detours[Site]) >= 0)
					{
						WaysBack[static_cast<int32>(Site)] = NextName();
					}
				}
			}

			std::string NextName()
			{
				std::string Name;
				do
				{
					Name = "Part" + std::to_string(++LastPart);
				}
				while (Names.contains(Name));
				return Name;
			}

			/** Where the patch reaches an edited step: the disc's code that plays it, else its name. */
			std::string Place(int32 Step) const
			{
				return Kept(Step) >= 0 ? OffsetText(Disc.Steps[static_cast<std::size_t>(Kept(Step))].Offset) : Labels.at(Step);
			}

			std::string DetourLines() const
			{
				std::string Text;
				for (std::size_t Site = 0; Site < Disc.Steps.size(); ++Site)
				{
					const int32 Step = Match.Detours[Site];
					if (Step < 0)
					{
						continue;
					}
					const FStep& At = Disc.Steps[Site];
					std::string Instruction;
					AppendInstruction(Instruction, *ReadEventInstruction(CodeOf(Disc), At.Offset), [](std::string& Out, std::size_t Target) { Out += OffsetText(Target); });
					Text += "detour " + OffsetText(At.Offset) + " to " + (Kept(Step) >= 0 ? WaysBack.at(static_cast<int32>(Site)) : Labels.at(Step)) + " ";
					Text += std::string_view(Instruction).substr(Instruction.find_first_not_of(' '));
					Text += "\n";
				}
				return Text;
			}

			std::string CodeLines() const
			{
				std::string Text;
				const auto Jump = [this](std::string& Out, std::size_t Target) { Out += Place(Edited.StepAt(Target)); };
				for (std::size_t Step = 0; Step < Edited.Steps.size(); ++Step)
				{
					if (!Carried[Step])
					{
						continue;
					}
					const FStep& At = Edited.Steps[Step];
					if (const auto Label = Labels.find(static_cast<int32>(Step)); Label != Labels.end())
					{
						Text += Label->second + ":\n";
					}
					if (const auto Scripts = ScriptsAt.find(static_cast<int32>(Step)); Scripts != ScriptsAt.end())
					{
						for (const auto& [Actor, Script] : Scripts->second)
						{
							Text += "actor " + std::to_string(Actor) + " script " + std::to_string(Script) + ":\n";
						}
					}
					AppendInstruction(Text, *ReadEventInstruction(CodeOf(Edited), At.Offset), Jump, Message);
					Text += "\n";
					// What follows is the next carried step, or the disc's code that plays it.
					if (!EndsThread(At) && !FollowedInPlace(Step))
					{
						Text += "    Jump " + Place(Edited.StepAt(At.Successors.front())) + "\n";
					}
				}
				for (const auto& [Site, Name] : WaysBack)
				{
					Text += Name + ":\n    Jump " + Place(Match.Detours[static_cast<std::size_t>(Site)]) + "\n";
				}
				return Text;
			}

			const FProgram& Edited;
			const FProgram& Disc;
			const FCorrespondence& Match;
			const std::function<void(std::string&, int32)>& Message;
			const std::set<std::string>& Names;
			std::vector<bool> Carried;
			std::vector<int32> Pending;
			/** The scripts whose entry points are carried steps, by step. */
			std::map<int32, std::vector<std::pair<int32, int32>>> ScriptsAt;
			std::map<int32, std::string> Labels;
			/** The names of the ways a detour takes to a step the disc's code plays elsewhere, by detoured disc step. */
			std::map<int32, std::string> WaysBack;
			std::string EntryLines;
			int32 LastPart = 0;
		};
	}

	TResult<std::string> WriteShareableCode(const EventScriptMatch::FProgram& Edited, const EventScriptMatch::FProgram& Disc,
		const EventScriptMatch::FCorrespondence& Match, const std::function<void(std::string&, int32)>& Message, const std::set<std::string>& Names)
	{
		return FCodeWriter(Edited, Disc, Match, Message, Names).Write();
	}
}
