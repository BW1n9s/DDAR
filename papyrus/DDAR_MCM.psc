Scriptname DDAR_MCM extends SKI_ConfigBase

; Selection and settings persist in the save. Applying is always explicit after load.
Armor Property SourceArmor Auto
Armor Property TargetArmor Auto
Bool Property ShowOverlap = False Auto
Bool Property AllowExtra52 = False Auto
Int aPage = 0
Int bPage = 0
Int aOption
Int bOption
Int refreshOption
Int applyOption
Int restoreOption
Int clearOption
Int overlapOption
Int extraOption
Int aNext
Int bNext
Int aPrev
Int bPrev
Int detailsOption
Int snapshotOption

Event OnConfigInit()
    ModName = "DD Appearance Replacer"
    Pages = new String[5]
    Pages[0] = "Equipment mapping"
    Pages[1] = "Current mapping"
    Pages[2] = "Diagnostics"
    Pages[3] = "Settings"
    Pages[4] = "About"
EndEvent

Event OnConfigOpen()
    DDAR_Native.RefreshCatalog(SourceArmor, ShowOverlap)
EndEvent

String Function ArmorName(Armor item)
    If item
        Return item.GetName()
    EndIf
    Return "Select..."
EndFunction

Event OnPageReset(String page)
    aOption = -1
    bOption = -1
    refreshOption = -1
    applyOption = -1
    restoreOption = -1
    clearOption = -1
    overlapOption = -1
    extraOption = -1
    aNext = -1
    bNext = -1
    aPrev = -1
    bPrev = -1
    detailsOption = -1
    snapshotOption = -1
    SetCursorFillMode(TOP_TO_BOTTOM)
    If page == "Equipment mapping" || page == ""
        AddHeaderOption("A: ordinary armor in backpack")
        aOption = AddMenuOption("Appearance A", ArmorName(SourceArmor))
        AddTextOption("A page / entries", (aPage + 1) + " / " + DDAR_Native.GetCount(False))
        aPrev = AddTextOption("Previous A page", "<")
        aNext = AddTextOption("Next A page", ">")
        refreshOption = AddTextOption("Refresh catalogs", "Refresh")
        SetCursorPosition(1)
        AddHeaderOption("B: registered DD / DCL devices")
        bOption = AddMenuOption("Device B", ArmorName(TargetArmor))
        AddTextOption("B page / entries", (bPage + 1) + " / " + DDAR_Native.GetCount(True))
        bPrev = AddTextOption("Previous B page", "<")
        bNext = AddTextOption("Next B page", ">")
        applyOption = AddTextOption("Apply selected mapping", "Apply")
        restoreOption = AddTextOption("Disable / restore DD appearance", "Restore")
    ElseIf page == "Current mapping"
        AddHeaderOption("Single mapping test build")
        AddTextOption("A", ArmorName(SourceArmor))
        AddTextOption("B", ArmorName(TargetArmor))
        AddToggleOption("Native mapping active", DDAR_Native.IsActive(), OPTION_FLAG_DISABLED)
        applyOption = AddTextOption("Apply / refresh", "Apply")
        restoreOption = AddTextOption("Disable / restore", "Restore")
        clearOption = AddTextOption("Clear saved selection", "Delete")
    ElseIf page == "Diagnostics"
        AddHeaderOption("Runtime status and full slot sets")
        refreshOption = AddTextOption("Refresh catalog / status", "Refresh")
        detailsOption = AddTextOption("Status and full slot details", "Read")
        snapshotOption = AddTextOption("Write inventory / biped snapshot", "Log")
        AddTextOption("Source A (highlight for slots)", "A", OPTION_FLAG_DISABLED)
        AddTextOption("Target B (highlight for slots)", "B", OPTION_FLAG_DISABLED)
        SetInfoText(DDAR_Native.GetStatus() + "\nA: " + DDAR_Native.Describe(SourceArmor, False) + "\nB: " + DDAR_Native.Describe(TargetArmor, True))
    ElseIf page == "Settings"
        AddHeaderOption("Catalog and experimental compatibility")
        overlapOption = AddToggleOption("Include partially overlapping B slots", ShowOverlap)
        extraOption = AddToggleOption("Allow belt B extra slot 52", AllowExtra52)
        AddTextOption("Apply after loading save", "Manual only", OPTION_FLAG_DISABLED)
    ElseIf page == "About"
        AddHeaderOption("DDAR 0.0.3 - MCM test")
        AddTextOption("Runtime", "SE 1.5.97", OPTION_FLAG_DISABLED)
        AddTextOption("Backend", "Player / female / single belt ARMA", OPTION_FLAG_DISABLED)
        AddTextOption("Saved mappings", "One, stored in this save", OPTION_FLAG_DISABLED)
        AddTextOption("DD / DCL catalog", "Local DD NG API v2", OPTION_FLAG_DISABLED)
        AddTextOption("Verification", "In-game visual check required", OPTION_FLAG_DISABLED)
    EndIf
