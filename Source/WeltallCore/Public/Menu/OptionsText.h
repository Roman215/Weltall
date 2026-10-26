#pragma once

#include <array>
#include <initializer_list>
#include <string>
#include <string_view>

/**
 * Every word the options screen shows, in one place: the pages' titles, each row's label, its values and its three
 * lines of help, and what the pages say back after an action; and the port's other words in the game's menus.
 *
 * The rules the text must keep (the menus' tests check them against the game's font):
 * - a help line holds about 30 letters, and a row's label and value share one line;
 * - the font has letters, digits, spaces and + , - . / ! " # % & ' ( ) : ? [ ] = only (no semicolon);
 * - "{}" is filled in as the game runs, in order: a key's name, a count, a card's name...
 * Lists of values are in the order the setting steps through them.
 */
namespace Weltall::OptionsText
{
	/** A row's label and its help. */
	struct FRow
	{
		std::string_view Label;
		std::array<std::string_view, 3> Help;
	};

	inline constexpr std::string_view On = "On";
	inline constexpr std::string_view Off = "Off";

	namespace Titles
	{
		inline constexpr std::string_view Display = "Display";
		inline constexpr std::string_view Sound = "Sound";
		inline constexpr std::string_view Camera = "Camera";
		inline constexpr std::string_view Fixes = "Fixes";
		inline constexpr std::string_view CustomFixes = "Custom Fixes";
		inline constexpr std::string_view Crt = "CRT Filter";
		inline constexpr std::string_view Controls = "Controls";
		inline constexpr std::string_view Saves = "Saves";
		inline constexpr std::string_view Import = "Import";
		inline constexpr std::string_view Assets = "Assets";
		inline constexpr std::string_view Modding = "Modding";
		inline constexpr std::string_view PerfectWorks = "Perfect Works";
	}

	namespace Display
	{
		inline constexpr FRow FullScreen{"Fullscreen", {"Borderless fullscreen. When", "off, the game runs in a", "window of the size below."}};
		inline constexpr FRow Resolution{"Resolution", {"Window size when not in", "fullscreen.", ""}};
		/** The value: width, then height. */
		inline constexpr std::string_view ResolutionValue = "{} x {}";
		inline constexpr FRow Aspect{"Aspect ratio", {"Auto fills the window. 16:9", "and 4:3 add black bars", "where needed."}};
		inline constexpr std::array<std::string_view, 3> AspectValues{"Auto", "16:9", "4:3"};
		inline constexpr FRow Sorting{"Depth sorting", {"Draws characters and Gears in", "the right depth order. Scenery", "too includes the scenery."}};
		inline constexpr std::array<std::string_view, 3> SortingValues{"Original", "Per pixel", "Scenery too"};
		inline constexpr FRow SceneryFilter{"Scenery filter", {"Sharp: original. Crisp: steady", "in motion. Smooth: softened.", "Upscaled: smooth diagonals."}};
		inline constexpr FRow SpriteFilter{"Sprite filter", {"Sharp: original. Crisp: steady", "in motion. Smooth: softened.", "Upscaled: smooth diagonals."}};
		inline constexpr std::array<std::string_view, 4> FilterValues{"Sharp", "Crisp", "Smooth", "Upscaled"};
		inline constexpr FRow MovieFilter{"Movie filter", {"Smooth: original look.", "Upscaled: a cleaner picture", "with sharper lines."}};
		inline constexpr std::array<std::string_view, 2> MovieFilterValues{"Smooth", "Upscaled"};
		inline constexpr FRow Crt{"CRT filter", {"Simulates a CRT screen with", "scanlines. Press Confirm to", "customize it."}};
		inline constexpr std::array<std::string_view, 4> CrtValues{"Off", "Monitor", "TV", "Custom"};
		inline constexpr FRow AntiAliasing{"Anti-aliasing", {"Smooths jagged edges. MSAA", "is fast. Supersampling is", "sharper but costs more."}};
		inline constexpr std::array<std::string_view, 3> AntiAliasingValues{"Off", "MSAA", "Supersampling"};
		inline constexpr FRow Culling{"Widescreen drawing", {"Full draws the whole", "widescreen view. Original", "draws only the 4:3 area."}};
		inline constexpr std::array<std::string_view, 2> CullingValues{"Original", "Full"};
		inline constexpr FRow Pilots{"Pilot sprites", {"In Gear-sized areas, Detailed", "shows the party on foot with", "their own sprites."}};
		/** The values with the party's own sprites on, then off. */
		inline constexpr std::array<std::string_view, 2> PilotsValues{"Detailed", "Original"};
		inline constexpr FRow Gears{"Gear sprites", {"3D shows Gears with their", "battle models. In play keeps", "sprites in cutscenes."}};
		inline constexpr std::array<std::string_view, 3> GearsValues{"Original", "3D in play", "3D always"};
		inline constexpr FRow ChapterCards{"Chapter titles", {"Shows each chapter's title", "as the chapter begins.", ""}};
		inline constexpr FRow TextSpeed{"Text speed", {"How fast text appears in", "message boxes. Instant shows", "each page at once."}};
		inline constexpr std::array<std::string_view, 4> TextSpeedValues{"Normal", "2x", "4x", "Instant"};
		inline constexpr FRow BattleSpeed{"Battle speed", {"How fast battles play. Change", "it in battle with Camera left", "and right."}};
		inline constexpr std::array<std::string_view, 3> BattleSpeedValues{"Normal", "2x", "4x"};
		inline constexpr FRow BattleFlashes{"Battle flashes", {"Softer dims the screen flashes", "of attacks and Ethers. Off", "removes them."}};
		inline constexpr std::array<std::string_view, 3> BattleFlashesValues{"Original", "Softer", "Off"};
		inline constexpr FRow PauseInBackground{"Pause in background", {"Pauses the game when its", "window isn't in focus. Play", "time stops too."}};
	}

