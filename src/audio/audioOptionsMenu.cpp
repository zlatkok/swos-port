#include "audioOptionsMenu.h"
#include "audio.h"
#include "chants.h"
#include "textInput.h"
#include "audioOptions.mnu.h"

using namespace AudioOptionsMenu;

static void refreshVolumeEntries();

void showAudioOptionsMenu()
{
    showMenu(audioOptionsMenu);
}

static void initAudioOptionsMenu()
{
    refreshVolumeEntries();
}

static void increaseVolume()
{
    if (getMasterVolume() < kMaxVolume)
        setMasterVolume(getMasterVolume() + 1);

    refreshVolumeEntries();
}

static void decreaseVolume()
{
    if (getMasterVolume() > kMinVolume)
        setMasterVolume(getMasterVolume() - 1);

    refreshVolumeEntries();
}

static void enterMasterVolume()
{
    auto entry = A5.as<MenuEntry *>();
    auto numberEntered = inputNumber(*entry, 3, kMinVolume, kMaxVolume);
    if (numberEntered)
        setMasterVolume(entry->fg.number);
}

static void increaseMusicVolume()
{
    if (getMusicVolume() < kMaxVolume)
        setMusicVolume(getMusicVolume() + 1);

    refreshVolumeEntries();
}

static void decreaseMusicVolume()
{
    if (getMusicVolume() > kMinVolume)
        setMusicVolume(getMusicVolume() - 1);

    refreshVolumeEntries();
}

static void enterMusicVolume()
{
    auto entry = A5.as<MenuEntry *>();
    auto numberEntered = inputNumber(*entry, 3, kMinVolume, kMaxVolume);
    if (numberEntered)
        setMusicVolume(entry->fg.number);
}

static void refreshVolumeEntries()
{
    volumeEntry.setNumber(getMasterVolume());
    musicVolumeEntry.setNumber(getMusicVolume());
}
