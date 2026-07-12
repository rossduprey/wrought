#include "WroughtSimSubsystem.h"

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

// The #28 seam smoke-test, driven HEADLESS. No editor window, no mouse: this opens a
// game world, grabs the live UWroughtSimSubsystem, feeds it synthetic world
// coordinates, and asserts the round-trip -- the same six findings the host-free
// core/ check proves, now against the actual Unreal subsystem. Runs under:
//
//   UnrealEditor-Cmd <Project>.uproject \
//     -ExecCmds="Automation RunTests Wrought.Seam" -unattended -nop4 -nullrhi -stdout
//
// -nullrhi means it needs no GPU, so it runs on a headless box or VM. If this passes
// unattended, the portable-field thesis is proven end to end: sim -> subsystem -> panel.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWroughtSeamTest,
    "Wrought.Seam",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

namespace
{
    // Pull one phase's line off the rendered panel. Returns false if the phase never
    // came up (which is itself an assertable fact -- e.g. no chalcocite by hand).
    bool FindPhase(const FWroughtBite& B, const TCHAR* Name, float& OutGrade, float& OutLocked)
    {
        for (const FWroughtPhaseReadout& R : B.Phases)
        {
            if (R.Phase == Name)
            {
                OutGrade  = R.Grade;
                OutLocked = R.LockedMassKg;
                return true;
            }
        }
        OutGrade = 0.f; OutLocked = 0.f;
        return false;
    }
}

bool FWroughtSeamTest::RunTest(const FString& /*Parameters*/)
{
    // A throwaway game world so WorldSubsystems get created. -nullrhi safe.
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld*/ false);
    if (!TestNotNull(TEXT("game world created"), World))
        return false;

    FWorldContext& Ctx = GEngine->CreateNewWorldContext(EWorldType::Game);
    Ctx.SetCurrentWorld(World);

    UWroughtSimSubsystem* Sim = World->GetSubsystem<UWroughtSimSubsystem>();
    TestNotNull(TEXT("WroughtSimSubsystem present on the world"), Sim);

    if (Sim)
    {
        // Register the frame: Place(0,0) at world origin, Unreal cm per sim meter.
        Sim->PlaneOrigin   = FVector::ZeroVector;
        Sim->UnitsPerMeter = 100.f;

        // DEPOSITS[]: copper-hill at Place(0,0), tin-creek at Place(300,120). World cm
        // = Place meters * UnitsPerMeter, z ignored (depth is a tier).
        const FVector Hill  (0.f,      0.f,     0.f);
        const FVector Creek (300.f*100.f, 120.f*100.f, 0.f);

        // The tier enum is SURFACE=0, MIDDLE=1, DEEP=2 (geology.h). Named here so the
        // test reads like the finding, not like magic integers.
        const int32 SURFACE_T = 0, DEEP_T = 2;

        float grade = 0.f, locked = 0.f;

        // --- Bare hand at the copper hill ---
        Sim->EquipBareHand();

        // 1 + 4. The weathered cap is won by hand, and it is an OXIDE (cuprite).
        Sim->SelectedTier = SURFACE_T;
        FWroughtBite capBite = Sim->BiteAt(Hill);
        TestTrue(TEXT("bare hand wins the weathered cap"), capBite.bWonRock);
        TestTrue(TEXT("cap reads as cuprite (oxide)"),
                 FindPhase(capBite, TEXT("cuprite"), grade, locked) && grade > 0.f);

        // 2. The fresh sulfide root skips off the bare hand -- nothing won. This empty
        //    bite IS the pick gate.
        Sim->SelectedTier = DEEP_T;
        FWroughtBite rootByHand = Sim->BiteAt(Hill);
        TestFalse(TEXT("bare hand skips off the deep rock"), rootByHand.bWonRock);

        // --- Sapling pick at the copper hill ---
        Sim->EquipSaplingPick();

        // 3 + a locked check. The pick wins the deep tier; it is a SULFIDE (chalcocite),
        //    and it comes up LOCKED -- breaker-bound, not pan-ready.
        Sim->SelectedTier = DEEP_T;
        FWroughtBite rootByPick = Sim->BiteAt(Hill);
        TestTrue(TEXT("sapling pick wins the deep root"), rootByPick.bWonRock);
        const bool hasChalcocite = FindPhase(rootByPick, TEXT("chalcocite"), grade, locked);
        TestTrue(TEXT("deep root reads as chalcocite (sulfide)"), hasChalcocite && grade > 0.f);
        TestTrue(TEXT("hard-rock sulfide comes up LOCKED (breaker-bound)"),
                 hasChalcocite && locked > 0.f);

        // --- The placer needs neither pick nor breaker ---
        // 5. Bare hand takes the tin creek at depth; cassiterite, and FREE (no locked mass).
        Sim->EquipBareHand();
        Sim->SelectedTier = DEEP_T;
        FWroughtBite creekBite = Sim->BiteAt(Creek);
        TestTrue(TEXT("bare hand takes the placer whole (no pick needed)"), creekBite.bWonRock);
        const bool hasCassiterite = FindPhase(creekBite, TEXT("cassiterite"), grade, locked);
        TestTrue(TEXT("placer reads as cassiterite"), hasCassiterite && grade > 0.f);
        TestTrue(TEXT("placer tin comes up FREE (no locked mass, no breaker)"),
                 hasCassiterite && locked <= 0.f);
    }

    // Tear the world down.
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(/*bInformEngineOfWorld*/ false);

    return true;
}
