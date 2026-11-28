#ifndef OPENGOLDBOX_OLD_USER_FOLDER_H
#define OPENGOLDBOX_OLD_USER_FOLDER_H

// On Windows the game's user data folder was %APPDATA%\Godot\app_userdata\OpenGold
// and is now %APPDATA%\OpenGold. Moves the old folder's settings, saves and other
// files into the new one, keeping any file the new folder already has, and
// removes the old folder once it is empty. Does nothing on other systems or
// when there is no old folder. Call before anything reads the user folder.
void move_old_user_folder();

#endif