	/** The CRT filter's parts. */
	namespace Crt
	{
		inline constexpr FRow Scanlines{"Scanlines", {"How dark the gaps between", "lines are. 0 gives a smooth", "picture."}};
		inline constexpr FRow Mask{"Phosphor mask", {"The pattern of red, green and", "blue dots: a monitor's", "stripes or a TV's slots."}};
		inline constexpr std::array<std::string_view, 3> MaskValues{"None", "Stripes", "Slots"};
		inline constexpr FRow MaskStrength{"Mask strength", {"How visible the phosphor", "mask is.", ""}};
		inline constexpr FRow MaskSize{"Mask size", {"How fine the phosphor mask", "is. Coarser is easier to see.", ""}};
		inline constexpr std::array<std::string_view, 3> MaskSizeValues{"Fine", "Medium", "Coarse"};
		inline constexpr FRow Sharpness{"Sharpness", {"How crisp the picture is.", "Lower is softer.", ""}};
		inline constexpr FRow ColorBleed{"Color bleed", {"How far colors smear side to", "side, as over S-Video. 0", "keeps them crisp."}};
		inline constexpr FRow Glow{"Glow", {"Adds a glow around bright", "parts of the picture.", ""}};
		inline constexpr FRow Curvature{"Curvature", {"How curved the screen is.", "0 is flat.", ""}};
		inline constexpr FRow Corners{"Rounded corners", {"How rounded the screen's", "corners are.", ""}};
		inline constexpr FRow Vignette{"Vignette", {"Darkens the corners of the", "picture.", ""}};
	}

	/** The Sound page: the volumes, the output, then the voices' language and the movies' subtitles as the discs and mods give them. */
	namespace Sound
	{
		inline constexpr std::string_view Volume = "Volume";
		inline constexpr FRow Master{"Master", {"Overall volume.", "", ""}};
		inline constexpr FRow Music{"Music", {"Music volume.", "", ""}};
		inline constexpr FRow Effects{"Effects", {"Sound effects volume.", "", ""}};
		inline constexpr FRow VoiceVolume{"Voices", {"Volume of voices in battle:", "shouts, cries and spoken", "lines."}};
		inline constexpr FRow Movies{"Movies", {"Movie volume. Speech and", "music in movies can't be set", "apart."}};
		inline constexpr std::string_view VolumeValue = "{}%";
		inline constexpr std::string_view OutputHeading = "Output";
		inline constexpr FRow Output{"Sound mode", {"Wide-Stereo spreads the", "sound wider. Mono plays the", "same sound from both sides."}};
		inline constexpr std::array<std::string_view, 3> OutputValues{"Stereo", "Wide-Stereo", "Mono"};
		inline constexpr FRow InBackground{"Background audio", {"Keeps playing sound when the", "game's window isn't in focus.", ""}};
		inline constexpr std::string_view Language = "Language";
		inline constexpr FRow Voices{"Voices", {"Voice language. Auto uses the", "disc you're playing. Applies", "from the next movie or battle."}};
		inline constexpr std::array<std::string_view, 3> VoiceValues{"Auto", "English", "Japanese"};
		/** The Voices row's help where the discs give one language. */
		inline constexpr std::array<std::string_view, 3> OnlyEnglish{"Japanese voices need the", "undub discs in the Games", "folder."};
		inline constexpr std::array<std::string_view, 3> OnlyJapanese{"English voices need the USA", "discs in the Games folder.", ""};
		/** The same where discs are added through the device's file picker. */
		inline constexpr std::array<std::string_view, 3> OnlyEnglishPicked{"Japanese voices need the", "undub discs: add them on the", "Saves page."};
		inline constexpr std::array<std::string_view, 3> OnlyJapanesePicked{"English voices need the USA", "discs: add them on the Saves", "page."};
		inline constexpr FRow Subtitles{"Subtitles", {"Movie subtitles. Auto shows", "them with Japanese voices.", "On always shows them."}};
		inline constexpr std::array<std::string_view, 3> SubtitleValues{"Auto", "On", "Off"};
		/** The Subtitles row's help without the undub's discs, whose Japanese voices Auto waits for. */
		inline constexpr std::array<std::string_view, 3> SubtitlesWithoutUndub{"Auto shows them with Japanese", "voices (undub discs needed).", "On always shows them."};
		inline constexpr std::array<std::string_view, 3> TranslationSubtitles{"Movie subtitles from the", "translation's discs.", ""};
		/** The Subtitles row where no disc or mod gives subtitles. */
		inline constexpr std::string_view NoSubtitles = "None found";
		inline constexpr std::array<std::string_view, 3> NoSubtitlesHelp{"Movie subtitles need the", "undub discs in Games or", "Perfect Works in Mods."};
		inline constexpr std::array<std::string_view, 3> NoSubtitlesPicked{"Movie subtitles need undub", "discs (Saves page) or Perfect", "Works (Modding page)."};
		inline constexpr FRow SubtitleSource{"Subtitles from", {"Which translation the movie", "subtitles use. Auto uses", "Perfect Works when it's on."}};
		inline constexpr std::array<std::string_view, 3> SubtitleSourceValues{"Auto", "Undub", "Perfect Works"};
		inline constexpr std::string_view TranslationSource = "Translation";
		/** The Subtitles from row's help where one source gives subtitles. */
		inline constexpr std::array<std::string_view, 3> OnlySource{"The only subtitles found.", "", ""};
	}

	/** The Modding page: the game's balance as the player changes it, then the Assets page and the mods found. */
	namespace Modding
	{
		inline constexpr std::string_view Balance = "Balance";
		inline constexpr FRow Experience{"Experience", {"Multiplies the experience you", "get from battles. Normal is", "the original amount."}};
		inline constexpr FRow Gold{"Gold", {"Multiplies the gold you get", "from battles. Normal is the", "original amount."}};
		inline constexpr std::array<std::string_view, 5> RewardValues{"Normal", "1.5x", "2x", "3x", "4x"};
		inline constexpr FRow Encounters{"Encounters", {"Changes how often random", "battles happen on the field", "and the world map."}};
		inline constexpr std::array<std::string_view, 5> EncounterValues{"None", "1/4", "1/2", "Normal", "2x"};
		inline constexpr std::string_view Mods = "Mods";
		inline constexpr FRow Assets{"Assets", {"Export game files to edit and", "load your own versions. Press", "Confirm to open."}};
		/** "{}" is the version of the mod found; the first line has no version where its release names none. */
		inline constexpr FRow PerfectWorks{"Perfect Works", {"Perfect Works Build {},", "found in the Mods folder.", "Press Confirm for options."}};
		inline constexpr std::string_view PerfectWorksUnversioned = "Perfect Works Build,";