EndEvent

Event OnOptionMenuOpen(Int option)
    If option == aOption
        SetMenuDialogOptions(DDAR_Native.GetLabels(False, aPage))
    ElseIf option == bOption
        SetMenuDialogOptions(DDAR_Native.GetLabels(True, bPage))
    EndIf
    SetMenuDialogStartIndex(0)
    SetMenuDialogDefaultIndex(0)
EndEvent

Event OnOptionMenuAccept(Int option, Int index)
    If option == aOption
        SourceArmor = DDAR_Native.GetAt(False, aPage * 20 + index)
        TargetArmor = None
        bPage = 0
        DDAR_Native.RefreshCatalog(SourceArmor, ShowOverlap)
    ElseIf option == bOption
        TargetArmor = DDAR_Native.GetAt(True, bPage * 20 + index)
    EndIf
    ForcePageReset()
EndEvent

Event OnOptionSelect(Int option)
    If option == applyOption
        If !SourceArmor || !TargetArmor
            ShowMessage("Select A and B first.", False)
        ElseIf ShowMessage("Apply this B rendered mapping? Shared rendered forms affect all inventory devices using that form.\n" + DDAR_Native.Describe(TargetArmor, True))
            DDAR_Native.QueueChange(SourceArmor, TargetArmor, AllowExtra52, False)
            ShowMessage("Queued. Close all menus to apply. Reopen Diagnostics to read the result.", False)
        EndIf
    ElseIf option == detailsOption
        ShowMessage(DDAR_Native.GetStatus() + "\nA: " + DDAR_Native.Describe(SourceArmor, False) + "\nB: " + DDAR_Native.Describe(TargetArmor, True), False)
    ElseIf option == snapshotOption
        DDAR_Native.Snapshot()
        ShowMessage("Snapshot queued. Close menus; read DDARProbe.log.", False)
    ElseIf option == restoreOption
        DDAR_Native.QueueChange(None, None, False, True)
        ShowMessage("Restore queued. Close all menus to execute.", False)
    ElseIf option == clearOption
        If ShowMessage("Clear saved A/B selection and restore the original appearance?")
            SourceArmor = None
            TargetArmor = None
            DDAR_Native.QueueChange(None, None, False, True)
        EndIf
    ElseIf option == refreshOption
        DDAR_Native.RefreshCatalog(SourceArmor, ShowOverlap)
        ShowMessage(DDAR_Native.GetStatus() + "\nCatalog refresh requested; reopen the page when ready.", False)
    ElseIf option == overlapOption
        ShowOverlap = !ShowOverlap
        TargetArmor = None
        bPage = 0
        DDAR_Native.RefreshCatalog(SourceArmor, ShowOverlap)
    ElseIf option == extraOption
        If AllowExtra52
            AllowExtra52 = False
        ElseIf ShowMessage("Experimental: allow target belt ARMA slots 49 + 52 with source slot 49. Visual partitions and restoration require in-game verification. Enable?")
            AllowExtra52 = True
        EndIf
    ElseIf option == aPrev && aPage > 0
        aPage -= 1
    ElseIf option == aNext && (aPage + 1) * 20 < DDAR_Native.GetCount(False)
        aPage += 1
    ElseIf option == bPrev && bPage > 0
        bPage -= 1
    ElseIf option == bNext && (bPage + 1) * 20 < DDAR_Native.GetCount(True)
        bPage += 1
    EndIf
    ForcePageReset()
EndEvent

Event OnOptionHighlight(Int option)
    If option == aOption
        SetInfoText(DDAR_Native.Describe(SourceArmor, False))
    ElseIf option == bOption
        SetInfoText(DDAR_Native.Describe(TargetArmor, True))
    Else
        SetInfoText(DDAR_Native.GetStatus() + "\nA: " + DDAR_Native.Describe(SourceArmor, False) + "\nB: " + DDAR_Native.Describe(TargetArmor, True))
    EndIf
EndEvent
