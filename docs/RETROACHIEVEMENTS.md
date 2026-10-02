<sub>[SkyEmu OMEGA](../README.md) › [Docs](README.md) › RetroAchievements</sub>

# RetroAchievements

SkyEmu supports [RetroAchievements](https://retroachievements.org), which adds achievements, leaderboards and
rich presence to Game Boy, Game Boy Color, Game Boy Advance and Nintendo DS games.

## Getting started

1. Create an account on [retroachievements.org](https://retroachievements.org) (or press **Register** in the menu).
2. Open **Menu → RetroAchievements**, enter your username and password and press **Login**. SkyEmu keeps a login
   token, not your password, so you stay logged in.
3. Load a game. A notification shows how many of its achievements you have unlocked.

The trophy button in the menu bar opens the achievements panel: the game, your points, what you are doing in the
game (rich presence, as shown on your RetroAchievements profile) and every achievement grouped by progress, with
how many players have it.

## Modes

| Option | What it does |
|---|---|
| **Hardcore Mode** | Unlocks and points count as hardcore, and leaderboard entries are submitted. Cheats, the cheat finder, loading save states, rewind, slow motion, frame stepping, the debug tools and the HTTP control server are turned off. |
| **Encore Mode** | Achievements you have already unlocked can be unlocked again, to play through a set once more. The game restarts when you turn it on or off. |
| **Unofficial Achievements** | Also loads achievements that are still being tested by their authors. They are listed in their own group and their unlocks are not sent. |
| **Spectator Mode** | Achievements and leaderboards work as usual, but nothing is sent to RetroAchievements. Use it to try a set or watch someone play without unlocking anything. |

Changing **Unofficial Achievements** or **Spectator Mode** reloads the achievements of the running game; the game
itself keeps running. All four options are saved.

## On-screen

| Option | Shows |
|---|---|
| **Notifications** | Unlocks, leaderboard results and the game summary. **Only one notification at a time** shows them one after the other. |
| **Progress Indicators** | The progress of an achievement that counts something, such as *7/10 coins* |
| **Leaderboard Trackers** | The time or score of a running leaderboard attempt (Hardcore or Encore Mode) |
| **Challenge Indicators** | Achievements that are primed: a challenge is running and the achievement unlocks when it is completed, such as beating a boss without taking damage |

## ROM hacks and translations

The game is identified from the ROM as it is played, after a [ROM patch](CHEATS_AND_PATCHES.md#rom-patches) has been
applied. Translations and hacks that have their own achievement sets on RetroAchievements are recognized when you
play them with their patch.

## From another app

| What | [HTTP control server](HTTP_CONTROL_SERVER.md) | C API ([`skyemu_dll.h`](../src/skyemu_dll.h)) |
|---|---|---|
| User, game, rich presence and achievements | [`/achievements`](HTTP_CONTROL_SERVER.md#achievements) | `se_get_achievements_json()` |
| Log in, log out | | `se_ra_login()`, `se_ra_logout()`, `se_ra_get_login_state()`, `se_ra_get_login_error()` |
| Hardcore Mode | `hardcore_mode` with [`/setting`](HTTP_CONTROL_SERVER.md#setting) | `se_set_hardcore_mode()` |
| Unofficial Achievements | `ra_unofficial` | `se_set_ra_unofficial()` |
| Spectator Mode | `ra_spectator` | `se_set_ra_spectator()` |
| On-screen options | `draw_notifications`, `draw_progress_indicators`, `draw_leaderboard_trackers`, `draw_challenge_indicators`, `only_one_notification` | `se_set_draw_notifications()` and the others |

On Android, `MainSkyEmuObject` has the same calls with an `se_android_` prefix, for example
`se_android_ra_login(String, String)` and `se_android_get_achievements_json()`.

The HTTP control server is turned off in Hardcore Mode, except for `/achievements`, which only reads. There is no
HTTP command to log in, because the server is not encrypted and can be reached from other devices on your network.

<details>
<summary>Example <code>/achievements</code> response</summary>

```json
{
  "available": true,
  "logged_in": true,
  "user": {"username": "sky", "display_name": "sky", "score": 1250, "score_softcore": 40},
  "hardcore": true, "encore": false, "unofficial": false, "spectator": false,
  "game": {"id": 515, "title": "The Legend of Zelda: The Minish Cap", "hash": "…", "rich_presence": "Exploring Hyrule Town"},
  "summary": {"achievements": 72, "unlocked": 12, "unofficial": 0, "unsupported": 0, "points": 640, "points_unlocked": 95},
  "achievements": [
    {"id": 12345, "title": "…", "description": "…", "points": 5, "unlocked": true, "unlocked_hardcore": true,
     "unlock_time": 1735689600, "unofficial": false, "bucket": "Unlocked", "progress": "", "percent": 0.0,
     "rarity": 61.20, "rarity_hardcore": 48.75, "badge_url": "https://media.retroachievements.org/Badge/…png"}
  ]
}
```

Before a game is loaded `game`, `summary` and `achievements` are missing. When nobody is logged in the response
only has `logged_in`, `pending_login` and, after a failed login, `login_error`.

</details>
