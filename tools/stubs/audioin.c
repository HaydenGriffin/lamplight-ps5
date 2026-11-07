/* Lamplight for PS5 - Import stub for the console's libSceAudioIn (the controller's microphone).
 * Copyright (C) 2026 Hayden Griffin
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The payload SDK has no stub for this library. Only the names matter to the ELF converter, which
 * records them as imports from libSceAudioIn.sprx; the console's own library is what runs. */
int sceAudioInOpen(int user, int type, int index, unsigned length, unsigned rate, unsigned format)
{
    return -1;
}
int sceAudioInInput(int handle, void *buffer)
{
    return -1;
}
int sceAudioInClose(int handle)
{
    return -1;
}
