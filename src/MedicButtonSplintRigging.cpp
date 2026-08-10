#include <Debug.h>

#include <core/Functions.h>

#include <kenshi/Enums.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/gui/TitleScreen.h>

#include "version.h"

#include <sstream>

// -----------------------------------------------------------------------
// Deferred splint rigging injection state
// When JOB_MEDIC is added via shift+click, we defer the splint rigging
// task injection by two frames so JOB_REPAIR_ROBOT can be queued first.
// The main loop hook flushes on the second frame.
//
// Location is stored as raw floats to avoid Ogre::Vector3 dllimport
// constructor/assignment linker errors.
// -----------------------------------------------------------------------
struct PendingSplintJob
{
    bool active;
    int framesRemaining;
    PlayerInterface *playerInterface;
    float locationX;
    float locationY;
    float locationZ;
};

static PendingSplintJob gPendingSplintJob = {false, 0, nullptr, 0.0F, 0.0F, 0.0F};

// -----------------------------------------------------------------------
// Hook: PlayerInterface::newPlayerTaskSelectedCharacters
// Pass-through only — splint rigging is injected via shift+click jobs.
// -----------------------------------------------------------------------
void (*newPlayerTaskSelectedCharacters_orig)(
    PlayerInterface *thisptr,
    TaskType taskType,
    const hand &targetH,
    Building *destinationIndoors,
    const Ogre::Vector3 &clickPosition,
    bool addDontClear
) = nullptr;

void newPlayerTaskSelectedCharacters_hook(
    PlayerInterface *thisptr,
    TaskType taskType,
    const hand &targetH,
    Building *destinationIndoors,
    const Ogre::Vector3 &clickPosition,
    bool addDontClear
)
{ newPlayerTaskSelectedCharacters_orig(thisptr, taskType, targetH, destinationIndoors, clickPosition, addDontClear); }

// -----------------------------------------------------------------------
// Hook: PlayerInterface::addJobSelectedCharacters
// Fires on shift+medic button click (adds as persistent job)
// -----------------------------------------------------------------------
void (*addJobSelectedCharacters_orig)(
    PlayerInterface *thisptr, TaskType task, RootObject *subject, bool shift, bool add, const Ogre::Vector3 &location
) = nullptr;

void addJobSelectedCharacters_hook(
    PlayerInterface *thisptr, TaskType task, RootObject *subject, bool shift, bool add, const Ogre::Vector3 &location
)
{
    addJobSelectedCharacters_orig(thisptr, task, subject, shift, add, location);

    if (task == JOB_MEDIC)
    {
        std::ostringstream oss;
        oss << "addJobSelectedCharacters: deferring splint rigging task"
            << " shift=" << (shift ? "true" : "false") << " add=" << (add ? "true" : "false") << " location=("
            << location.x << "," << location.y << "," << location.z << ")";
        DebugLog(oss.str());

        gPendingSplintJob.active = true;
        gPendingSplintJob.framesRemaining = 2;
        gPendingSplintJob.playerInterface = thisptr;
        gPendingSplintJob.locationX = location.x;
        gPendingSplintJob.locationY = location.y;
        gPendingSplintJob.locationZ = location.z;
    }
    else if (task == JOB_REPAIR_ROBOT)
    {
        if (gPendingSplintJob.active)
        {
            DebugLog(
                "addJobSelectedCharacters: JOB_REPAIR_ROBOT added, refreshing "
                "pending splint task"
            );
            gPendingSplintJob.framesRemaining = 2;
        }
        else
        {
            DebugLog(
                "addJobSelectedCharacters: JOB_REPAIR_ROBOT added, no pending "
                "injection. Adding pending splint task."
            );
            gPendingSplintJob.active = true;
            gPendingSplintJob.framesRemaining = 2;
            gPendingSplintJob.playerInterface = thisptr;
            gPendingSplintJob.locationX = location.x;
            gPendingSplintJob.locationY = location.y;
            gPendingSplintJob.locationZ = location.z;
        }
    }
}

// -----------------------------------------------------------------------
// Hook: GameWorld::_NV_mainLoop_GPUSensitiveStuff
// Flushes any pending splint rigging task on the second frame
// -----------------------------------------------------------------------
void (*GameWorld_mainLoop_orig)(GameWorld *thisptr, float time) = nullptr;

void GameWorld_mainLoop_hook(GameWorld *thisptr, float time)
{
    if (gPendingSplintJob.active)
    {
        gPendingSplintJob.framesRemaining--;
        std::ostringstream oss;
        oss << "mainLoop: pending splint task framesRemaining=" << gPendingSplintJob.framesRemaining;
        DebugLog(oss.str());
        if (gPendingSplintJob.framesRemaining <= 0)
        {
            DebugLog(
                "mainLoop: flushing pending splint task via "
                "newPlayerTaskSelectedCharacters"
            );

            auto &pendingLocation = reinterpret_cast<const Ogre::Vector3 &>(gPendingSplintJob.locationX);
            hand nullHand;
            newPlayerTaskSelectedCharacters_orig(
                gPendingSplintJob.playerInterface, SPLINT_ORDER, nullHand, nullptr, pendingLocation, true
            );
            gPendingSplintJob.active = false;
        }
    }

    GameWorld_mainLoop_orig(thisptr, time);
}

__declspec(dllexport) void startPlugin()
{
    DebugLog("v" MBSR_VERSION_STRING " loaded");

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
                                  KenshiLib::GetRealAddress(&PlayerInterface::newPlayerTaskSelectedCharacters),
                                  newPlayerTaskSelectedCharacters_hook, &newPlayerTaskSelectedCharacters_orig
                              ))
    {
        ErrorLog("Could not add newPlayerTaskSelectedCharacters hook!");
    }
    else
    {
        DebugLog("newPlayerTaskSelectedCharacters hook installed");
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
                                  KenshiLib::GetRealAddress(&PlayerInterface::addJobSelectedCharacters),
                                  addJobSelectedCharacters_hook, &addJobSelectedCharacters_orig
                              ))
    {
        ErrorLog("Could not add addJobSelectedCharacters hook!");
    }
    else
    {
        DebugLog("addJobSelectedCharacters hook installed");
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
                                  KenshiLib::GetRealAddress(&GameWorld::_NV_mainLoop_GPUSensitiveStuff),
                                  GameWorld_mainLoop_hook, &GameWorld_mainLoop_orig
                              ))
    {
        ErrorLog("Could not add main loop hook!");
    }
    else
    {
        DebugLog("main loop hook installed");
    }
}