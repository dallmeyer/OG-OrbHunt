# Orb Hunt II

<img src="https://raw.githubusercontent.com/dallmeyer/OG-OrbHunt/main/ModImage2.png" height="400">

Play via the [openGOAL mod-launcher](https://jakmods.dev)!

## Concept
The core idea is the same as the [Jak 1 Orb Hunt](README_JAK1.md) - it's a collectathon across all the Jak II levels. This time however, it's not just the vanilla collectables that are moved around 😈 Since the Precursor Orbs in Jak II only exist in a handful of levels, and there's only 286(ish) in total, it just wasn't really enough to spread across the entire game. Soooo I added a few Orbs...

There are now 3125 orbs in total spread across every level in the game - and I mean every level 😎

## Gameplay
- Normally in Jak II, there are certain levels or areas that you can only reach at specific points in the storyline, and cannot revisit during post-game
  - Vanilla OpenGOAL fixes some of these that softlock you out of Orbs, like the Metalhead Nest
  - I've built on top of that to guarantee you can get both in and out of every level, which means more Orb hiding spots :)
- I've modified a handful of conditions throughout the game for loading levels and opening airlocks, added some new actors, and taken a few other creative liberties in changing gameplay mechanics that will allow you to travel to levels or parts of levels that would otherwise be inaccessible. (hint: try Dark Bomb on stuff)
- Many level-loading conditions depend on your storyline progression, and so to keep my modifications simpler, **the game starts in a sort of NG+ mode**. 
  - Your active mission will be to break into the Metalhead Nest and defeat the final boss (though you won't be able to right away...)
  - This means you'll have the JET-board, all the guns, and all the Dark Jak powers from the start
- There's also a minimap added to the L3 HUD for most levels
  - This is particularly useful with the ORB MAP ICONS option (see Difficulty Toggles below)

## Orbs

**All Orbs needed for 100% should be "in bounds"** meaning you should never need to clip through (what appear to be) solid walls/objects (though I can't stop you from doing it when possible)
- Taking a death should never be necessary (good luck with deathless), though a few orbs might require Save+Load / Restart Mission
- Some orbs might be inside other... things
- It shouldn't be possible to softlock yourself out of any orbs (though you might need to Save+Load / Restart Mission to reset things like crates)
- You will need to work around some invisible collision/walls
- You will need knowledge of speedrunning tech, such as dealing with slippery surfaces, extended uppercuts, boosted uppercuts, infinite jump ceilings, rocket uppercuts, proxies, Abahbounces, sliding Abahbounces, etc. You will need a solid grasp on jetboard mechanics, including things like extended frontflips, freefalling, and of course the hover glitch.
  - You shouldn't need to use any invuln glitches (might be worth unlocking the invulnerabilty secret though...)

### Orb Guide
- 🟡 JET-board hover glitch hints - certain orbs have a yellow/gold tint, and indicate that you should (and probably must) use the [hover glitch](https://www.youtube.com/watch?v=gEZQjj_pVuY&t=364s) to reach them
- 🟠 Normal Orbs - any normal-looking orbs should NOT require the hover glitch
- 🟣 Abahbounce hints - certain orbs have a blue/purple tint, and indicate that you should do an [Abahbounce](https://www.youtube.com/watch?v=G8fdBxKxocI) at that spot (probably to reach another Orb at a higher location)

## Side Missions
- You can still collect orbs from side missions like the original game, and they DO count towards the per-level HUD counters
- When you activate a side mission, you will notice that the "open orbs" despawn - this is intentional to avoid any issues with level load slots
  - Once the side mission is no longer active, the open orbs should respawn.
- Note that the Stadium levels DO have open orbs, but they won't be spawned/cannot be collected while the race side missions are active. You'll have to find a way to load these levels without the race side missions being activated in order to get the open orbs 😉

## Difficulty Toggles
Some collectables are well-hidden and/or annoying to reach - you may want to ease your pain by using some of the below options to change the difficulty (you can find them under Game Options > Orb Hunt Options)
- AUTOSAVE ON ORB - when enabled, autosave will kick in every time you collect an orb
- CLOSEST ORB - color-coded text displays the distance from Jak to the closest nearby Precursor Orb (including orbs within crates or other actors)
- ORB MAP ICONS - when enabled, icons are shown on the map and minimap for Orbs in any active levels
- PSEUDO SAVESTATES - set custom checkpoints that you can quickly reset to! These only work when Jak has a standpoint - you'll hear a ding to confirm/bonk if rejected
  - Hold R3 and press Dpad Down to store your current position
  - Hold R3 and press Dpad Up to reset back to your previously stored position
    - Note: this will also reset actors (green eco crates, enemies, etc), similar to dying or loading a save file
  - Note: if you reboot into _debug mode_, be sure `Game Mode` is enabled, not `Debug Mode`
- SLIPPERY ASSIST
  - (NORMAL - normal physics)
  - LESS SLIP - Jak gets standpoints on most surfaces that he would normally slip off (just not steep surfaces/walls)
  - SPIDERMAN - Jak gets standpoints on any surface/wall
- HOVER RESTRICTION - when enabled, the game enforces a hoverless mode for most Orbs
  - Only the yellow/gold "hover Orbs" can be picked up via hovering
  - After any hovering, adds a 30s cooldown window during which you cannot collect *non-hover* Orbs
    - Collecting a *hover* Orb will instantly clear the cooldown