		/**
		 * Where the game reaches the device's files only through its file picker (Android): External assets in the
		 * Assets page's place (its exports would make files nothing else reaches there), Perfect Works' row, then the
		 * device's rows under their heading: a mod or pack added, and each one on the device.
		 */
		inline constexpr FRow External{"External assets", {"Uses texture packs and other", "files added below.", ""}};
		inline constexpr std::array<std::string_view, 3> PerfectWorksPicked{"Perfect Works Build {}.", "Press Confirm for options.", ""};
		inline constexpr FRow AddZip{"Add zip file", {"Adds a mod or texture pack", "from a zip file.", ""}};
		inline constexpr FRow AddFolder{"Add folder", {"Adds a mod or texture pack", "from a folder. Unpack .7z and", ".rar files first."}};
		/** A mod or pack on the device: its name, then its kind (EModItemKind's order) and size. */
		inline constexpr std::array<std::string_view, 3> Item{"{}", "{}, {}.", "Press Confirm to remove it."};
		inline constexpr std::array<std::string_view, 5> ItemKinds{"Perfect Works Build", "Mod", "Texture pack", "Music pack", "Asset files"};
		inline constexpr std::array<std::string_view, 3> RemoveAsked{"{}", "Press Confirm again to", "remove it from this device."};
		/** Perfect Works, in the game's use: its value, and its help once removed or replaced by one added. */
		inline constexpr std::string_view OnRestart = "Pending";
		inline constexpr std::array<std::string_view, 3> RemovedOnRestart{"{}", "Will be removed when you", "restart Weltall."};
		inline constexpr std::array<std::string_view, 3> ReplacedOnRestart{"{}", "The new version replaces it", "when you restart Weltall."};
		inline constexpr std::array<std::string_view, 3> Removed{"Removed {}.", "", ""};
		inline constexpr std::array<std::string_view, 3> RemoveFailed{"Couldn't remove it.", "", ""};
		/** A zip's progress, its files not counted before they are read: the file, then how much of the zip is read. */
		inline constexpr std::array<std::string_view, 3> Extracting{"Extracting:", "{}", "{}% done."};
		/** A folder's files listed before they are copied: the folder's name, then how many so far. */
		inline constexpr std::array<std::string_view, 3> Scanning{"Scanning folder:", "{}", "{} files found."};
		/**
		 * What a transfer added, Perfect Works switched on and External assets on for a pack: "{}" Perfect Works' version
		 * after a space (nothing where it names none), a pack's name, a count of files.
		 */
		inline constexpr std::array<std::string_view, 3> PerfectWorksAdded{"Added Perfect Works{}.", "Restart Weltall to use it.", ""};
		inline constexpr std::array<std::string_view, 3> PerfectWorksNext{"Added Perfect Works{}.", "It replaces the current one", "when you restart Weltall."};
		inline constexpr std::array<std::string_view, 3> TexturesAdded{"Added texture pack", "{}.", ""};
		inline constexpr std::array<std::string_view, 3> MusicAdded{"Added music pack", "{}.", ""};
		inline constexpr std::array<std::string_view, 3> FoldersAdded{"Added {} from", "{}.", "Restart Weltall to use them."};
		inline constexpr std::array<std::string_view, 3> NotAMod{"No mod or texture pack found", "that Weltall can use.", ""};
		inline constexpr std::array<std::string_view, 3> NotAZip{"That's not a zip file. Unpack", ".7z and .rar files first, then", "use Add folder."};
	}

	/** The Perfect Works page: the parts of the mod the game takes, from its next start. */
	namespace PerfectWorks
	{
		/** The page's first line, which the cursor passes over. */
		inline constexpr std::string_view Notice = "Changes apply on restart";
		inline constexpr FRow Enabled{"Perfect Works", {"Uses Perfect Works from the", "Mods folder.", ""}};
		/** The same where it was added through the device's file picker. */
		inline constexpr std::array<std::string_view, 3> EnabledPicked{"Uses Perfect Works Build.", "", ""};
		inline constexpr std::string_view Story = "Story";
		inline constexpr FRow Script{"Script", {"Uses the Perfect Works", "translation for names and", "story script."}};
		inline constexpr FRow StoryMode{"Story mode", {"Uses Perfect Works' story", "mode: easy battles and no", "random battles on the field."}};
		inline constexpr FRow Music{"Music", {"Uses Perfect Works' new music", "for some scenes.", ""}};
		inline constexpr std::string_view Gameplay = "Gameplay";
		inline constexpr FRow Party{"Party and items", {"Uses Perfect Works' stats and", "abilities for the party, its", "items and its battle rules."}};
		inline constexpr FRow Enemies{"Enemies", {"Uses Perfect Works' stats and", "AI for some enemies and", "bosses."}};
		inline constexpr FRow Arena{"Battling", {"Uses Perfect Works' rebalance", "of the Battling arena. Expert", "adds its tweaks for 2 players."}};
		inline constexpr std::array<std::string_view, 3> ArenaValues{"Original", "Basic", "Expert"};
		inline constexpr std::string_view Graphics = "Graphics";
		inline constexpr FRow Portraits{"Portraits", {"Fixed uses Perfect Works'", "portraits, Resized for 4:3.", "Portrait packs need Original."}};
		inline constexpr std::array<std::string_view, 3> PortraitValues{"Original", "Fixed", "Resized"};
		inline constexpr FRow Roni{"Roni's portrait", {"Uses Perfect Works' portrait", "of Roni.", ""}};
		inline constexpr FRow Emeralda{"Emeralda", {"Uses Perfect Works' fix for", "Emeralda's sprites when she", "sits down at the cafe."}};
		inline constexpr FRow TitleScreen{"Title screen", {"Uses Perfect Works' title", "screen.", ""}};
		/** A part's values with the mod's on, then the game's own. */
		inline constexpr std::array<std::string_view, 2> ModValues{"Perfect Works", "Original"};
		inline constexpr std::array<std::string_view, 2> FixValues{"Fixed", "Original"};
	}

	namespace Assets
	{
		inline constexpr FRow External{"External assets", {"Loads your files from the", "Assets folder. Everything else", "comes from your discs."}};
		inline constexpr FRow Capture{"Capture textures", {"Saves textures as they're", "drawn, for any the export", "misses. Turn off when done."}};
		inline constexpr FRow Textures{"Export textures", {"Saves all textures as PNG", "files to edit. Existing files", "are kept."}};
		inline constexpr FRow Text{"Export text", {"Saves the game's text and", "font as files to edit.", "Existing files are kept."}};
		inline constexpr FRow Export{"Dump disc assets", {"Copies every file on your", "discs to Assets. Existing", "files are kept."}};
		inline constexpr FRow Music{"Export music WAVs", {"Saves every song as a WAV", "file. This can take several", "GB. Existing files are kept."}};
		inline constexpr FRow Cancel{"Cancel export", {"Stops after this step.", "Finished files are kept.", "Start again to resume."}};
		inline constexpr FRow Progress{"Export status", {"Files are in Assets beside", "Games. See Docs/Modding.md", "for formats and editing."}};
		inline constexpr std::array<std::string_view, 6> States{"Ready", "Working", "Done", "Cancelled", "Failed", "Unavailable"};
	}

