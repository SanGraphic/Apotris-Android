#include "scene.hpp"

std::string getDescription(std::string element) {

    // MODES
    // ---------------------------------------------------------------------------------
    if (element == "Marathon") {
        return "Try to get the best score as the game gets faster and faster.";

    } else if (element == "Sprint") {
        return "Clear lines to reach the goal as fast as possible.";

    } else if (element == "Dig") {
        return "Dig through lines of garbage.";

    } else if (element == "Ultra") {
        return "Score as much as possible in a limited time.";

    } else if (element == "Blitz") {
        return "Get the best score in a limited time while the game gets "
               "faster and faster.";

    } else if (element == "Combo") {
        return "Aim for the most consecutive clears in a 4-wide well.";

    } else if (element == "Survival") {
        return "Survive as long as possible while garbage rises from below.";

    } else if (element == "Classic") {
        return "Old school stacking mechanics.";

    } else if (element == "Master") {
        return "Get the highest Grade while the game gets aggressively faster.";

    } else if (element == "Death") {
        return "Try to survive at brutal speeds.";

    } else if (element == "Zen") {
        return "The goal is to relax...";

    } else if (element == "2P Battle") {
        return "Battle your friends.";

    } else if (element == "CPU Battle") {
        return "Go against an AI opponent.";

    } else if (element == "Training") {
        return "Try out new strategies or improve your playing.";

        // Graphics
        // ---------------------------------------------------------------------------------

        // AUDIO
        // ---------------------------------------------------------------------------------
    } else if (element == "Announcer") {
        return "Announcer SFX when you clear lines.";

    } else if (element == "Playback") {
        return "Cycle through songs or Loop the same song.";

    } else if (element == "Piece Move SFX") {
        return "Play a sound affect when you move the falling piece. Initial "
               "only plays a sound for the start of the movement.";

    } else if (element == "Track List") {
        return "Enable/Disable music tracks.";

        // CONTROL
        // -------------------------------------------------------------------------------
    } else if (element == "A+B to Hold") {
        return "Hold when pressing A and B.";

    } else if (element == "Quick Restart") {
        return "Press/Hold L and R to restart the game.";

    } else if (element == "Rumble") {
        return "Configure rumble strength, modes, and more.";

    } else if (element == "Adv. Rumble") {
        return "Advanced options such as rumble cart type, update rate, etc.";

    } else if (element == "Test Rumble") {
        return "Test Rumble with patterns: Drop, Clear 1-4, B2B Clear 4, Clear 8, Zone 1-3";

    } else if (element == "Rumble Strength") {
        return "Strength of rumble on compatible hardware.";

    } else if (element == "Zone Rumble Mode") {
        return "Optionally enable Zone rumble and select either continuous in background, or rippling on piece drop.";

    } else if (element == "Enable GBP Support") {
        return "Enable Game Boy Player handshake for GameCube Controller rumble. (REQUIRES RESTART!)";

    } else if (element == "Patt. Update Rate") {
        return "Set the update rate of the rumble patterns.";

    } else if (element == "Rumble Cart Type") {
        return "Set Cartridge Rumble hardware type. A: ROM GPIO (most common!) | B: DS Rumble Pak | C: EZFlash and others.";

    } else if (element == "Rumble Style") {
        return "Select between Dynamic rumble or Simple (old) rumble styles.";

    } else if (element == "EZFlash Motor") {
        return "Optionally send motor strength command to EZFlash rumble carts. "
               "(EZ3-in-1/EZODE Only! Separate from Rumble Strength setting!)";

    } else if (element == "Reset Controls") {
        return "Set bindings back to default.";

        // HANDLING
        // ------------------------------------------------------------------------------
    } else if (element == "DAS") {
        return "Delay before auto repeat starts.";

    } else if (element == "ARR") {
        return "How often auto repeat activates.";

    } else if (element == "SDF") {
        return "Soft Drop Auto Repeat Rate.";

    } else if (element == "Delay Soft Drop") {
        return "Whether DAS affects Soft Drop or not.";

    } else if (element == "Drop Protection") {
        return "Disables back-to-back hard drops for the set frames.";

    } else if (element == "Directional Delay") {
        return "Reset DAS on direction change.";

    } else if (element == "Disable Diagonals") {
        return "STRICT ignores diagonals completely, SOFT registers the second "
               "direction when the first gets released.";

    } else if (element == "Initial Hold") {
        return "Buffer hold during delays.";

    } else if (element == "Initial Rotation") {
        return "Buffer rotations during delays.";

    } else if (element == "Initial System") {
        return "TYPE A always buffers actions, TYPE B only buffers if you "
               "started holding the button during a delay.";

    } else if (element == "Reset Handling") {
        return "Set handling options back to default.";

        // GAMEPLAY
        // ------------------------------------------------------------------------------
    } else if (element == "Previews") {
        return "Number of upcoming pieces to show.";

    } else if (element == "Pro Mode") {
        return "Removes clear delays and enables features for advanced "
               "players.";

    } else if (element == "Goal Line") {
        return "Shows the remaining lines to clear.";

    } else if (element == "Spawn Preview") {
        return "Shows where the next piece will spawn so you can avoid "
               "blocking out.";

    } else if (element == "Rotation System") {
        return "Changes how minos rotate.";

    } else if (element == "Randomizer") {
        return "Changes the method of generating random minos.";

    } else if (element == "Big Mode") {
        return "Minos are twice as big! (Disables Highscores)";

    } else if (element == "Peek Above") {
        return "Moves the camera up when trying to place near the top of the "
               "field.";

    } else if (element == "Countdown on Unpause") {
        return "Enables a grace period countdown when unpausing.";

    } else if (element == "Interrupt Countdown") {
        return "Allow interrupting the countdown with a rotation input (1P only).";

        // SAVING
        // ------------------------------------------------------------------------------
    } else if (element == "Save Type") {
        return "Autodetected save type.";

    } else if (element == "Chip Size") {
        return "Autodetected save chip size (has no effect on features).";

    } else if (element == "Autosave") {
        return "How often the game saves automatically (doesn't save while in "
               "a round).";
        // VIDEO
        // ------------------------------------------------------------------------------
    } else if (element == "Integer Scale") {
        return "Fixes uneven pixel sizes at the cost of less control over "
               "scaling.";

    } else if (element == "Shaders") {
        return "Enables GPU shaders (Requires OpenGL).";
#ifdef N3DS
        // 3DS-specific scaling options:
    } else if (element == "1x") {
        return "Pixel-perfect display centered on the screen.";

    } else if (element == "1.5x smooth") {
        return "Fit height with linear filtering.";

    } else if (element == "1.5x sharp") {
        return "Sharper linear filtering with 2x prescale.";

    } else if (element == "1.5x ultra") {
        return "Use double the horizontal pixels for crisp output; "
               "vertically same as \"1.5x sharp\". "
               "Old 2DS not supported.";
#endif
        // Sleep
        // ------------------------------------------------------------------------------
    } else if (element == "Autosleep Time") {
        return "Put the console to sleep if no input is detected for the chosen duration.";
    } else if (element == "Autosleep Wake") {
        return "Choose to allow waking from Autosleep with any button or only the Sleep Combo.";
    } else if (element == "Sleep Combo") {
        return "Button combination to Sleep/Wake the console while paused or in a menu.";
    }

    return "";
}

