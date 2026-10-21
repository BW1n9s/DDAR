using Mutagen.Bethesda;
using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Skyrim;

if (args.Length != 1) throw new ArgumentException("Expected output Data directory");
var root = Path.GetFullPath(args[0]);
Directory.CreateDirectory(root);
var mod = new SkyrimMod(ModKey.FromNameAndExtension("DDAR.esp"), SkyrimRelease.SkyrimSE);
mod.ModHeader.Flags |= SkyrimModHeader.HeaderFlag.Small;
mod.ModHeader.Version = 44;
var quest = mod.Quests.AddNew();
quest.EditorID = "DDAR_MCMQuest";
quest.Name = "DD Appearance Replacer";
quest.Flags = Quest.Flag.StartGameEnabled;
quest.Priority = 10;
quest.FormVersion = 44;
quest.VirtualMachineAdapter = new QuestAdapter {Version = 5, ObjectFormat = 2, ExtraBindDataVersion = 2};
quest.VirtualMachineAdapter.Scripts.Add(new ScriptEntry {Name = "DDAR_MCM", Flags = ScriptEntry.Flag.Local});
var alias = new QuestAlias {ID = 0, Name = "Player", Type = QuestAlias.TypeEnum.Reference};
alias.ForcedReference.SetTo(new FormKey(ModKey.FromNameAndExtension("Skyrim.esm"), 0x14));
quest.Aliases.Add(alias);
quest.NextAliasID = 1;
var fragmentAlias = new QuestFragmentAlias {
    Property = new ScriptObjectProperty {Alias = 0}, Version = 5, ObjectFormat = 2
};
fragmentAlias.Property.Object.SetTo(quest.FormKey);
fragmentAlias.Scripts.Add(new ScriptEntry {Name = "DDAR_PlayerAlias", Flags = ScriptEntry.Flag.Local});
quest.VirtualMachineAdapter.Aliases.Add(fragmentAlias);
var path = Path.Combine(root, "DDAR.esp");
mod.WriteToBinary(path);
using var read = SkyrimMod.CreateFromBinaryOverlay(path, SkyrimRelease.SkyrimSE);
var check = read.Quests.Single();
if (check.EditorID != "DDAR_MCMQuest" || !check.Flags.HasFlag(Quest.Flag.StartGameEnabled) ||
    check.VirtualMachineAdapter?.Scripts.Single().Name != "DDAR_MCM" ||
    check.VirtualMachineAdapter.Aliases.Single().Scripts.Single().Name != "DDAR_PlayerAlias" ||
    check.Aliases.Single().ForcedReference.FormKey.ID != 0x14)
    throw new InvalidDataException("MCM quest/VMAD/player alias roundtrip failed");
Directory.CreateDirectory(Path.Combine(root, "SEQ"));
// xEdit's SEQ generator writes FixedFormID: the file master index, not the runtime
// load-order index and not the bare local ID. The writer finalized masters above.
var fileFormId = ((uint)read.ModHeader.MasterReferences.Count << 24) | quest.FormKey.ID;
File.WriteAllBytes(Path.Combine(root, "SEQ", "DDAR.seq"), BitConverter.GetBytes(fileFormId));
Console.WriteLine($"ESP roundtrip OK: {check.FormKey}, StartGameEnabled, DDAR_MCM, player alias + reload script; SEQ {fileFormId:X8}");