	namespace Camera
	{
		inline constexpr FRow Zoom{"Zoom", {"Camera distance on the field.", "100% is the original.", ""}};
		inline constexpr std::string_view ZoomValue = "{}%";
		inline constexpr FRow Tilt{"Tilt", {"Camera angle on the field, in", "degrees. 0 is the original.", ""}};
		inline constexpr FRow Scenes{"Cutscene camera", {"Original uses the game's", "camera in cutscenes. Custom", "keeps yours when it can."}};
		/** The values with the switch on, then off. */
		inline constexpr std::array<std::string_view, 2> ScenesValues{"Original", "Custom"};
		inline constexpr FRow Follow{"Camera follow", {"Steady keeps the camera close", "to the party. Original lets it", "trail behind."}};
		/** The values with the switch on, then off. */
		inline constexpr std::array<std::string_view, 2> FollowValues{"Steady", "Original"};
		inline constexpr FRow Reset{"Reset camera", {"Resets the zoom, tilt and", "rotation.", ""}};
	}

	namespace Fixes
	{
		inline constexpr std::string_view Label = "Bug fixes";
		inline constexpr std::array<std::string_view, 3> PresetValues{"Port Enhancements", "Original", "Custom"};
		/** The help of each preset, in the order above. */
		inline constexpr std::array<std::array<std::string_view, 3>, 3> PresetHelp{{
			{"Fixes the original game's", "bugs.", ""},
			{"Plays exactly like the", "original, bugs and all.", ""},
			{"Press Confirm to choose", "each fix yourself.", ""},
		}};
		/** A fix's value, on and off. */
		inline constexpr std::string_view FixOn = "On";
		inline constexpr std::string_view FixOff = "Original";
	}

	/** The Custom Fixes list: its headings, then each fix's label and help. */
	namespace FixGroups
	{
		inline constexpr std::string_view Field = "Field";
		inline constexpr std::string_view Sound = "Sound";
		inline constexpr std::string_view Loading = "Loading saves";
		inline constexpr std::string_view Menus = "Menus";
		inline constexpr std::string_view Battle = "Battle";
		inline constexpr std::string_view WorldMap = "World map";
		inline constexpr std::string_view Arena = "Arena";
	}

