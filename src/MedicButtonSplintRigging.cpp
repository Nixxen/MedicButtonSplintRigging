#include <Debug.h>

#include <core/Functions.h>

#include <kenshi/Enums.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/PlayerInterface.h>

#include "version.h"

#include <sstream>

const static bool kDebugLogEnabled = false;

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

static void InitPendingSplintJob(PlayerInterface *playerInterface, const Ogre::Vector3 &loc)
{
    gPendingSplintJob.active = true;
    gPendingSplintJob.framesRemaining = 2;
    gPendingSplintJob.playerInterface = playerInterface;
    gPendingSplintJob.locationX = loc.x;
    gPendingSplintJob.locationY = loc.y;
    gPendingSplintJob.locationZ = loc.z;
}

// Resolved via GetRealAddress — no hook needed; called directly from main loop
void (*newPlayerTaskSelectedCharacters_orig)(
    PlayerInterface *thisptr,
    TaskType taskType,
    const hand &targetH,
    Building *destinationIndoors,
    const Ogre::Vector3 &clickPosition,
    bool addDontClear
) = nullptr;

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
        if (kDebugLogEnabled)
        {
            std::ostringstream logMessage;
            logMessage << "addJobSelectedCharacters: deferring splint rigging task"
                       << " shift=" << (shift ? "true" : "false") << " add=" << (add ? "true" : "false")
                       << " location=(" << location.x << "," << location.y << "," << location.z << ")";
            DebugLog(logMessage.str());
        }

        InitPendingSplintJob(thisptr, location);
    }
    else if (task == JOB_REPAIR_ROBOT)
    {
        if (gPendingSplintJob.active)
        {
            if (kDebugLogEnabled)
            {
                DebugLog(
                    "addJobSelectedCharacters: JOB_REPAIR_ROBOT added, refreshing "
                    "pending splint task"
                );
            }
            gPendingSplintJob.framesRemaining = 2;
        }
        else
        {
            if (kDebugLogEnabled)
            {
                DebugLog(
                    "addJobSelectedCharacters: JOB_REPAIR_ROBOT added, no pending "
                    "injection. Adding pending splint task."
                );
            }
            InitPendingSplintJob(thisptr, location);
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
        if (kDebugLogEnabled)
        {
            std::ostringstream logMessage;
            logMessage << "mainLoop: pending splint task framesRemaining=" << gPendingSplintJob.framesRemaining;
            DebugLog(logMessage.str());
        }
        if (gPendingSplintJob.framesRemaining <= 0)
        {
            DebugLog("[Info]: Injecting deferred splint rigging task for shift+medic button click");

            const auto &pendingLocation = reinterpret_cast<const Ogre::Vector3 &>(gPendingSplintJob.locationX);
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

    newPlayerTaskSelectedCharacters_orig = reinterpret_cast<decltype(newPlayerTaskSelectedCharacters_orig)>(
        KenshiLib::GetRealAddress(&PlayerInterface::newPlayerTaskSelectedCharacters)
    );

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