std::string getDescription(std::string mode, std::string element,
                           std::string option) {
    if (mode == "Marathon") {
        if (element == "Type") {
            if (option == "Normal") {
                return "The traditional marathon experience.";

            } else if (option == "Zone") {
                return "Clearing lines charges the Zone meter. Activate Zone "
                       "to slow time and clear as many times as you can.";
            } else if (option == "Tower") {
                return "Build as high as possible leaving no empty space.";
            }
        } else if (element == "Level") {
            return "Starting Level.";

        } else if (element == "Lines") {
            return "How many lines to clear before the game ends.";
        }
    } else if (mode == "Sprint") {
        if (element == "Type") {
            if (option == "Normal") {
                return "Reach the goal by clearing lines.";

            } else if (option == "Attack") {
                return "Reach the goal by sending garbage.";
            }
        } else if (element == "Lines") {
            return "Goal lines.";
        }
    } else if (mode == "Dig") {
        if (element == "Type") {
            if (option == "Normal") {
                return "Clear the garbage lines as fast as possible.";

            } else if (option == "Efficiency") {
                return "Clear the garbage lines using as few pieces as "
                       "possible.";
            }
        } else if (element == "Lines") {
            return "Goal lines.";
        }
    } else if (mode == "Survival") {
        if (element == "Difficulty") {
            return "How often garbage lines rise from the bottom.";
        }
    } else if (mode == "Classic") {
        if (element == "Type") {
            if (option == "A-TYPE") {
                return "Score as much as possible while the game gets faster "
                       "and faster.";

            } else if (option == "Efficiency") {
                return "Clear 25 lines while scoring as much as possible.";
            }
        } else if (element == "Level") {
            return "Starting Level.";

        } else if (element == "Height") {
            return "How high the starting garbage lines are.";
        }
    } else if (mode == "Master") {
        if (option == "Normal") {
            return "Mechanics that mirror the other normal modes.";

        } else if (option == "Classic") {
            return "Classic mechanics for Grandmaster fanatics.";
        }
    } else if (mode == "Master") {
        if (option == "Normal") {
            return "Mechanics that mirror the other normal modes.";

        } else if (option == "Classic") {
            return "Classic mechanics for Grandmaster fanatics.";
        }
    } else if (mode == "Training") {
        if (element == "Finesse Training") {
            return "Forces you to play with perfect finesse, showing the "
                   "correct move when you make a mistake.";

        } else if (element == "Level") {
            return "Starting Level. (Level 0 turns off gravity)";
        }
    }

    return "";
}