	namespace Fix
	{
		inline constexpr FRow HpRecovery{"Event HP recovery", {"Events that heal the party", "restore HP. Original: some", "restore EP instead."}};
		inline constexpr FRow InstantFades{"Instant fades", {"Instant screen fades happen", "right away. Original: some", "leave the screen black."}};
		inline constexpr FRow SlowFades{"Slow fade-ins", {"Slow fade-ins from black play", "in full. Original: their", "start is cut off."}};
		inline constexpr FRow MovieMapChange{"Map change at movies", {"Map changes wait for their", "movie to finish. Original:", "they can stall."}};
		inline constexpr FRow GearAnimations{"Gear animations", {"Every part of a Gear", "animates. Original: some", "parts can freeze."}};
		inline constexpr FRow ScrollingTextures{"Scrolling textures", {"Scrolling textures move", "smoothly. Original: some", "jump when they loop."}};
		inline constexpr FRow EntryCamera{"Camera on map entry", {"A camera turned as a map", "starts stays put. Original:", "it swings from the old angle."}};
		inline constexpr FRow ScreenShakes{"Screen shakes", {"Screen shakes fade out over", "their own length. Original:", "some end early or late."}};
		inline constexpr FRow Ladders{"Ladders", {"Characters with no climbing", "animation keep their own.", "Original: garbled frames."}};
		inline constexpr FRow BledavikHotel{"Bledavik hotel", {"The hotel charges for the", "party you have. Original: a", "party of two pays for three."}};
		inline constexpr FRow RamsusShip{"Ramsus's ship", {"The ship's hum plays at its", "intended volume. Original:", "at full volume."}};
		inline constexpr FRow LacanMemories{"Lacan's memories", {"Room sounds fade out under", "the monologue. Original:", "they keep playing."}};
		inline constexpr FRow WidescreenCharacters{"Widescreen characters", {"Characters outside the 4:3", "area keep moving. Original:", "they stand still."}};
		inline constexpr FRow PartySprites{"Party sprites", {"Party members always use", "their own sprites. Original:", "pieces of others can show."}};
		inline constexpr FRow RpsHelp{"RPS help", {"Lose three times at Lahan's", "rock-paper-scissors and his", "wife offers to help."}};
		inline constexpr FRow ScenePictures{"Scene pictures", {"Painted backdrops stretch to", "fill screens up to 21:9.", "Original: black at the sides."}};
		inline constexpr FRow WidescreenStaging{"Widescreen staging", {"No one waits in view beside", "the 4:3 area for a cue.", "Original: some can be seen."}};
		inline constexpr FRow MoonGlow{"Moon glow", {"The glow around the moon", "fades into the night sky.", "Original: a dark square."}};
		inline constexpr FRow SpriteFrames{"Sprite frames", {"Sprite pieces such as Fei's", "arm stay attached. Original:", "some float a few pixels off."}};
		inline constexpr FRow SwimStroke{"Swimming stroke", {"Bart keeps his fast stroke", "past the waterway's exits.", "Original: it can turn slow."}};
		inline constexpr FRow SilentSongs{"Silent song starts", {"A song started silently", "doesn't silence the next one.", "Original: it does."}};
		inline constexpr FRow SoundPanning{"Sound panning", {"Sounds panning across end", "where they should. Original:", "they can end elsewhere."}};
		inline constexpr FRow HighNotes{"High notes", {"Notes above the range play", "at the top. Original: they", "wrap around to low notes."}};
		inline constexpr FRow GearWeight{"Gear weight", {"After loading, Gear weight", "penalties use the save's", "data. Original: older data."}};
		inline constexpr FRow GearDeathblows{"Gear Deathblows", {"After loading, pilots keep", "the Deathblows of their", "Gear. Original: another's."}};
		inline constexpr FRow AgilityShown{"Agility display", {"Status shows agility over 16", "as 16, as battles use it.", "Original: the raw value."}};
		inline constexpr FRow DeathblowBar{"Deathblow bar", {"A Deathblow learned to 100%", "shows a full bar. Original:", "the bar shows empty."}};
		inline constexpr FRow BillyAmmo{"Billy's ammo", {"Unequipping ammo keeps its", "shots. Original: shots can", "be lost."}};
		inline constexpr FRow EtherHealing{"Ether healing", {"Ethers heal and cost the", "same in menus as in battle.", "Original: as a basic Ether."}};
		inline constexpr FRow FixFrameHp{"Fix Frame HP", {"Repairs the caster's own", "Gear. Original: checks the", "first member's Gear."}};
		inline constexpr FRow NamesEndingInS{"Names ending in s", {"Names keep their final s.", "Original: it is cut off, so", "Crescens shows as Crescen."}};
		inline constexpr FRow MemberChangeEp{"EP in Member Change", {"Member Change shows EP, as", "every other screen does.", "Original: it shows MP."}};
		inline constexpr FRow ShopPointer{"Shop pointer", {"The pointer at Buy, Sell and", "Exit sits beside the words.", "Original: it covers them."}};
		inline constexpr FRow GearShopParts{"Gear shop parts", {"The Gear shop counts every", "part's bonus. Original: it", "skips some for a while."}};
		inline constexpr FRow GearShopPace{"Gear shop pace", {"The Gear in the shop turns", "at the normal pace. Original:", "three times faster."}};
		inline constexpr FRow SellingEquipped{"Selling equipped items", {"Selling worn equipment", "updates stats right away.", "Original: only later."}};
		inline constexpr FRow NewGameClock{"New game play time", {"A new game's play time starts", "at zero. Original: it counts", "from power-on."}};
		inline constexpr FRow InfinityChance{"Infinity chance", {"An undamaged Gear's Infinity", "chance is the same in every", "slot. Original: it varies."}};
		inline constexpr FRow ItemBuffs{"Item buffs", {"Item buffs last based on", "their user. Original: on the", "previous action."}};
		inline constexpr FRow EpAtLevel99{"EP at Ether level 99", {"EP keeps growing at Ether", "level 99. Original: it stops", "growing there."}};
		inline constexpr FRow EtherBuffs{"Ether buffs", {"Ether buffs work whatever", "the element. Original: some", "elements block them."}};
		inline constexpr FRow BuffsWearOff{"Buffs wear off", {"Atk, ACC and EV UP wear off", "after their turns. Original:", "they last the whole battle."}};
		inline constexpr FRow DefUpOnFoot{"Def UP on foot", {"Def UP raises defense by half", "on foot. Original: it does", "nothing on foot."}};
		inline constexpr FRow DefDownOnFoot{"Def DOWN on foot", {"Def DOWN lowers defense by a", "third on foot. Original: it", "does nothing on foot."}};
		inline constexpr FRow MissedStatusMoves{"Missed status moves", {"A status move that misses", "doesn't reset the status's", "duration. Original: it does."}};
		inline constexpr FRow ImmunityItems{"Igissol, Aegissol", {"Igissol and Aegissol grant", "their immunities. Original:", "they grant none."}};
		inline constexpr FRow GearElements{"Gear element changes", {"A Gear's element change wears", "off. Original: it stays, and", "clears other statuses."}};
		inline constexpr FRow GearStop{"Gear Stop", {"A stopped Gear's sure hits", "end with its Stop. Original:", "they last the whole battle."}};
		inline constexpr FRow GearPowerLoss{"Gear PWR loss", {"PWR loss lowers a Gear's", "attack until cured. Original:", "it does nothing."}};
		inline constexpr FRow LargeGearNumbers{"Large Gear numbers", {"Damage and healing on Gears", "past 65,535 stay correct.", "Original: they wrap around."}};
		inline constexpr FRow Counters{"Counters", {"A counter no longer stops a", "group attack or hits other", "enemies. Original: it can."}};
		inline constexpr FRow LastBossHp{"Last boss's HP", {"The final form has the HP its", "script gives it. Original:", "slightly less."}};
		inline constexpr FRow DeusFirstFight{"Deus's first fight", {"Deus's attack on one Gear", "can't freeze the game.", "Original: it can, at times."}};
		inline constexpr FRow EtherGuns{"Billy's ether guns", {"Ether guns fire the element", "of the ammo loaded. Original:", "always wind and fire."}};
		inline constexpr FRow HiddenEnemies{"Hidden enemies", {"An enemy that reveals itself", "keeps its group working.", "Original: the group breaks."}};
		inline constexpr FRow WeaponTrails{"Weapon trails", {"Weapon trails blend with the", "scene. Original: they cover", "what is behind them."}};
		inline constexpr FRow SpellLighting{"Spell lighting", {"Spells with no lighting of", "their own use the stage's.", "Original: whatever was last."}};
		inline constexpr FRow StageColors{"Stage colors", {"Tints keep the stage's own", "colors. Original: flames can", "change color."}};
		inline constexpr FRow BattleEvents{"Battle events", {"Scripted battle events start", "every actor they name.", "Original: one can be skipped."}};
		inline constexpr FRow EventDefeats{"Event defeats", {"Losing a scripted battle", "counts as a defeat. Original:", "it can end otherwise."}};
		inline constexpr FRow Landmarks{"Landmarks after battle", {"Story landmarks stay after a", "battle. Original: hidden", "until you re-enter the area."}};
		inline constexpr FRow SeaBlastSound{"Sea blast sound", {"In the blast over the sea,", "the rushing sound plays too.", "Original: it is cut off."}};
		inline constexpr FRow MahanonFalls{"Mahanon's falls", {"The waterfalls get all their", "particles. Original: thinner", "falls."}};
		inline constexpr FRow OverlappingTrees{"Overlapping trees", {"Overlapping trees keep their", "order. Original: they swap as", "the camera moves."}};
		inline constexpr FRow CpuRangedMoves{"CPU ranged moves", {"The CPU picks ranged moves", "by their own strength.", "Original: by the next move's."}};
		inline constexpr FRow PracticeOpponents{"Practice opponents", {"Easy, Normal and Hard CPUs", "play at the level they name.", "Original: the menu's level."}};
		inline constexpr FRow PracticeFrameRate{"Practice frame rate", {"Practice runs at the frame", "rate its label shows.", "Original: slower than shown."}};
	}

	namespace Controls
	{
		inline constexpr std::string_view Device = "Device";
		inline constexpr std::string_view Keyboard = "Keyboard";
		inline constexpr std::string_view Controller = "Controller";
		inline constexpr std::string_view ButtonsHeading = "Buttons";
		inline constexpr std::string_view CameraHeading = "Camera";
		/** What a key or button is called on each device: in "{}" below and on the reset row. */
		inline constexpr std::string_view KeyNoun = "key";
		inline constexpr std::string_view ButtonNoun = "button";

