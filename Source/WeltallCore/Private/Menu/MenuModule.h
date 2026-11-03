#pragma once

#include "Graphics/Vram.h"
#include "Menu/MenuModuleParts.h"
#include "Menu/MenuPrimitive.h"
#include "Menu/MenuProgram.h"
#include "Menu/MenuResources.h"
#include "Menu/MenuTask.h"
#include "Text/MesWindow.h"

#include <array>
#include <coroutine>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Weltall
{
	/** What the menus' input gives each frame: one code, the first the queued pad states yield. */
	enum class EMenuCode : uint8
	{
		Right = 0,
		Down = 1,
		Left = 2,
		Up = 3,
		Confirm = 4,
		Cancel = 5,
		Square = 6,
		Triangle = 7,
		None = 8,
		R1 = 9,
		L1 = 10,
		Start = 11,
		Select = 12,
	};

	/** The 3D layer's opening and closing. */
	enum class EMenuLayer : uint8
	{
		Still = 0,
		Opening = 1,
		Closing = 2,
		OpenAsked = 3,
		CloseAsked = 4,
	};

	/**
	 * The menu's block of flags: mostly which of its pieces each frame links, by index as the original
	 * keeps them, with the party's members and whether each has a gear in their places among them (the
	 * menu clears some runs of flags wholesale, and one of those runs reaches past its own).
	 */
	namespace MenuFlag
	{
		constexpr int32 Count = 0x6C;
		constexpr int32 Selection = 3;
		constexpr int32 Pointer = 4;
		constexpr int32 Gold = 5;
		constexpr int32 PlayTime = 6;
		constexpr int32 Commands = 9;
		constexpr int32 SubCommands = 10;
		constexpr int32 FileDetails = 11;
		/** The seven windows: made (0x20 on), then growing (0x27 on). */
		constexpr int32 Windows = 0x20;
		/** The command arc's text labels (8): placed, never linked. */
		constexpr int32 ArcLabels = 0x0C;
		constexpr int32 TargetPointers = 0x2F;
		/** The party's three members by slot, 0xFF for none. */
		constexpr int32 Party = 0x30;
		constexpr int32 HelpLabels = 0x34;
		constexpr int32 MemberHints = 0x53;
		/** The inner arc's text labels (4), linked flat. */
		constexpr int32 SubArcLabels = 0x5C;
		/** Whether each party slot's member has a gear. */
		constexpr int32 PartyHasGear = 0x60;
		constexpr int32 MemberHintsMade = 0x67;
		/** The status sheet, its stats and its equipment's names. */
		constexpr int32 StatusSheet = 7;
		constexpr int32 Stats = 8;
		constexpr int32 EquipNames = 0x4B;
		/** The sheet's text labels (6): its inner arc's three (placed, never linked), its equipment's headings. */
		constexpr int32 SheetLabels = 0x14;
		/** The Deathblows' rows, and the Skills screen's title (its first of two labels). */
		constexpr int32 Deathblows = 0x4D;
		constexpr int32 SkillsTitle = 0x4E;
		/** The lists' tags (8): the side and the extent an item or ability works on, the EP's labels. */
		constexpr int32 HelpTags = 0x38;
		/** The Equip screen's slots' labels (6): a page's names. */
		constexpr int32 EquipSlotLabels = 0x40;
		constexpr int32 TargetPanels = 0x46;
		constexpr int32 ItemList = 0x48;
		constexpr int32 ScrollKnob = 0x49;
		constexpr int32 AbilityList = 0x4A;
		constexpr int32 EquipList = 0x4C;
		/** The two list cursors. */
		constexpr int32 Cursors = 0x50;
		/** The Gear screen's labels (6): its inner arc's four, its equipment's headings. */
		constexpr int32 GearLabels = 0x54;
		/** The File screen's inner arc's text labels (6): placed, never linked. */
		constexpr int32 FileLabels = 0x1A;
		/** The File screen's card labels, once its cards are read. */
		constexpr int32 Cards = 0x68;
	}

	/** The pad's logical buttons as the menus test them (the US release swaps Cross and Circle: confirm is Circle). */
	namespace MenuButton
	{
		constexpr uint16 L2 = 0x0001;
		constexpr uint16 L1 = 0x0004;
		constexpr uint16 R1 = 0x0008;
		constexpr uint16 Triangle = 0x0010;
		constexpr uint16 Circle = 0x0020;
		constexpr uint16 Cross = 0x0040;
		constexpr uint16 Square = 0x0080;
		constexpr uint16 Select = 0x0100;
		constexpr uint16 Start = 0x0800;
		constexpr uint16 Up = 0x1000;
		constexpr uint16 Right = 0x2000;
		constexpr uint16 Down = 0x4000;
		constexpr uint16 Left = 0x8000;
	}

	/** One of the codes the input gives: the buttons it takes (held again, or newly pressed) and the sound it makes. */
	struct FMenuInputTest
	{
		uint16 Bits = 0;
		bool bRepeated = false;
		EMenuCode Code = EMenuCode::None;
		/** None: silent. */
		uint8 Sound = 0;
	};

	/**
	 * What every module of the Menu program shares, each with its own copy in the original: the frame's
	 * output and the steps that wait for frames, the input, the sound, VRAM, the windows, the text
	 * labels and the 3D layer. A module is its own program run from its start (Run) and draws its own
	 * frames (DrawFrame).
	 */
	class FMenuModule
	{
	public:
		FMenuModule(const FMenuServices& InServices, const FMenuRequest& InRequest);
		virtual ~FMenuModule() = default;
		FMenuModule(const FMenuModule&) = delete;
		FMenuModule& operator=(const FMenuModule&) = delete;

		FMenuTaskArena& TaskArena() { return Arena; }

		/** The module's run for the request, from its start to its end. */
		virtual TMenuTask<> Run() = 0;

		/** The gear shop's model of the gear it shows; none in the other modules. */
		virtual const FGearShopModel* GearModel() const { return nullptr; }

		/** Waits for the next frame: draws it, then the program lets its vertical blank pass. */
		auto Frame()
		{
			struct FAwaiter
			{
				FMenuModule& Menu;

				bool await_ready() const noexcept { return false; }
				void await_suspend(std::coroutine_handle<> Step) noexcept
				{
					Menu.Waiting = Step;
					Menu.DrawOrWaitForPad();
				}
				void await_resume() const noexcept {}
			};
			return FAwaiter{*this};
		}

		/** Stops the run with an error naming what the port does not run yet: the program reports it. */
		auto Fail(std::string Message)
		{
			struct FAwaiter
			{
				FMenuModule& Menu;
				std::string Message;

				bool await_ready() const noexcept { return false; }
				void await_suspend(std::coroutine_handle<>) noexcept { Menu.Fault = FError{std::move(Message)}; }
				void await_resume() const noexcept {}
			};
			return FAwaiter{*this, std::move(Message)};
		}

		/**
		 * The wait for a controller the last frame's input read began: true while it holds the run, the
		 * frame's drawing done in the step a controller comes back in; false when there is none.
		 */
		bool StepPadWait();

		std::coroutine_handle<> WaitingStep() const { return Waiting; }
		const std::optional<FError>& Failure() const { return Fault; }
		bool IsSoftReset() const { return bSoftReset; }
		const FMenuFrame& DrawnFrame() const { return Drawn; }
		const FVram& Vram() const { return MenuVram; }
		void SetUploadObserver(IVramUploads* Observer) { UploadObserver = Observer; }

	protected:
		/** One frame: the input read, the scene linked, the ordering table taken into the snapshot. */
		virtual void DrawFrame() = 0;
		/** The frame drawn, or with no controller connected its input read begins its wait: the sound paused, the frame held. */
		void DrawOrWaitForPad();

		// The frame's services (MenuModule.cpp).
		/** The first code the queued pad states yield, by the module's tests in order; the rest wait. */
		void ReadInput(std::span<const FMenuInputTest> Tests);
		void UpdateTransition();
		void OpenLayer();
		void CloseLayer();
		void AddScreen(const std::vector<FMenuPrimitive>& Primitives);
		/** Pieces drawn on the layer, projected and linked in order. */
		void AddLayered(std::vector<FMenuPrimitive>& Parts);
		/** A 3D quad's corners projected through this frame's layer or flat view, then linked. */
		void AddProjected(FMenuPrimitive& Quad, bool bOnLayer, int32 Entry);
		void PlaySound(uint8 Effect);
		/**
		 * An image into the menu's VRAM, seen by the observer when there is one, as Kind unless it names its own: most of
		 * what the menus upload is text they typed.
		 */
		void Upload(const FVramImage& Image, EVramUploadKind Kind = EVramUploadKind::Text);
		/** A number's nine digits (hundreds of millions first) into NumberDigits, its leading zeros blank. */
		void SplitNumber(uint32 Value);
		uint8 Member(int32 Slot) const { return Flags[static_cast<std::size_t>(MenuFlag::Party + Slot)]; }
		/** The grey the menus lay over the picture they were opened on, and its subtractive drawing mode. */
		void InitBackdrop();
		void AddBackdrop();
		/** A character's large portrait, as the player has it drawn, into the place of a party slot's (its CLUT, then its pixels). */
		void UploadPortrait(int32 Slot, uint8 Character);
		/** The sheet's TIM archive into VRAM, then the small faces the player has drawn otherwise: Elly's with purple hair, Id's (UploadFace). */
		void UploadArchive();
		/**
		 * Character's small face (sheet frame 14Eh + id) from the archive's own pixels, recoloured with Elly's purple hair while the
		 * player has it; Id's in Fei's place.
		 */
		void UploadFace(uint8 Character);
		/** The record whose pictures and Deathblow names stand for Character, as the player has it drawn (FCharacterLooks). */
		uint8 DrawnCharacter(uint8 Character) const;
		/** Whether Character's portrait and small face are Id's, made from his dialogue face (IdsPictures). */
		bool DrawnAsId(uint8 Character) const;
		/** The game's name Number as the menus type it, from its first byte: "Id" for Fei while the player has him drawn as Id. */
		std::span<const uint8> ShownName(int32 Number) const;

		// Labels (MenuLabels.cpp).
		void RenderLabels(std::span<FMenuLabel> Labels, std::span<const uint8> Messages, int32 Base);
		void SetLabelTexture(FMenuLabel& Label, int32 Index, int32 Base);
		/** The label placed in this frame's draw buffer: drawn from that buffer's copy from now on. */
		void RecordLabel(FMenuLabel& Label);
		/** The label's texture, as just set, in its other buffer's copy too. */
		static void MirrorLabelTexture(FMenuLabel& Label);
		/** A string typed as the kernel's renderer types it into one plane of an image of 4-bit strips; its width in pixels. */
		int32 RenderString(FMesWindow& Typing, FVramImage& Image, std::span<const uint8> Text, int32 Plane);
		void ClearFlags(int32 First, int32 Count);

		/** A label's texture in one plane of the strips at (384, 128): row Index's half, Base rows down; dimmed unless Mode's 80h. */
		void SetStripLabelTexture(FMenuLabel& Label, int32 Index, int32 Base, uint8 Mode);
		/** Plain text (letters, digits and the font's marks) typed as RenderString types the game's; its width in pixels. */
		int32 RenderPlain(FMesWindow& Typing, FVramImage& Image, std::string_view Text, int32 Plane);
		/** Glyph indices as the text they are in the message encoding (the glyph map's bytes), ended by 0. */
		void GlyphText(std::span<const uint16> Glyphs, std::vector<uint8>& Text) const;
		/**
		 * A prompt's lines (texts FirstText on), typed two to a strip from (320, 78) and placed on the layer
		 * one under another from (X, Y); with SecondLine, plain text in the second line's place.
		 */
		void RenderPromptLines(std::span<FMenuLabel> Lines, uint8 FirstText, int32 X, int32 Y, std::string_view SecondLine = {});

		// The arcs of orbs, their labels and the selection's pointer (MenuArcs.cpp), laid out by the module's tables.
		TMenuTask<> RevealCommands(std::span<const FMenuArcCommand> Commands);
		/** The arc with Selected's orb red and the pointer at its place (Place, where it is not the command's own number). */
		void DrawCommands(std::span<const FMenuArcCommand> Commands, int32 Selected, int32 Place = -1);
		TMenuTask<> RevealSubCommands(int32 Row);
		void DrawSubCommands(int32 Row);
		void DrawSelection(int32 Place);
		void HideSelection();
		std::span<const FMenuArcCommand> SubCommandRow(int32 Row) const;
		/** A text label beside an arc's orb (Index, the arc's places from ArcOffset), the flags of its set cleared first. */
		void PlaceArcLabel(std::span<FMenuLabel> Labels, std::span<const int32> Offsets, int32 FirstFlag, int32 Cleared, int32 Index, int32 ArcOffset);
		void AddArc(FMenuArc& Drawing, bool bApplyDim);
		void AddSelection();

		// Windows (MenuWindows.cpp).
		void OpenWindow(int32 Index, int16 X, int16 Y, uint16 Width, uint16 Height, bool bGrows, bool bOnLayer, int32 Entry, bool bScrollBar);
		void InitWindowPrims(int32 Index);
		void BuildWindow(int32 Index, int16 X, int16 Y, uint16 Width, uint16 Height, bool bOnLayer, int32 Entry, bool bScrollBar);
		void GrowWindows();
		void AddWindows();
		void CloseWindow(int32 Index);


		FMenuServices Services;
		FMenuRequest Request;
		const FMenuResources& Resources;
		FMenuTaskArena Arena;
		std::coroutine_handle<> Waiting;
		/** The frame's input read waits for a controller, and the play time as it began, which the wait does not count. */
		bool bPadWait = false;
		uint32 PadWaitPlayTime = 0;
		std::optional<FError> Fault;
		bool bSoftReset = false;

		// The frame.
		int32 Buffer = 0;
		EMenuCode Code = EMenuCode::None;
		uint16 PadHeld = 0;
		bool bDrawScene = false;
		bool bSoundOn = false;
		bool bSoftResetAllowed = true;
		FMenuOrderingTable Table;
		FMenuFrame Drawn;
		FVram MenuVram;
		IVramUploads* UploadObserver = nullptr;

		// The 3D layer.
		EMenuLayer LayerState = EMenuLayer::Still;
		FMenuLayerTransform Layer;
		FGte LayerGte;
		FGte FlatGte;

		FMenuPrimitive Background;
		FMenuPrimitive BackgroundMode;

		// The arcs: the module's places and inner arcs, the commands selected, the orbs and labels drawn.
		std::span<const int32> ArcX;
		std::span<const int32> ArcY;
		std::span<const std::array<FMenuArcCommand, 4>> SubArcRows;
		/** The inner arc's first place among ArcX and ArcY. */
		int32 SubArcFirstPlace = 0;
		int32 Command = 0;
		int32 DrawnCommand = 0xFF;
		int32 SubCommand = 0;
		int32 DrawnSubCommand = 0xFF;
		int32 SubCommandCount = 0;
		FMenuArc Arc;
		FMenuArc SubArc;
		std::vector<FMenuPrimitive> Pointer;
		FMenuPrimitive SelectionMode;

		// What is shown, the party's members among it.
		std::array<uint8, MenuFlag::Count> Flags{};
		std::array<FMenuWindow, 7> Windows{};
		std::array<FMenuWindowGrowth, 7> Growths{};
		/** The digits a number is split into, 9 of them, leading zeros blank. */
		std::array<uint8, 9> NumberDigits{};

		// The text labels, and what renders them.
		FVramImage LabelImage;
		FVramImage NameImage;
		FMesWindow LabelTyping{24, 1};
		FMesWindow NameTyping{0x24, 1};
		FVramImage PromptImage;
		FMesWindow PromptTyping{0x36, 1};
		std::vector<uint8> GlyphScratch;
		std::vector<FMenuPrimitive> Scratch;
	};
}
