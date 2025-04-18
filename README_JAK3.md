# Orb Hunt 3

<!-- <img src="https://raw.githubusercontent.com/dallmeyer/OG-OrbHunt/main/ModImage2.png" height="400"> -->

Play via the [JakMods mod list](https://jakmods.dev)!

## Concept
The core idea is the same as the [Jak 1 Orb Hunt](README_JAK1.md) / [Jak 2 Orb Hunt](README_JAK2.md) - it's a collectathon across all the Jak 3 levels. Similar to Jak II, since the Precursor Orbs in Jak 3 only exist in a handful of levels, and there's only 600 in total, it just wasn't really enough to spread across the entire game. Soooo I added a few Orbs...

There are now X orbs in total spread across every level in the game - and I mean every level 😎

## Gameplay
- Normally in Jak 3, there are certain levels or areas that you can only reach at specific points in the storyline, and cannot revisit during post-game
- I've modified a handful of conditions throughout the game for loading levels and opening airlocks, added some new actors, and taken a few other creative liberties in changing gameplay mechanics that will allow you to travel to levels or parts of levels that would otherwise be inaccessible.
  - These changes guarantee that you can get both in and out of every level, which means more Orb hiding spots :)
- Many level-loading conditions depend on your storyline progression, and so to keep my modifications simpler, **the game starts in a sort of NG+ mode**. 
  <!-- - Your active mission will be to break into the Metalhead Nest and defeat the final boss (though you won't be able to right away...) -->
  - This means you'll have the JET-board, all the guns, most of the vehicles, and all the Dark Jak and Light Jak powers from the start

## Orbs

**All Orbs needed for 100% should be "in bounds"** meaning you should never need to clip through (what appear to be) solid walls/objects (though I can't stop you from doing it when possible)
- Taking a death should never be necessary (good luck with deathless), though a few orbs might require Save+Load / Restart Mission
- Some orbs might be inside other... things
- It shouldn't be possible to softlock yourself out of any orbs (though you might need to Save+Load / Restart Mission to reset things like crates)
- You will need to work around some invisible collision/walls
- You will need knowledge of speedrunning tech, such as dealing with slippery surfaces, extended uppercuts, boosted uppercuts, infinite jump ceilings, rocket uppercuts, proxies, Abahbounces, sliding Abahbounces, Concussor jumps, invulnerability glitches etc. You will need a solid grasp on jetboard mechanics, including things like extended frontflips, freefalling, and of course the hover glitch. I guess you could also use the Light Jak infinite flight glitch...

### Orb Guide
- 🟡 JET-board hover glitch hints - certain orbs have a yellow/gold tint, and indicate that you should (and probably must) use the [hover glitch](https://www.youtube.com/watch?v=gEZQjj_pVuY&t=364s) or the Light Jak infinite flight glitch to reach them
- 🟠 Normal Orbs - any normal-looking orbs should NOT require the hover glitch / Light Jak infinite flight glitch
- 🟣 Abahbounce hints - certain orbs have a blue/purple tint, and indicate that you should do an [Abahbounce](https://www.youtube.com/watch?v=G8fdBxKxocI) at that spot (probably to reach another Orb at a higher location)

### Minimap & Orb Tagging
- There's a minimap added to the L3 HUD for most levels
  - While in first-person you can aim at an orb and press any of X/Square/Circle/Triangle/R1 to "tag" the orb and add it to your map
    - You can also zoom in/out with L2 and R2 while in first person
  - Alternatively, you can toggle the ORB MAP ICONS option to see all the locations on your map from the beginning (see Difficulty Toggles below)

## Side Missions
- You can still collect orbs from side missions like the original game, and they DO count towards the per-level HUD counters
- When you activate a side mission, you will notice that the "open orbs" despawn - this is intentional to avoid any issues with level load slots
  - Once the side mission is no longer active, the open orbs should respawn.
<!-- - Note that the Stadium levels DO have open orbs, but they won't be spawned/cannot be collected while the race side missions are active. You'll have to find a way to load these levels without the race side missions being activated in order to get the open orbs 😉 -->

## Difficulty Toggles
Some collectables are well-hidden and/or annoying to reach - you may want to ease your pain by using some of the below options to change the difficulty (you can find them under Game Options > Orb Hunt Options)
- AUTOSAVE ON ORB - when enabled, autosave will kick in every time you collect an orb
- ORB GLOW - ported from Jak 1, accessibility feature that adds a glow effect around orbs
- ORB GHOSTS - when enabled, transparent "orb ghosts" will remain after collecting an orb (to help remind you of paths/what you've collected)
- CLOSEST ORB - when the HUD is activated, a color-coded text displays the distance from Jak to the closest nearby Precursor Orb (including orbs within crates or other actors)
- ORB MAP ICONS - when enabled, icons are shown on the map and minimap for Orbs in any active levels
  - when not enabled, you can still "tag" orbs manually while in first-person, by aiming at an orb and pressing any of X/Square/Circle/Triangle/R1
- PSEUDO SAVESTATES - set custom checkpoints that you can quickly reset to! These only work when Jak has a standpoint - you'll hear a ding to confirm/bonk if rejected
  - Hold R3 and press Dpad Down to store your current position
  - Hold R3 and press Dpad Up to reset back to your previously stored position
    - Note: this will also reset actors (green eco crates, enemies, etc), similar to dying or loading a save file
  - Note: if you reboot into _debug mode_, be sure `Game Mode` is enabled, not `Debug Mode`
- EASY ABAHBOUNCE - when enabled, you don't need to time the X press during Abahbounces - simply hold X during a slippery ground pound, and you'll get a free optimal Abahbounce
- COLLISION RENDERER TOGGLE - when enabled, press Select to toggle Collision Renderer mode on/off
- SLIPPERY ASSIST
  - (NORMAL - normal physics)
  - LESS SLIP - Jak gets standpoints on most surfaces that he would normally slip off (just not steep surfaces/walls)
  - SPIDERMAN - Jak gets standpoints on any surface/wall
- HOVER RESTRICTION - when enabled, the game enforces a hoverless mode for most Orbs
  - Only the yellow/gold "hover Orbs" can be picked up via hovering / Light Jak infinite flight glitch
  - After any hovering or Light Jak infinite flight, adds a 30s cooldown window during which you cannot collect *non-hover* Orbs
    - Collecting a *hover* Orb will instantly clear the cooldown