		/** Each PlayStation button's row, named by what it does first, with what it does. */
		inline constexpr FRow Up{"Up", {"Moves up.", "", ""}};
		inline constexpr FRow Down{"Down", {"Moves down.", "", ""}};
		inline constexpr FRow Left{"Left", {"Moves left.", "", ""}};
		inline constexpr FRow Right{"Right", {"Moves right.", "", ""}};
		inline constexpr FRow Confirm{"Confirm", {"Confirm, talk, fierce attack.", "", ""}};
		inline constexpr FRow Cancel{"Cancel", {"Cancel, run or walk.", "", ""}};
		inline constexpr FRow Menu{"Menu", {"Opens the menu, strong attack.", "", ""}};
		inline constexpr FRow Jump{"Jump", {"Jump, weak attack.", "", ""}};
		inline constexpr FRow CameraLeft{"Camera left", {"Rotates camera, slows battle.", "", ""}};
		inline constexpr FRow CameraRight{"Camera right", {"Rotates camera, speeds battle.", "", ""}};
		inline constexpr FRow GearLeft{"Gear L", {"With Gear R: get on/off Gear.", "", ""}};
		inline constexpr FRow GearRight{"Gear R", {"With Gear L: get on/off Gear.", "", ""}};
		inline constexpr FRow Pause{"Pause", {"Pauses the game.", "", ""}};
		inline constexpr FRow Map{"Map", {"Shows the map.", "", ""}};
		/** The free camera's rows. */
		inline constexpr FRow Rotate{"Rotate", {"Rotates the camera.", "", ""}};
		inline constexpr FRow Tilt{"Tilt", {"Tilts the camera up/down.", "", ""}};
		inline constexpr FRow Zoom{"Zoom", {"Zooms the camera in/out.", "", ""}};
		inline constexpr FRow ResetCamera{"Reset camera", {"Resets zoom and rotation.", "", ""}};
		/** A key row's second line: the rows that clear and swap, by their labels. */
		inline constexpr std::string_view ClearAndSwap = "{} to clear, {} to swap.";

		/** The help's "{}": the device's noun, then the Cancel row's label. */
		inline constexpr FRow AlwaysRun{"Always run", {"Run without holding a {}.", "Hold {} to walk.", ""}};
		inline constexpr FRow AnalogMovement{"Analog movement", {"Walk in any direction with the", "stick, slower when pushed", "lightly."}};
		/** The help's "{}": the Confirm row's label. */
		inline constexpr FRow WalkIntoDoors{"Walk into doors", {"Doors open as you walk into", "them. When off, press {}.", ""}};
		inline constexpr FRow OriginalButtonIcons{"Original PSX icons", {"Shows PlayStation button", "icons. Your bindings still", "apply."}};
		inline constexpr FRow ButtonLayout{"Button layout", {"Swaps confirm and cancel.", "Japan swaps jump and menu too.", ""}};
		/** In EButtonLayout's order. */
		inline constexpr std::array<std::string_view, 3> ButtonLayoutValues{"USA", "Circle confirms", "Japan"};
		/** What the layout's row tells when a pair the player bound keeps its buttons. */
		inline constexpr std::string_view OwnButtonsKept = "Custom bindings are kept.";
		inline constexpr FRow MouseSensitivity{"Mouse sensitivity", {"How fast the mouse rotates", "the camera.", ""}};
		inline constexpr FRow StickSensitivity{"Stick sensitivity", {"How fast the stick rotates", "the camera.", ""}};
		/** The touch screen's pad. */
		inline constexpr std::string_view TouchHeading = "Touch screen";
		inline constexpr FRow TouchMovement{"Movement", {"Use a d-pad or an analog", "stick on the touch screen.", ""}};
		/** In ETouchMovement's order. */
		inline constexpr std::array<std::string_view, 2> TouchMovementValues{"D-pad", "Stick"};
		inline constexpr FRow TouchOpacity{"Opacity", {"How opaque the on-screen", "buttons are.", ""}};
		inline constexpr FRow TouchVibration{"Vibration", {"Vibrates briefly when you", "press an on-screen button.", ""}};
		/** The reset row's label, and what it says when done, for the keyboard and for a controller. */
		inline constexpr std::string_view ResetKeys = "Reset keys";
		inline constexpr std::string_view ResetButtons = "Reset buttons";
		/** The help's "{}": the device's noun. */
		inline constexpr std::array<std::string_view, 3> ResetHelp{"Restores the default {}s.", "", ""};
		inline constexpr std::string_view KeysReset = "Default keys restored.";
		inline constexpr std::string_view ButtonsReset = "Default buttons restored.";

		/** Listening for a key: what to press ("{}" the noun), then how to stop. */
		inline constexpr std::string_view PressKey = "Press a {} to assign.";
		inline constexpr std::string_view StopListening = "Esc or wait to cancel.";
		/** Listening for an axis: the way asked for, on the mouse and on a stick. */
		inline constexpr std::string_view MoveMouseRight = "Move the mouse right.";
		inline constexpr std::string_view MoveMouseUp = "Move the mouse up.";
		inline constexpr std::string_view TurnWheelUp = "Turn the wheel up.";
		inline constexpr std::string_view PushStickRight = "Push a stick right.";
		inline constexpr std::string_view PushStickUp = "Push a stick up.";
		inline constexpr std::string_view PushStickDown = "Push a stick down.";

		/** What a key action told: "{}" the row whose only key it is, or the noun. */
		inline constexpr std::string_view OnlyKey = "Needed by {}.";
		inline constexpr std::string_view CannotClear = "Can't clear the only {}.";
		inline constexpr std::string_view Inverted = "Inverted.";
	}

	namespace Saves
	{
		inline constexpr FRow Import{"Import saves", {"Copies saves from a memory", "card image or save file into", "one of your cards."}};
		inline constexpr FRow Export{"Export card", {"Saves a card as a memory card", "image for emulators, in the", "Cards folder."}};
		/** A card's name: its number. */
		inline constexpr std::string_view Card = "Card {}";

		/** The file browser: the folder above, a folder or drive's value, and a folder with nothing to show. */
		inline constexpr FRow Up{"..", {"Up one folder.", "", ""}};
		inline constexpr std::string_view Folder = "Folder";
		inline constexpr std::string_view Drive = "Drive";
		inline constexpr std::string_view NoFolders = "No folders";

