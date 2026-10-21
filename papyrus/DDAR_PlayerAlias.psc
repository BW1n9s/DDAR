Scriptname DDAR_PlayerAlias extends ReferenceAlias
Event OnPlayerLoadGame()
    (GetOwningQuest() as DDAR_MCM).OnGameReload()
EndEvent
