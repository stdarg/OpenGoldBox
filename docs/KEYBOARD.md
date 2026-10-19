# Keyboard shortcuts

Every key OpenGoldBox answers to, by screen. Where a screen has no key of its
own, Godot's usual focus keys apply: Tab moves between controls, Enter or
Space presses the focused button, and the arrow keys move in a list.

On macOS, Ctrl is the Control key, not Command.

## Everywhere

| Key | Does |
| --- | --- |
| Ctrl+X | Quit the game. |
| Ctrl+S | Save a screenshot. |

Both work in every window. Neither advances the splash screens.

## Startup

| Key | Does |
| --- | --- |
| Any key | Show the next splash screen; after the second, go on to character creation. |
| Esc | Skip the splash screens. |
| Enter | Pick the highlighted language, or submit the game data folder. |

## Character creation

| Key | Does |
| --- | --- |
| Arrow keys | Move through the portrait grid when it has focus: Left and Right by one portrait, Up and Down by one row. |
| Esc | Close the full-size portrait (opened by double-clicking one). |

## Exploration

| Key | Does |
| --- | --- |
| Up | Step forward. |
| Down | Turn around. |
| Left / Right | Turn left / right. |
| L | Look. |
| C | Camp: rest, and use Cast / Use. |
| M | Cast menu: cast a spell or use a feature outside combat (the original's Magic menu). |
| Enter | The bottom-right button: Continue, Choose, Submit, Buy or Leave temple. |
| Any key | Leave the monster close-up and start the fight. |

These keys do nothing while the Inventory, the Cast menu or the Camp dialog is
open. While an answer is being typed, or while several choices are on screen,
only Enter works. M works only when the Cast button does.

### In a shop

| Key | Does |
| --- | --- |
| Tab | Choose the next party member as the buyer. |
| 1 to 8 | Choose that party member as the buyer (top-row number keys). |
| Enter | Buy the selected item. |

## Camp

| Key | Does |
| --- | --- |
| Esc | Finish resting and close the Camp dialog. |
| Esc, in the spell choice | Keep the current spells. |
| Esc, in the training choice | Keep the current training. |

## Party

| Key | Does |
| --- | --- |
| Esc, in Choose weapon hand | Close it. |

## Combat

None of these work with Ctrl held.

### Moving

| Key | Moves |
| --- | --- |
| Arrow keys | North, east, south, west. |
| Shift+Up / Shift+Right / Shift+Down / Shift+Left | North-east / south-east / south-west / north-west. |
| Keypad 1 to 9 (not 5) | In the eight directions. |
| Insert / Page Up / Delete / Page Down | North-west / north-east / south-west / south-east. |

### Acting

| Key | Does |
| --- | --- |
| A | Choose the next action. |
| Space | Use the chosen action: at once for one without a target (Rage, Wild Shape), otherwise on the selected creature. |
| Z | Switch between the first and second spell slot. |
| Enter | Continue past the encounter text; otherwise end the turn. |
| Q | Put the whole party on Quick (the computer fights for it). |
| M | Turn Quick magic on or off: whether members on Quick cast spells. |
| Space, while the computer fights for a member | Take back control of the party. |

### Choosing targets

| When | Key | Does |
| --- | --- | --- |
| Stabilize, Throw, Light or Nick | Left / Right | Change the target. |
| | Space | Use it. |
| | Esc | Go back to moving. |
| Aiming an area spell | Arrows, Shift+arrows, keypad | Move the aim point. |
| | Space or Enter | Cast. |
| | Esc | Cancel. |
| An area spell chosen, not yet aimed | Space or Enter | Start aiming. |
| Choosing a spell's creatures | Space | Cast. |
| | Esc | Cancel. |
| An optional effect's target | Any arrow | Choose the next target. |
| | Space or Enter | Use it. |
| | Esc | Skip it. |
| Free movement | Esc | Finish moving. |

### Combat windows

| Window | Key | Does |
| --- | --- | --- |
| Initiative choice | Esc | Keep the current order. |
| Optional effect | Esc | Skip it. |
| Nick attack | Esc | Cancel. |

While one of these windows is open, or while the party flees, the combat keys
above do nothing. When an action button or a list has focus, Enter and Space
press that button or pick from that list instead.

## Screens without keys of their own

Level-up, saving and loading, the Character Pool, the character sheets and the
Defeat window use the mouse and Godot's focus keys only.

## Known gaps

- M opens the Cast menu while exploring but turns Quick magic on or off in
  combat.
- In exploration, Ctrl+L and Ctrl+C look and camp too, since the keys ignore
  modifiers.
- The combat footer does not mention Q and M until the screen first refreshes.
- While choosing creatures for a spell that targets several, the hint mentions
  only Esc, though Space casts.