		/** The import: the card the saves go into, the copy (its "{}" how many), and each save picked. */
		inline constexpr FRow Into{"Into", {"The card to copy the saves", "to. Each keeps its slot", "number."}};
		inline constexpr FRow Copy{"Copy {}", {"Press Confirm to copy.", "", ""}};
		inline constexpr std::string_view OneSave = "1 save";
		inline constexpr std::string_view ManySaves = "{} saves";
		/** The copy's value and help when it would overwrite saves: how many, then the card; the saves' numbers fill the second line. */
		inline constexpr std::string_view Overwrites = "Overwrites {}";
		inline constexpr std::array<std::string_view, 3> OverwriteHelp{"This overwrites in {}:", "", "Press Confirm twice to copy."};
		/** The same after the first Confirm: the saves' numbers fill the second line, the card is the third's "{}". */
		inline constexpr std::array<std::string_view, 3> OverwriteAsked{"Confirm again to overwrite", "", "in {}. Cancel keeps them."};
		/** A save's help: its number and its play time. */
		inline constexpr std::array<std::string_view, 3> SaveHelp{"Slot {}, play time", "{}.", ""};

		/** What the pages tell after an action: "{}" a count of saves, then a card. */
		inline constexpr std::array<std::string_view, 3> Unreadable{"Couldn't read the file.", "", ""};
		inline constexpr std::array<std::string_view, 3> NotACard{"Not a memory card image", "or save file.", ""};
		inline constexpr std::array<std::string_view, 3> NoSaves{"No Xenogears saves found.", "", ""};
		inline constexpr std::array<std::string_view, 3> CopyFailed{"A save couldn't be copied.", "", ""};
		inline constexpr std::array<std::string_view, 3> Copied{"Copied {} into", "{}.", ""};
		inline constexpr std::array<std::string_view, 3> DamagedCard{"A save on the card is", "damaged. Nothing was written.", ""};
		inline constexpr std::array<std::string_view, 3> WriteFailed{"Couldn't write the card", "image.", ""};
		inline constexpr std::array<std::string_view, 3> Exported{"Saved as {}.mcr in", "the Cards folder.", ""};

		/**
		 * Where the game reaches the device's files only through its file picker (Android): the two rows' help there,
		 * then the device's own rows under their heading.
		 */
		inline constexpr std::array<std::string_view, 3> ImportPickedHelp{"Copies saves from a memory", "card image or save file into", "one of your cards."};
		inline constexpr std::array<std::string_view, 3> ExportPickedHelp{"Saves a card as a memory card", "image for emulators.", ""};
		inline constexpr std::string_view DeviceHeading = "This device";
		inline constexpr FRow AddDiscs{"Add disc images", {"Copies disc images into", "Weltall's folder.", ""}};
		inline constexpr FRow AddDiscFolder{"Add disc folder", {"Copies new disc images from a", "folder. Download itself can't", "be picked: use a subfolder."}};
		inline constexpr FRow BackUp{"Back up saves", {"Saves all your saves to a", "zip file.", ""}};
		inline constexpr FRow Restore{"Restore saves", {"Restores your saves from a", "backup zip file.", ""}};
		inline constexpr FRow SaveLog{"Save the log", {"Saves a copy of the log for a", "bug report.", ""}};
		/** Restore's help after the first Confirm. */
		inline constexpr std::array<std::string_view, 3> RestoreAsked{"This replaces your saves.", "Press Confirm again to pick", "the backup."};

		/** A transfer under way: "{}" the file's number and how many, its name, then how much is copied. */
		inline constexpr std::array<std::string_view, 3> Picking{"Waiting for the file picker.", "", ""};
		inline constexpr std::array<std::string_view, 3> Copying{"Copying file {} of {}:", "{}", "{}% done."};
		/** A folder's disc images found before any is copied: the folder's name, then how many so far. */
		inline constexpr std::array<std::string_view, 3> ScanningDiscs{"Scanning folder:", "{}", "{} disc files found."};
		/** What a transfer told at its end: "{}" a count of files, or the file copied out. */
		inline constexpr std::string_view OneFile = "1 file";
		inline constexpr std::string_view ManyFiles = "{} files";
		inline constexpr std::array<std::string_view, 3> DiscsCopied{"Copied {} into", "Weltall's folder.", ""};
		inline constexpr std::array<std::string_view, 3> BackedUp{"Backed up {}.", "", ""};
		inline constexpr std::array<std::string_view, 3> Restored{"Restored {}.", "", ""};
		inline constexpr std::array<std::string_view, 3> LogSaved{"Saved a copy of the log.", "", ""};
		inline constexpr std::array<std::string_view, 3> CardSaved{"Saved {}.", "", ""};
		/** Why a transfer failed, in ETransferFailure's order from NoRoom (the Modding page's own two empty): "{}" the file's name. */
		inline constexpr std::array<std::array<std::string_view, 3>, 8> Failures{{
			{"Not enough storage space.", "", ""},
			{"Couldn't read the file", "{}", ""},
			{"Couldn't write the file", "{}", ""},
			{"That's not a backup of", "Weltall's saves.", ""},
			{"This device has no file", "picker.", ""},
			{"", "", ""},
			{"", "", ""},
			{"No new disc images found in", "that folder.", ""},
		}};
	}

	/** The port's words about the player's discs: under the game's disc prompts ("INSERT DISC 2"), and the screen shown without disc 1. */
	namespace Discs
	{
		/** The disc asked for is still not where the game looks for it. */
		inline constexpr std::string_view NotFound = "Not found in Games.";
		/**
		 * Where the disc asked for is picked through the device's file picker: the copy ("{}" how much is done), the files
		 * picked holding no such disc, the copy failed, and the picker closed with nothing picked.
		 */
		inline constexpr std::string_view PromptCopying = "Copying: {}%";
		inline constexpr std::string_view PromptNotPicked = "Not the right disc.";
		inline constexpr std::string_view PromptNotCopied = "Couldn't copy them.";
		inline constexpr std::string_view PromptPickAgain = "Confirm to pick it.";

		/** The screen shown while neither disc is there: what is wrong, where the discs go ("{}" the folder), what was found and what not. */
		inline constexpr std::string_view Missing = "No Xenogears discs found.";
		inline constexpr std::string_view WhereTheyGo = "Put your disc images of the USA release (BIN/CUE, CHD, ISO or IMG) in this folder:\n{}\n"
														"The game starts as soon as one is found.";
		/** A disc found, "{}" its number then its file. */
		inline constexpr std::string_view Found = "Found disc {}: {}";
		inline constexpr std::string_view NotUsed = "Not used:";
		/** A file not used, "{}" its name then why. */
		inline constexpr std::string_view NotUsedFile = "{}: {}";
		/** After the files listed, "{}" how many more there are. */
		inline constexpr std::string_view More = "And {} more.";

		/**
		 * The same screen where the discs come in through the device's file picker (Android): what to do in place of where
		 * they go, the button that opens the picker, and what the copy tells under it.
		 */
		inline constexpr std::string_view MissingPicked = "To play, Weltall needs your own copy of Xenogears: disc images of the USA release or a supported "
														  "undub, as BIN/CUE, CHD, ISO or IMG files.\n\n"
														  "1. Put the disc images on this device in a folder inside Download, such as Download/Xenogears. "
														  "You can connect it to a computer by USB and copy them there.\n"
														  "2. Tap \"Add folder\" below and pick that folder, or tap \"Add disc images\" and pick the files. "
														  "For a BIN/CUE disc, pick its .cue file together with its .bin files. Android doesn't let apps "
														  "pick the Download folder itself.\n\n"
														  "Weltall copies them into its own folder, and the game starts once both discs are there. You can "
														  "delete the copies in Download afterwards.";
		inline constexpr std::string_view AddButton = "Add disc images";
		inline constexpr std::string_view AddFolderButton = "Add folder";
		/** A picked folder's disc images found before any is copied: "{}" the folder's name, then how many so far. */
		inline constexpr std::string_view Scanning = "Scanning {}: {} disc files found.";
		/** One disc of the two there: which ("{}" its number, then the other's), and the button that starts with it alone. */
		inline constexpr std::string_view OneDiscReady = "Disc {} added. Add Disc {} the same way, or tap \"Continue anyway\" to play with just Disc {} for "
														 "now.";
		inline constexpr std::string_view ContinueButton = "Continue anyway";
		/** "{}" the file's name, then how much is copied; with several files, its number and how many before. */
		inline constexpr std::string_view CopyingOne = "Copying {}: {}%";
		inline constexpr std::string_view CopyingMany = "Copying {} (file {} of {}): {}%";
		/** "{}" a count of files (Saves::OneFile, ManyFiles). */
		inline constexpr std::string_view Copied = "Copied {}.";
		/** "{}" the file's name, then why (one of Reasons); none named where the picker could not open. */
		inline constexpr std::string_view NotCopied = "Couldn't copy {}: {}";
		inline constexpr std::string_view NotCopiedNoName = "Couldn't copy the disc images: {}";
		/** A folder picked with no disc image the Games folder does not hold already: "{}" its name. */
		inline constexpr std::string_view NoNewDiscs = "No new disc images found in {}.";
		/** Why a transfer failed, in ETransferFailure's order from NoRoom. */
		inline constexpr std::array<std::string_view, 5> Reasons{
			"not enough storage space.",
			"the file couldn't be read.",
			"the file couldn't be written.",
			"it's not a backup of Weltall's saves.",
			"this device has no file picker.",
		};
	}

	/**
	 * The question asked after the second disc's prompt, where the port has the expanded second disc: its line, its
	 * two answers (the left one, the original's, then the right one, the expanded second disc's, where the pointer
	 * starts), and what each means, shown while the pointer is on it.
	 */
	namespace SecondDisc
	{
		inline constexpr std::string_view Question = "Which version of Disc 2?";
		inline constexpr std::string_view Original = "Original";
		inline constexpr std::string_view Extended = "Extended";
		inline constexpr std::array<std::string_view, 3> OriginalHelp{"Disc 2 as in the original,", "much of it told through", "narration."};
		inline constexpr std::array<std::string_view, 3> ExtendedHelp{"The narrated parts become", "playable: new scenes, places", "and battles."};

		/** The Fixes page's row in a game that tells it extended: Confirm twice tells the rest as the original, for good. */
		inline constexpr FRow Row{"Disc 2", {"The rest of Disc 2 plays as", "new scenes. Press Confirm to", "switch to the original."}};
		/** Its help once Confirm was pressed, and once the game was switched where it stays. */
		inline constexpr std::array<std::string_view, 3> SwitchAsked{"Press Confirm again to switch.", "You can't switch back to", "Extended afterwards."};
		inline constexpr std::array<std::string_view, 3> Switched{"Disc 2 plays as the original", "from here on.", ""};
	}

	/** The screen shown over the game's last picture when it stops, or in its place when it cannot start: what happened, then why. */
	namespace Stop
	{
		inline constexpr std::string_view Stopped = "The game has stopped.";
		inline constexpr std::string_view CannotStart = "The game can't start.";
		/** Where the game's log is, "{}" its file. */
		inline constexpr std::string_view Log = "Details are in the log file:\n{}\n"
												"Please attach it to your bug report. When the game starts again, the log is kept in the same folder under "
												"a name with the date, for a few days.";
		/** Where the log cannot be reached from outside the game (Android): in place of Log, the button, and what it tells. */
		inline constexpr std::string_view LogPicked = "Details are in the log. Tap \"Save the log\" to save a copy for your bug report.";
		inline constexpr std::string_view SaveLogButton = "Save the log";
		inline constexpr std::string_view LogSaved = "Log saved.";
		/** "{}" why (one of Discs::Reasons). */
		inline constexpr std::string_view LogNotSaved = "Couldn't save the log: {}";
		inline constexpr std::string_view SavesKept = "Your saves are safe. Restart the game to continue from your last save.";
		/** The last line, "{}" the game's version. */
		inline constexpr std::string_view Version = "Weltall {}";
	}

	/** Text with each "{}" replaced by the next of the values from Next, which moves past them; one past End stays "{}". */
	inline std::string FillFrom(std::string_view Text, const std::string_view*& Next, const std::string_view* End)
	{
		std::string Filled;
		for (std::size_t At = 0; At < Text.size(); ++At)
		{
			if (Text.substr(At, 2) == "{}" && Next != End)
			{
				Filled += *Next++;
				++At;
				continue;
			}
			Filled += Text[At];
		}
		return Filled;
	}

	/** Text with each "{}" replaced by the next of Values, in order. */
	inline std::string Fill(std::string_view Text, std::initializer_list<std::string_view> Values)
	{
		const std::string_view* Next = Values.begin();
		return FillFrom(Text, Next, Values.end());
	}

	/** A help's lines with each "{}" replaced by the next of Values, the first line's first. */
	inline std::array<std::string, 3> Fill(const std::array<std::string_view, 3>& Lines, std::initializer_list<std::string_view> Values = {})
	{
		const std::string_view* Next = Values.begin();
		std::array<std::string, 3> Filled;
		for (std::size_t Line = 0; Line < Lines.size(); ++Line)
		{
			Filled[Line] = FillFrom(Lines[Line], Next, Values.end());
		}
		return Filled;
	}
}
